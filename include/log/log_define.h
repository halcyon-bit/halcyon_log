#ifndef LOG_DEFINE_H
#define LOG_DEFINE_H

#include <base/common/platform.h>

#if defined USE_CPP14 || defined USE_CPP11
#define LOG_BEGIN_NAMESPACE    namespace halcyon { namespace log {

#define LOG_END_NAMESPACE      }  }
#else
// c++17 以上
#define LOG_BEGIN_NAMESPACE    namespace halcyon::log {

#define LOG_END_NAMESPACE      }
#endif


#if defined WINDOWS
#ifdef HALCYON_LOG_DLL_BUILD
#define HALCYON_LOG_API _declspec(dllexport)
#elif defined HALCYON_LOG_STATIC
#define HALCYON_LOG_API
#else
#define HALCYON_LOG_API _declspec(dllimport)
#endif
#else
#define HALCYON_LOG_API
#endif


// 使用 lz4 压缩算法
// #define USE_HALCYON_COMPRESS_LZ4

// 使用 zstd 压缩算法
// #define USE_HALCYON_COMPRESS_ZSTD

// 未定义以上两个，则不压缩

#endif