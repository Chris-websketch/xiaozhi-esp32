#include "image_manager.h"
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/stat.h>

#include "image_resource/storage/spiffs_manager.h"
#include "image_resource/storage/cache_manager.h"
#include "image_resource/network/downloader.h"
#include "image_resource/network/version_checker.h"
#include "image_resource/loader/image_loader.h"
#include "image_resource/loader/packed_loader.h"
#include "image_resource/preload/preload_manager.h"
#include "image_resource/utils/download_mode.h"
#include "image_resource/utils/cleanup_helper.h"
#include "config/resource_config.h"
#include "assets/lang_config.h"

#define TAG "ImageResManager"

// 路径常量
#define IMAGE_URL_CACHE_FILE "/resources/image_urls.json"
#define LOGO_URL_CACHE_FILE "/resources/logo_url.json"
#define EMOTICON_URL_CACHE_FILE "/resources/emoticon_urls.txt"
#define IMAGE_BASE_PATH "/resources/images/"
#define LOGO_FILE_PATH "/resources/images/logo.bin"
#define LOGO_FILE_PATH_H "/resources/images/logo.h"
#define EMOTICON_BASE_PATH "/resources/emoticons/"
#define PACKED_FILE_PATH "/resources/images/packed.rgb"
#define MAX_IMAGE_FILES 9
#define MAX_EMOTICON_FILES 6

// 表情包文件名映射
static const char* EMOTICON_FILENAMES[6] = {
    "happy.bin",
    "sad.bin",
    "angry.bin",
    "surprised.bin",
    "calm.bin",
    "shy.bin"
};

using namespace ImageResource;

ImageResourceManager::ImageResourceManager() 
    : config_(nullptr),
      initialized_(false),
      has_valid_images_(false),
      has_valid_logo_(false),
      has_valid_emoticons_(false),
      pending_animations_download_(false),
      pending_logo_download_(false),
      pending_emoticons_download_(false),
      animations_download_completed_(false),
      logo_download_completed_(false),
      emoticons_download_completed_(false),
      logo_data_(nullptr),
      progress_callback_(nullptr),
      preload_progress_callback_(nullptr) {
    
    // 初始化配置
    config_ = &ConfigManager::GetInstance().get_config();
    
    // 创建模块实例
    spiffs_mgr_ = new SpiffsManager();
    cache_mgr_ = new CacheManager();
    downloader_ = new Downloader(config_);
    version_checker_ = new VersionChecker();
    image_loader_ = new ImageLoader();
    packed_loader_ = new PackedLoader(config_, spiffs_mgr_);  // 传递SpiffsManager用于GC
    preload_mgr_ = new PreloadManager(config_);
    download_mode_ = new DownloadMode();
    cleanup_helper_ = new CleanupHelper();
}

ImageResourceManager::~ImageResourceManager() {
    // 释放图片数据
    for (auto ptr : image_data_pointers_) {
        if (ptr) free(ptr);
    }
    if (logo_data_) free(logo_data_);
    
    // 删除模块实例
    delete cleanup_helper_;
    delete download_mode_;
    delete preload_mgr_;
    delete packed_loader_;
    delete image_loader_;
    delete version_checker_;
    delete downloader_;
    delete cache_mgr_;
    delete spiffs_mgr_;
}

