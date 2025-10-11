#include "image_loader.h"
#include <esp_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define TAG "ImageLoader"

namespace ImageResource {

uint8_t* ImageLoader::LoadImage(const char* filepath, size_t& out_size) {
    out_size = 0;
    
    // 检查文件是否存在
    struct stat st;
    if (stat(filepath, &st) != 0) {
        ESP_LOGE(TAG, "文件不存在: %s", filepath);
        return nullptr;
    }
    
    size_t file_size = st.st_size;
    
    // 检查文件扩展名
    const char* ext = strrchr(filepath, '.');
    if (ext) {
        if (strcmp(ext, ".bin") == 0) {
            // 二进制格式或原始RGB数据
            const size_t rgb565_size = 240 * 240 * 2;
            if (file_size == rgb565_size) {
                ESP_LOGI(TAG, "检测到标准RGB565格式");
                return LoadRawImage(filepath, rgb565_size, out_size);
            } else {
                ESP_LOGI(TAG, "尝试作为二进制格式加载");
                return LoadBinaryImage(filepath, out_size);
            }
        } else if (strcmp(ext, ".h") == 0) {
            ESP_LOGI(TAG, "检测到.h格式");
            return LoadHFormatImage(filepath, out_size);
        }
    }
    
    // 默认尝试作为原始数据
    return LoadRawImage(filepath, 0, out_size);
}

uint8_t* ImageLoader::LoadBinaryImage(const char* filepath, size_t& out_size) {
    out_size = 0;
    
    FILE* f = fopen(filepath, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法打开文件: %s", filepath);
        return nullptr;
    }
    
    // 设置64KB缓冲区
    setvbuf(f, NULL, _IOFBF, 65536);
    
    // 读取文件头
    BinaryImageHeader header;
    if (fread(&header, sizeof(BinaryImageHeader), 1, f) != 1) {
        ESP_LOGE(TAG, "读取文件头失败");
        fclose(f);
        return nullptr;
    }
    
    // 验证魔数
    if (header.magic != BINARY_IMAGE_MAGIC) {
        ESP_LOGW(TAG, "文件魔数无效: 0x%08X，尝试作为原始数据", (unsigned int)header.magic);
        fclose(f);
        
        // 回退到原始数据加载
        return LoadRawImage(filepath, 0, out_size);
    }
    
    // 验证版本
    if (header.version != BINARY_IMAGE_VERSION) {
        ESP_LOGE(TAG, "文件版本不支持: %u", (unsigned int)header.version);
        fclose(f);
        return nullptr;
    }
    
    // 验证数据大小
    if (header.data_size == 0 || header.data_size > 200000) {
        ESP_LOGE(TAG, "数据大小无效: %u", (unsigned int)header.data_size);
        fclose(f);
        return nullptr;
    }
    
    // 分配内存
    uint8_t* img_buffer = (uint8_t*)malloc(header.data_size);
    if (!img_buffer) {
        ESP_LOGE(TAG, "内存分配失败: %u字节", (unsigned int)header.data_size);
        fclose(f);
        return nullptr;
    }
    
    // 读取图像数据
    size_t read_size = fread(img_buffer, 1, header.data_size, f);
    fclose(f);
    
    if (read_size != header.data_size) {
        ESP_LOGE(TAG, "读取数据不完整: %zu/%u字节", read_size, (unsigned int)header.data_size);
        free(img_buffer);
        return nullptr;
    }
    
    out_size = header.data_size;
    ESP_LOGI(TAG, "成功加载二进制图片: %zu字节 (尺寸: %ux%u)", 
            out_size, (unsigned int)header.width, (unsigned int)header.height);
    
    return img_buffer;
}

uint8_t* ImageLoader::LoadRawImage(const char* filepath, size_t expected_size, size_t& out_size) {
    out_size = 0;
    
    FILE* f = fopen(filepath, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法打开原始数据文件: %s", filepath);
        return nullptr;
    }
    
    // 设置64KB缓冲区
    setvbuf(f, NULL, _IOFBF, 65536);
    
    // 获取文件大小
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size <= 0) {
        ESP_LOGE(TAG, "文件大小无效: %ld", file_size);
        fclose(f);
        return nullptr;
    }
    
    // 如果指定了期望大小，检查是否匹配
    if (expected_size > 0 && (size_t)file_size != expected_size) {
        ESP_LOGW(TAG, "文件大小 %ld 不符合期望 %zu", file_size, expected_size);
    }
    
    ESP_LOGI(TAG, "作为原始RGB数据加载: %ld字节", file_size);
    
