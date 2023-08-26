#ifndef BASE_MD5_H
#define BASE_MD5_H

#include <base/common/noncopyable.h>

#include <string>
#include <fstream>

#ifdef USE_HALCYON_STRING_VIEW
#include <base/string/string_view.h>
#else
#include <string_view>
#endif

BASE_BEGIN_NAMESPACE

#ifndef USE_HALCYON_STRING_VIEW
using std::string_view;
#endif

/// MD5
class HALCYON_BASE_API MD5 final : noncopyable
{
public:
    MD5() noexcept;

    /**
     * @brief       构造函数
     * @param[in]   输入的字符串
     * @param[in]   字符串的长度
     */
    MD5(const void* input, size_t length);

    /**
     * @brief       构造函数
     * @param[in]   字符串
     */
    MD5(string_view str);

    /**
     * @brief       构造函数(计算文件的MD5)（待优化）
     * @param[in]   文件流操作
     * @ps          内部会关闭文件
     */
    MD5(std::ifstream& in);

    /**
     * @brief       计算字符串的MD5
     * @param[in]   字符串
     * @param[in]   字符串长度
     */
    void update(const void* input, size_t length);

    /**
     * @brief       计算字符串的MD5
     * @param[in]   字符串
     */
    void update(string_view str);

    /**
     * @brief       计算文件的MD5（待优化）
     * @param[in]   文件流操作
     * @ps          内部会关闭文件
     */
    void update(std::ifstream& in);

    /**
     * @brief       获取MD5结果
     * @return      MD5结果(byte)
     */
    const uint8_t* digest();

    /**
     * @brief       MD5转为字符串(十六进制形式显示)
     * @return      字符串
     */
    std::string toString();

    /**
     * @brief       重置
     */
    void reset();

private:
    /**
     * @brief       计算MD5
     * @param[in]   数据
     * @param[in]   数据长度
     */
    void update(const uint8_t* input, size_t length);

    /**
     * @brief       最后的计算步骤
     */
    void finish();

    /**
     * @brief       转换
     */
    void transform(const uint8_t block[64]);

    /**
     * @brief
     */
    void encode(const uint32_t* input, uint8_t* output, size_t length);

    /**
     * @brief
     */
    void decode(const uint8_t* input, uint32_t* output, size_t length);

    /**
     * @brief       byte数据转为十六进制字符串
     * @param[in]   byte数据
     * @param[in]   byte数据长度
     * @return      十六进制字符串
     */
    std::string byteToHexString(const uint8_t* input, size_t length);

private:
    uint32_t state_[4];  /* state (ABCD) */
    uint32_t count_[2];  /* number of bits, modulo 2^64 (low-order word first) */
    uint8_t buffer_[64];  /* input buffer */
    uint8_t digest_[16];  /* message digest */
    bool finished_;  /* calculate finished ? */

    static const uint8_t kPadding[64];  /* padding for calculate */
    static const char kHex[16];
    static const size_t kBufferSize = 1024;
};

BASE_END_NAMESPACE

#endif