esp_err_t ImageResourceManager::Initialize() {
    ESP_LOGI(TAG, "初始化图片资源管理器...");
    
    // 挂载分区
    esp_err_t err = spiffs_mgr_->Mount("resources", "/resources");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "无法挂载resources分区");
        return err;
    }
    
    // 确保目录存在
    spiffs_mgr_->CreateDirectory(IMAGE_BASE_PATH);
    
    // 读取缓存的URL
    cached_dynamic_urls_ = cache_mgr_->ReadDynamicUrls(IMAGE_URL_CACHE_FILE);
    cached_static_url_ = cache_mgr_->ReadStaticUrl(LOGO_URL_CACHE_FILE);
    cached_emoticon_urls_ = cache_mgr_->ReadDynamicUrls(EMOTICON_URL_CACHE_FILE);
    
    ESP_LOGI(TAG, "当前本地动画图片URL数量: %d", cached_dynamic_urls_.size());
    ESP_LOGI(TAG, "当前本地logo URL: %s", cached_static_url_.c_str());
    ESP_LOGI(TAG, "当前本地表情包URL数量: %d", cached_emoticon_urls_.size());
    
    // 检查资源
    has_valid_images_ = CheckImagesExist();
    has_valid_logo_ = CheckLogoExists();
    has_valid_emoticons_ = CheckEmoticonsExist();
    
    if (has_valid_emoticons_) {
        ESP_LOGI(TAG, "表情包文件已存在");
    } else {
        ESP_LOGW(TAG, "表情包文件不存在或不完整");
    }
    
    if (has_valid_images_ || has_valid_logo_) {
        LoadImageData();
    }
    
    initialized_ = true;
    return ESP_OK;
}

bool ImageResourceManager::CheckImagesExist() {
    int local_file_count = 0;
    for (int i = 1; i <= MAX_IMAGE_FILES; i++) {
        char filename[128];
        snprintf(filename, sizeof(filename), "%soutput_%04d.bin", IMAGE_BASE_PATH, i);
        
        FILE* f = fopen(filename, "r");
        if (f != NULL) {
            fclose(f);
            local_file_count++;
        } else {
            break;
        }
    }
    
    ESP_LOGI(TAG, "本地动画图片文件数量: %d，期望: %d", local_file_count, MAX_IMAGE_FILES);
    
    if (local_file_count < MAX_IMAGE_FILES) {
        ESP_LOGW(TAG, "本地动画图片数量不足");
        return false;
    }
    
    return true;
}

bool ImageResourceManager::CheckLogoExists() {
    struct stat st;
    if (stat(LOGO_FILE_PATH, &st) == 0) {
        ESP_LOGI(TAG, "找到二进制logo文件");
        return true;
    }
    if (stat(LOGO_FILE_PATH_H, &st) == 0) {
        ESP_LOGI(TAG, "找到.h格式logo文件");
        return true;
    }
    return false;
}

bool ImageResourceManager::CheckEmoticonsExist() {
    for (int i = 0; i < MAX_EMOTICON_FILES; i++) {
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%s%s", 
                EMOTICON_BASE_PATH, EMOTICON_FILENAMES[i]);
        
        struct stat st;
        if (stat(filepath, &st) != 0 || st.st_size != 115200) {
            return false;
        }
    }
    return true;
}

void ImageResourceManager::LoadImageData() {
    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "加载图片前可用内存: %u字节", (unsigned int)free_heap);
    
    if (free_heap < 150000) {
        ESP_LOGW(TAG, "内存不足，跳过图片加载");
        return;
    }
    
    // 清空原有数据
    for (auto ptr : image_data_pointers_) {
        if (ptr) free(ptr);
    }
    image_data_pointers_.clear();
    image_array_.clear();
    
    if (logo_data_) {
        free(logo_data_);
        logo_data_ = nullptr;
    }
    
    // 加载logo
    if (has_valid_logo_) {
        LoadLogoFile();
    }
    
    // 尝试从打包文件加载
    if (has_valid_images_) {
        std::vector<uint8_t*> buffers;
        if (packed_loader_->LoadPacked(PACKED_FILE_PATH, 240 * 240 * 2, MAX_IMAGE_FILES, buffers)) {
            ESP_LOGI(TAG, "已从打包文件快速加载全部动画图片");
            image_data_pointers_ = buffers;
            image_array_.clear();
            for (auto buf : buffers) {
                image_array_.push_back(buf);
            }
            return;
        }
        
        // 回退：快速启动，延迟加载
        int actual_count = std::min((int)cached_dynamic_urls_.size(), MAX_IMAGE_FILES);
        if (actual_count == 0) {
            // 扫描本地文件数量
            for (int i = 1; i <= MAX_IMAGE_FILES; i++) {
                char filename[128];
                snprintf(filename, sizeof(filename), "%soutput_%04d.bin", IMAGE_BASE_PATH, i);
                FILE* f = fopen(filename, "rb");
                if (f != NULL) {
                    fclose(f);
                    actual_count++;
                } else {
                    break;
                }
            }
        }
        
        if (actual_count < MAX_IMAGE_FILES) {
            ESP_LOGW(TAG, "本地图片数量不足，标记为无效");
            has_valid_images_ = false;
            return;
        }
        
        // 预分配空间并立即加载前两张
        image_array_.resize(actual_count);
        image_data_pointers_.resize(actual_count, nullptr);
        
        if (actual_count > 0) {
            ESP_LOGI(TAG, "立即加载前两张关键图片");
            LoadImageFile(1);
            if (actual_count > 1) {
                LoadImageFile(2);
            }
        }
    }
}

