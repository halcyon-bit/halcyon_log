#include "compress_opt.h"

#ifdef USE_HALCYON_LZ4
#include <lz4/lz4.h>
#elif defined USE_HALCYON_ZSTD
#include <zstd/zstd.h>
#endif
#include <iostream>

using namespace halcyon;

#ifdef USE_HALCYON_LZ4

static bool lz4Compress(HALCYON_STRING_VIEW_NS string_view src, std::string& dst)
{
    int dst_size = LZ4_compressBound(static_cast<int>(src.size()));
    if (dst_size == 0) {
        return false;
    }
    dst.resize(dst_size);

    int com_size = LZ4_compress_fast(src.data(), const_cast<char*>(dst.data()), static_cast<int>(src.size()), dst_size, 1);
    if (com_size <= 0) {
        return false;
    }
    dst.resize(com_size);
    return true;
}

static bool lz4Decompress(HALCYON_STRING_VIEW_NS string_view src, std::string& dst)
{
    constexpr int kMaxDecompressedSize = 40960;
    dst.resize(kMaxDecompressedSize);
    int decom_size = LZ4_decompress_safe(src.data(), const_cast<char*>(dst.data()), static_cast<int>(src.size()), kMaxDecompressedSize);
    if (decom_size <= 0) {
        return false;
    }
    dst.resize(decom_size);
    return true;
}

#elif defined USE_HALCYON_ZSTD

static bool zstdCompress(HALCYON_STRING_VIEW_NS string_view src, std::string& dst)
{
    size_t dst_size = ZSTD_compressBound(src.size());
    if (1 == ZSTD_isError(dst_size)) {
        return false;
    }
    dst.resize(dst_size);

    size_t com_size = ZSTD_compress(const_cast<char*>(dst.data()), dst_size, src.data(), src.size(), ZSTD_fast);
    if (1 == ZSTD_isError(com_size)) {
        return false;
    }
    dst.resize(com_size);
    return true;
}

static bool zstdDecompress(HALCYON_STRING_VIEW_NS string_view src, std::string& dst)
{
    size_t dst_size = ZSTD_getFrameContentSize(src.data(), src.size());
    if (1 == ZSTD_isError(dst_size)) {
        return false;
    }
    dst.resize(dst_size);

    size_t decom_size = ZSTD_decompress(const_cast<char*>(dst.data()), dst_size, src.data(), src.size());
    if (1 == ZSTD_isError(decom_size)) {
        return false;
    }
    dst.resize(decom_size);
    return true;
}

#endif

LOG_BEGIN_NAMESPACE

bool compress(HALCYON_STRING_VIEW_NS string_view src, std::string& dst)
{
#ifdef USE_HALCYON_LZ4
    return ::lz4Compress(src, dst);
#elif defined USE_HALCYON_ZSTD
    return ::zstdCompress(src, dst);
#else
    dst = src.data();
    return true;
#endif
}

bool decompress(HALCYON_STRING_VIEW_NS string_view src, std::string& dst)
{
#ifdef USE_HALCYON_LZ4
    return ::lz4Decompress(src, dst);
#elif defined USE_HALCYON_ZSTD
    return ::zstdDecompress(src, dst);
#else
    dst = src.data();
    return true;
#endif
}

LOG_END_NAMESPACE