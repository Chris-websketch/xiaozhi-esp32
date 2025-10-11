#pragma once

#include <stdint.h>
#include <stddef.h>
#include "format_converter.h"

namespace ImageResource {

/**
 * 图片加载器
 * 负责从多种格式加载图片数据
 */
class ImageLoader {
public:
    ImageLoader() = default;
    ~ImageLoader() = default;

    // 禁用拷贝
    ImageLoader(const ImageLoader&) = delete;
    ImageLoader& operator=(const ImageLoader&) = delete;

    /**
     * 加载图片文件（自动检测格式）
     * @param filepath 文件路径
     * @param out_size 输出数据大小
     * @return 数据指针（需要调用者释放），失败返回nullptr
     */
    uint8_t* LoadImage(const char* filepath, size_t& out_size);

    /**
     * 加载二进制格式图片
     * @param filepath 文件路径
     * @param out_size 输出数据大小
     * @return 数据指针（需要调用者释放），失败返回nullptr
     */
    uint8_t* LoadBinaryImage(const char* filepath, size_t& out_size);

    /**
     * 加载原始RGB数据
     * @param filepath 文件路径
     * @param expected_size 期望的文件大小（0表示不检查）
     * @param out_size 输出数据大小
     * @return 数据指针（需要调用者释放），失败返回nullptr
     */
    uint8_t* LoadRawImage(const char* filepath, size_t expected_size, size_t& out_size);

    /**
     * 加载.h格式图片
     * @param filepath 文件路径
     * @param out_size 输出数据大小
     * @return 数据指针（需要调用者释放），失败返回nullptr
     */
    uint8_t* LoadHFormatImage(const char* filepath, size_t& out_size);

private:
    /**
     * 解析.h文件中的十六进制数组
     */
    uint8_t* ParseHexArray(const char* text, size_t text_size, int& out_size);
};

} // namespace ImageResource