bool ImageResourceManager::LoadImageFile(int image_index) {
    char filename[128];
    snprintf(filename, sizeof(filename), "%soutput_%04d.bin", IMAGE_BASE_PATH, image_index);
    
    size_t out_size = 0;
    uint8_t* data = image_loader_->LoadImage(filename, out_size);
    
    if (data && out_size > 0) {
        int array_index = image_index - 1;
        if (array_index >= 0 && array_index < image_array_.size()) {
            if (image_data_pointers_[array_index]) {
                free(image_data_pointers_[array_index]);
            }
            image_data_pointers_[array_index] = data;
            image_array_[array_index] = data;
            ESP_LOGI(TAG, "成功加载图片 %d", image_index);
            return true;
        } else {
            free(data);
        }
    }
    
    return false;
}

bool ImageResourceManager::LoadLogoFile() {
    size_t out_size = 0;
    
    // 尝试加载二进制格式
    uint8_t* data = image_loader_->LoadImage(LOGO_FILE_PATH, out_size);
    if (!data) {
        // 尝试.h格式
        data = image_loader_->LoadImage(LOGO_FILE_PATH_H, out_size);
    }
    
    if (data && out_size > 0) {
        logo_data_ = data;
        ESP_LOGI(TAG, "成功加载logo: %zu字节", out_size);
        return true;
    }
    
    return false;
}

