#ifndef BASE_PLATFORM_H
#define BASE_PLATFORM_H

/// 平台判断
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    // define something for Windows (32-bit and 64-bit, this part is common)
    #ifdef _WIN64
        // define something for Windows (64-bit only)
        #define WINDOWS
    #else 
        // define something for Windows (32-bit only)
        #define WINDOWS
    #endif
#elif __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
        // iOS Simulator
    #elif TARGET_OS_IPHONE
        // iOS device
    #elif TARGET_OS_MAC
        // Other kinds of Mac OS
    #else
        #error "Unknown Apple platform"
    #endif
#elif __linux__
    // linux
    #define LINUX
#elif __unix__ // all unices not caught above
    // Unix
    #define UNIX
#elif defined(_POSIX_VERSION)
// POSIX
#else
#   error "Unknown compiler"
#endif

/// C++ 标准判断
#if defined WINDOWS && defined _MSC_VER
#if _MSVC_LANG >= 202002L  // c++20
#define USE_CPP20
#elif _MSVC_LANG >= 201703L  // C++17
#define USE_CPP17
#elif _MSVC_LANG >= 201402L  // C++14
#define USE_CPP14
#elif _MSVC_LANG >= 201103L  // C++11
#define USE_CPP11
#else
#error "should use c++11 implementation"
#endif
#else
#if __cplusplus >= 202002L  // c++20
#define USE_CPP20
#elif __cplusplus >= 201703L  // C++17
#define USE_CPP17
#elif __cplusplus >= 201402L  // C++14
#define USE_CPP14
#elif __cplusplus >= 201103L  // C++11
#define USE_CPP11
#else
#error "should use c++11 implementation"
#endif
#endif

#if defined USE_CPP11 || defined USE_CPP14
#define FALLTHROUGH
#else
// c++17 for switch
#define FALLTHROUGH     [[fallthrough]]
#endif

#endif
