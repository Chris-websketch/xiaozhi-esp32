#include "format_converter.h"
#include <esp_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "FormatConverter"

namespace ImageResource {

bool FormatConverter::ConvertHFileToBinary(const char* h_filepath, const char* bin_filepath) {
    ESP_LOGI(TAG, "开始转换: %s -> %s", h_filepath, bin_filepath);
    
    FILE* f = fopen(h_filepath, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法打开.h文件: %s", h_filepath);
        return false;
    }
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* text_buffer = (char*)malloc(file_size + 1);
    if (text_buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        fclose(f);
        return false;
    }
    
    size_t read_size = fread(text_buffer, 1, file_size, f);
    text_buffer[read_size] = '\0';
    fclose(f);
    
    int array_size = 0;
    uint8_t* img_buffer = ParseHexArray(text_buffer, read_size, array_size);
    free(text_buffer);
    
    if (!img_buffer || array_size <= 0) {
        ESP_LOGE(TAG, "解析.h文件失败");
        return false;
    }
    
    // 创建二进制文件头
    BinaryImageHeader header = {
        .magic = BINARY_IMAGE_MAGIC,
        .version = BINARY_IMAGE_VERSION,
        .width = 240,
        .height = 240,
        .data_size = (uint32_t)array_size,
        .reserved = {0, 0, 0}
    };
    
    FILE* bin_file = fopen(bin_filepath, "wb");
    if (bin_file == NULL) {
        ESP_LOGE(TAG, "无法创建二进制文件: %s", bin_filepath);
        free(img_buffer);
        return false;
    }
    
    if (fwrite(&header, sizeof(BinaryImageHeader), 1, bin_file) != 1) {
        ESP_LOGE(TAG, "写入文件头失败");
        fclose(bin_file);
        free(img_buffer);
        return false;
    }
    
    if (fwrite(img_buffer, 1, array_size, bin_file) != array_size) {
        ESP_LOGE(TAG, "写入图像数据失败");
        fclose(bin_file);
        free(img_buffer);
        return false;
    }
    
    fclose(bin_file);
    free(img_buffer);
    
    ESP_LOGI(TAG, "转换成功: %d 字节数据", array_size);
    return true;
}

uint8_t* FormatConverter::ParseHexArray(const char* text, size_t text_size, int& out_size) {
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
    
    uint8_t* img_buffer = (uint8_t*)malloc(array_size);
    if (!img_buffer) {
        ESP_LOGE(TAG, "图像数据内存分配失败");
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
                
                // 字节序交换
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
        ESP_LOGW(TAG, "解析的数据不完整: %d/%d 字节", index, array_size);
    }
    
    out_size = index;
    return img_buffer;
}

} // namespace ImageResource