esp_err_t ImageResourceManager::CheckAndUpdateAllResources(const char* api_url, const char* version_url) {
    ESP_LOGI(TAG, "开始检查并更新所有资源...");
    
    bool need_update_animations = !has_valid_images_;
    bool need_update_logo = !has_valid_logo_;
    bool need_update_emoticons = !has_valid_emoticons_;
    
    // 检查服务器版本
    VersionChecker::ResourceVersions server_versions;
    esp_err_t check_result = version_checker_->CheckServer(version_url, server_versions);
    
    if (check_result == ESP_OK) {
        VersionChecker::ResourceVersions local_versions;
        local_versions.dynamic_urls = cached_dynamic_urls_;
        local_versions.static_url = cached_static_url_;
        local_versions.emoticon_urls = cached_emoticon_urls_;
        
        bool need_dyn = false, need_sta = false;
        version_checker_->NeedsUpdate(server_versions, local_versions, need_dyn, need_sta);
        
        need_update_animations = need_update_animations || need_dyn;
        need_update_logo = need_update_logo || need_sta;
        
        // 检查表情包版本
        if (!server_versions.emoticon_urls.empty()) {
            if (cached_emoticon_urls_ != server_versions.emoticon_urls) {
                need_update_emoticons = true;
                ESP_LOGI(TAG, "表情包需要更新");
            }
            server_emoticon_urls_ = server_versions.emoticon_urls;
        } else {
            // 服务器未返回表情包URL，使用硬编码的默认URL
            ESP_LOGW(TAG, "服务器未返回表情包URL，使用默认地址");
            server_emoticon_urls_ = {
                "https://imgbad.xmduzhong.com/i/2025/10/27/h50yza_0001.bin",   // happy
                "https://imgbad.xmduzhong.com/i/2025/10/27/h5ghmi_0001.bin",   // sad
                "https://imgbad.xmduzhong.com/i/2025/10/27/h4cm5f_0001.bin",   // angry
                "https://imgbad.xmduzhong.com/i/2025/10/27/h5qw39_0001.bin",   // surprised
                "https://imgbad.xmduzhong.com/i/2025/10/27/h4uokk_0001.bin",   // calm
                "https://imgbad.xmduzhong.com/i/2025/10/27/h5lop1_0001.bin"    // shy
            };
            // 检查是否需要下载
            if (cached_emoticon_urls_ != server_emoticon_urls_) {
                need_update_emoticons = true;
                ESP_LOGI(TAG, "使用默认表情包URL，需要下载");
            }
        }
        
        server_dynamic_urls_ = server_versions.dynamic_urls;
        server_static_url_ = server_versions.static_url;
    }
    
    // 如果都不需要更新，检查打包文件
    if (!need_update_animations && !need_update_logo && !need_update_emoticons) {
        ESP_LOGI(TAG, "所有资源都是最新版本");
        
        struct stat st;
        const size_t expected_size = 240 * 240 * 2 * MAX_IMAGE_FILES;
        if (stat(PACKED_FILE_PATH, &st) != 0 || (size_t)st.st_size != expected_size) {
            ESP_LOGI(TAG, "打包文件不存在或尺寸不匹配，开始构建");
            return BuildAndRestart() ? ESP_OK : ESP_FAIL;
        }
        
        return ESP_ERR_NOT_FOUND;
    }
    
    // 设置下载任务标志
    pending_animations_download_ = need_update_animations;
    pending_logo_download_ = need_update_logo;
    pending_emoticons_download_ = need_update_emoticons;
    animations_download_completed_ = false;
    logo_download_completed_ = false;
    emoticons_download_completed_ = false;
    
    // 下载资源
    if (need_update_animations) {
        DownloadImages();
    }
    if (need_update_logo) {
        DownloadLogo();
    }
    if (need_update_emoticons) {
        DownloadEmoticons();
    }
    
    // 检查是否所有下载都完成，统一执行打包
    bool all_completed = true;
    if (pending_animations_download_ && !animations_download_completed_) {
        all_completed = false;
    }
    if (pending_logo_download_ && !logo_download_completed_) {
        all_completed = false;
    }
    if (pending_emoticons_download_ && !emoticons_download_completed_) {
        all_completed = false;
    }
    
    if (all_completed && (pending_animations_download_ || pending_logo_download_)) {
        ESP_LOGI(TAG, "所有资源下载完成，开始执行打包");
        BuildAndRestart();
    }
    
    return ESP_OK;
}

esp_err_t ImageResourceManager::DownloadImages() {
    if (server_dynamic_urls_.empty()) {
        ESP_LOGE(TAG, "没有可下载的动画图片URL");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "开始下载动画图片...");
    
    download_mode_->Enter();
    
    // 删除旧文件（会显示"正在删除旧的动图文件"）
    cleanup_helper_->DeleteAnimationFiles(IMAGE_BASE_PATH, MAX_IMAGE_FILES, progress_callback_);
    
    // 删除完成后，显示下载消息
    if (progress_callback_) {
        progress_callback_(0, 100, Lang::Strings::DOWNLOADING_ANIMATIONS);
    }
    
    // 准备文件路径
    std::vector<std::string> filepaths;
    for (size_t i = 0; i < server_dynamic_urls_.size() && i < MAX_IMAGE_FILES; i++) {
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%soutput_%04d.bin", IMAGE_BASE_PATH, (int)(i + 1));
        filepaths.push_back(filepath);
    }
    
    // 下载
    downloader_->SetProgressCallback(progress_callback_);
    esp_err_t result = downloader_->DownloadBatch(server_dynamic_urls_, filepaths);
    
    if (result == ESP_OK) {
        cache_mgr_->SaveDynamicUrls(server_dynamic_urls_, IMAGE_URL_CACHE_FILE);
        cached_dynamic_urls_ = server_dynamic_urls_;
        has_valid_images_ = true;
        animations_download_completed_ = true;
        
        LoadImageData();
        ESP_LOGI(TAG, "动画图片下载完成，等待其他资源...");
        
        // 显示完成消息
        if (progress_callback_) {
            progress_callback_(100, 100, Lang::Strings::DOWNLOAD_COMPLETE);
            vTaskDelay(pdMS_TO_TICKS(1000)); // 显示1秒
        }
    }
    
    download_mode_->Exit();
    return result;
}