    // 分配内存
    uint8_t* img_buffer = (uint8_t*)malloc(file_size);
    if (!img_buffer) {
        ESP_LOGE(TAG, "内存分配失败: %ld字节", file_size);
        fclose(f);
        return nullptr;
    }
    
    // 读取整个文件
    size_t read_size = fread(img_buffer, 1, file_size, f);
    fclose(f);
    
    if (read_size != (size_t)file_size) {
        ESP_LOGE(TAG, "读取失败: 期望%ld，实际%zu", file_size, read_size);
        free(img_buffer);
        return nullptr;
    }
    
    out_size = file_size;
    ESP_LOGI(TAG, "成功加载原始RGB数据: %zu字节", out_size);
    
    return img_buffer;
}

uint8_t* ImageLoader::LoadHFormatImage(const char* filepath, size_t& out_size) {
    out_size = 0;
    
    FILE* f = fopen(filepath, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法打开.h文件: %s", filepath);
        return nullptr;
    }
    
    // 获取文件大小
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // 分配文本缓冲区
    size_t aligned_size = (file_size + 15) & ~15;  // 16字节对齐
    char* text_buffer = (char*)malloc(aligned_size + 1);
    if (text_buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        fclose(f);
        return nullptr;
    }
    
    // 设置64KB缓冲区
    setvbuf(f, NULL, _IOFBF, 65536);
    size_t read_size = fread(text_buffer, 1, file_size, f);
    text_buffer[read_size] = '\0';
    fclose(f);
    
    int array_size = 0;
    uint8_t* img_buffer = ParseHexArray(text_buffer, read_size, array_size);
    free(text_buffer);
    
    if (!img_buffer || array_size <= 0) {
        ESP_LOGE(TAG, "解析.h文件失败");
        return nullptr;
    }
    
    out_size = array_size;
    ESP_LOGI(TAG, "成功加载.h格式图片: %zu字节", out_size);
    
    return img_buffer;
}

uint8_t* ImageLoader::ParseHexArray(const char* text, size_t text_size, int& out_size) {
    out_size = 0;
    
    // 查找数组声明
    const char* array_pattern = "const unsigned char";
    const char* array_start = strstr(text, array_pattern);
    if (!array_start) {
        ESP_LOGE(TAG, "未找到数组声明");
        return nullptr;
    }
    
    // 查找数组大小
    const char* size_start = strstr(array_start, "[");
    const char* size_end = strstr(size_start, "]");
    if (!size_start || !size_end) {
        ESP_LOGE(TAG, "未找到数组大小");
        return nullptr;
    }
    
    // 提取数组大小
    char size_str[32] = {0};
    strncpy(size_str, size_start + 1, size_end - size_start - 1);
    int array_size = atoi(size_str);
    
    if (array_size <= 0 || array_size > 200000) {
        ESP_LOGE(TAG, "数组大小无效: %d", array_size);
        return nullptr;
    }
    
    // 查找数据开始位置
    const char* data_start = strstr(size_end, "{");
    if (!data_start) {
        ESP_LOGE(TAG, "未找到数组数据");
        return nullptr;
    }
    data_start++;
    
    // 分配图像缓冲区
    uint8_t* img_buffer = (uint8_t*)malloc(array_size);
    if (!img_buffer) {
        ESP_LOGE(TAG, "内存分配失败");
        return nullptr;
    }
    
    // 十六进制查找表
    static const int hex_values[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
         0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
        -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    
    const char* p = data_start;
    const char* text_end = text + text_size;
    int index = 0;
    
    while (p < text_end - 5 && index < array_size) {
        const char* next_zero = (const char*)memchr(p, '0', text_end - p);
        if (!next_zero || next_zero >= text_end - 3) break;
        
        p = next_zero;
        
        if (*(p+1) == 'x' || *(p+1) == 'X') {
            p += 2;
            
            int high = hex_values[(unsigned char)*p];
            int low = hex_values[(unsigned char)*(p+1)];
            
            if ((high | low) >= 0) {
                unsigned char value = (high << 4) | low;
                
                // 字节序交换逻辑
                if (index & 1) {
                    if (index > 0) {
                        unsigned char temp = img_buffer[index-1];
                        img_buffer[index-1] = value;
                        img_buffer[index] = temp;
                    } else {
                        img_buffer[index] = value;
                    }
                } else {
                    img_buffer[index] = value;
                }
                
                index++;
                p += 2;
            } else {
                p++;
            }
        } else {
            p++;
        }
    }
    
    if (index < array_size) {
        ESP_LOGW(TAG, "解析的数据不完整: %d/%d字节", index, array_size);
    }
    
    out_size = index;
    return img_buffer;
}

} // namespace ImageResource
