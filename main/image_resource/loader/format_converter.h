#pragma once

#include <stdint.h>
#include <stddef.h>

namespace ImageResource {

// 二进制图片格式常量
#define BINARY_IMAGE_MAGIC UINT32_C(0x42494D47)  // "BIMG"
#define BINARY_IMAGE_VERSION UINT32_C(1)

// 二进制图片文件头
struct BinaryImageHeader {
    uint32_t magic;        // 魔数 0x42494D47
    uint32_t version;      // 版本号
    uint32_t width;        // 图片宽度
    uint32_t height;       // 图片高度
    uint32_t data_size;    // 数据大小（字节）
    uint32_t reserved[3];  // 保留字段
};

/**
 * 格式转换器
 * 负责.h文件转二进制格式
 */
class FormatConverter {
public:
    FormatConverter() = default;
    ~FormatConverter() = default;

    // 禁用拷贝
    FormatConverter(const FormatConverter&) = delete;
    FormatConverter& operator=(const FormatConverter&) = delete;

    /**
     * 转换.h文件为二进制格式
     * @param h_filepath .h文件路径
     * @param bin_filepath 输出二进制文件路径
     * @return true成功，false失败
     */
    bool ConvertHFileToBinary(const char* h_filepath, const char* bin_filepath);

private:
    /**
     * 解析十六进制数组
     * @param text 文本内容
     * @param text_size 文本大小
     * @param out_size 输出数据大小
     * @return 数据指针（需要调用者释放），失败返回nullptr
     */
    uint8_t* ParseHexArray(const char* text, size_t text_size, int& out_size);
};

} // namespace ImageResource