esp_err_t ImageResourceManager::DownloadLogo() {
    if (server_static_url_.empty()) {
        ESP_LOGE(TAG, "没有可下载的logo URL");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "开始下载logo...");
    
    download_mode_->Enter();
    
    // 删除旧文件（会显示"正在删除旧的静图文件"）
    cleanup_helper_->DeleteLogoFile(LOGO_FILE_PATH, LOGO_FILE_PATH_H, progress_callback_);
    
    // 删除完成后，显示下载消息
    if (progress_callback_) {
        progress_callback_(0, 100, Lang::Strings::DOWNLOADING_STATIC_IMAGE);
    }
    
    downloader_->SetProgressCallback(progress_callback_);
    esp_err_t result = downloader_->DownloadFile(server_static_url_.c_str(), LOGO_FILE_PATH);
    
    if (result == ESP_OK) {
        cache_mgr_->SaveStaticUrl(server_static_url_, LOGO_URL_CACHE_FILE);
        cached_static_url_ = server_static_url_;
        has_valid_logo_ = true;
        logo_download_completed_ = true;
        
        LoadLogoFile();
        ESP_LOGI(TAG, "logo下载完成");
        
        // 显示完成消息
        if (progress_callback_) {
            progress_callback_(100, 100, Lang::Strings::DOWNLOAD_COMPLETE);
            vTaskDelay(pdMS_TO_TICKS(1000)); // 显示1秒
        }
    }
    
    download_mode_->Exit();
    return result;
}

esp_err_t ImageResourceManager::DownloadEmoticons() {
    if (server_emoticon_urls_.size() != MAX_EMOTICON_FILES) {
        ESP_LOGE(TAG, "表情包URL数量不正确: %zu (期望: %d)", 
                server_emoticon_urls_.size(), MAX_EMOTICON_FILES);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "开始下载表情包...");
    
    if (progress_callback_) {
        progress_callback_(0, 100, Lang::Strings::DOWNLOADING_EMOTICONS);
    }
    
    download_mode_->Enter();
    
    // 删除旧表情包文件（会显示"正在删除旧的表情包文件"）
    cleanup_helper_->DeleteEmoticonFiles(EMOTICON_BASE_PATH, EMOTICON_FILENAMES, MAX_EMOTICON_FILES, progress_callback_);
    
    // 删除完成后，显示下载消息
    if (progress_callback_) {
        progress_callback_(0, 100, Lang::Strings::DOWNLOADING_EMOTICONS);
    }
    
    // 确保目录存在
    mkdir("/resources/emoticons", 0755);
    
    // 准备文件路径
    std::vector<std::string> filepaths;
    for (int i = 0; i < MAX_EMOTICON_FILES; i++) {
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%s%s", 
                EMOTICON_BASE_PATH, EMOTICON_FILENAMES[i]);
        filepaths.push_back(filepath);
    }
    
    // 下载
    downloader_->SetProgressCallback(progress_callback_);
    esp_err_t result = downloader_->DownloadBatch(server_emoticon_urls_, filepaths);
    
    if (result == ESP_OK) {
        cache_mgr_->SaveDynamicUrls(server_emoticon_urls_, EMOTICON_URL_CACHE_FILE);
        cached_emoticon_urls_ = server_emoticon_urls_;
        has_valid_emoticons_ = true;
        emoticons_download_completed_ = true;
        
        ESP_LOGI(TAG, "表情包下载完成");
        
        // 显示完成消息
        if (progress_callback_) {
            progress_callback_(100, 100, Lang::Strings::DOWNLOAD_COMPLETE);
            vTaskDelay(pdMS_TO_TICKS(1000)); // 显示1秒
        }
    }
    
    download_mode_->Exit();
    return result;
}

bool ImageResourceManager::BuildAndRestart() {
    ESP_LOGI(TAG, "构建打包文件...");
    
    std::vector<std::string> source_files;
    for (int i = 1; i <= MAX_IMAGE_FILES; i++) {
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%soutput_%04d.bin", IMAGE_BASE_PATH, i);
        source_files.push_back(filepath);
    }
    
    bool success = packed_loader_->BuildPacked(source_files, PACKED_FILE_PATH, 
                                              240 * 240 * 2, progress_callback_);
    
    if (success) {
        ESP_LOGI(TAG, "打包完成，系统即将重启");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    }
    
    return success;
}

// 其他接口实现
esp_err_t ImageResourceManager::CheckAndUpdateResources(const char* api_url, const char* version_url) {
    return CheckAndUpdateAllResources(api_url, version_url);
}

esp_err_t ImageResourceManager::CheckAndUpdateLogo(const char* api_url, const char* logo_version_url) {
    return CheckAndUpdateAllResources(api_url, logo_version_url);
}

const std::vector<const uint8_t*>& ImageResourceManager::GetImageArray() const {
    return image_array_;
}

const uint8_t* ImageResourceManager::GetLogoImage() const {
    return logo_data_;
}

bool ImageResourceManager::LoadImageOnDemand(int image_index) {
    if (image_index < 1 || image_index > (int)image_array_.size()) {
        return false;
    }
    
    int array_index = image_index - 1;
    if (image_array_[array_index] != nullptr) {
        return true;
    }
    
    return LoadImageFile(image_index);
}

bool ImageResourceManager::IsImageLoaded(int image_index) const {
    if (image_index < 1 || image_index > (int)image_array_.size()) {
        return false;
    }
    return image_array_[image_index - 1] != nullptr;
}

esp_err_t ImageResourceManager::PreloadRemainingImages() {
    auto load_cb = [this](int index) { return LoadImageFile(index); };
    auto check_cb = [this](int index) { return IsImageLoaded(index); };
    return preload_mgr_->PreloadRemaining(load_cb, check_cb, image_array_.size(), 
                                         preload_progress_callback_);
}

esp_err_t ImageResourceManager::PreloadRemainingImagesSilent(unsigned long time_budget_ms) {
    auto load_cb = [this](int index) { return LoadImageFile(index); };
    auto check_cb = [this](int index) { return IsImageLoaded(index); };
    return preload_mgr_->PreloadSilent(load_cb, check_cb, image_array_.size(), time_budget_ms);
}

void ImageResourceManager::CancelPreload() {
    preload_mgr_->Cancel();
}

bool ImageResourceManager::IsPreloading() const {
    return preload_mgr_->IsPreloading();
}

bool ImageResourceManager::WaitForPreloadToFinish(unsigned long timeout_ms) {
    return preload_mgr_->WaitForFinish(timeout_ms);
}

void ImageResourceManager::SetDownloadProgressCallback(ProgressCallback callback) {
    progress_callback_ = callback;
}

void ImageResourceManager::SetPreloadProgressCallback(ProgressCallback callback) {
    preload_progress_callback_ = callback;
}

bool ImageResourceManager::ClearAllImageFiles() {
    bool success = cleanup_helper_->ClearAllImages(IMAGE_BASE_PATH, MAX_IMAGE_FILES);
    
    // 清理缓存
    remove(IMAGE_URL_CACHE_FILE);
    remove(LOGO_URL_CACHE_FILE);
    remove(PACKED_FILE_PATH);
    
    // 重置状态
    has_valid_images_ = false;
    has_valid_logo_ = false;
    cached_dynamic_urls_.clear();
    cached_static_url_.clear();
    
    for (auto ptr : image_data_pointers_) {
        if (ptr) free(ptr);
    }
    image_data_pointers_.clear();
    image_array_.clear();
    
    if (logo_data_) {
        free(logo_data_);
        logo_data_ = nullptr;
    }
    
    return success;
}
