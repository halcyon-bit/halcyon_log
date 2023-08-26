#ifndef BASE_DEFINE_H
#define BASE_DEFINE_H

#include <base/common/platform.h>

#if defined USE_CPP14 || defined USE_CPP11
#define BASE_BEGIN_NAMESPACE    namespace halcyon { namespace base {

#define BASE_END_NAMESPACE      }  }
#else
// c++17 以上
#define BASE_BEGIN_NAMESPACE    namespace halcyon::base {

#define BASE_END_NAMESPACE      }
#endif


#if defined WINDOWS
#ifdef  HALCYON_BASE_DLL_BUILD
#define  HALCYON_BASE_API _declspec(dllexport)
#elif defined HALCYON_BASE_STATIC
#define  HALCYON_BASE_API
#else
#define  HALCYON_BASE_API _declspec(dllimport)
#endif
#else
#define  HALCYON_BASE_API
#endif

// use string in halcyon::base replace std::string
//#define USE_HALCYON_STRING

// use string_view in halcyon::base replace std::string_view (c++17)
//#define USE_HALCYON_STRING_VIEW

// use any in halcyon::base replace std::any (c++17)
// #define USE_HALCYON_ANY

// use index_sequence in halcyon::base replace std::index_sequence (c++14)
// #define USE_HALCYON_INDEX_SEQUENCE

// use invoke in halcyon::base replace std::invoke (c++17)
// use apply in halcyon::base replace std::apply (c++17)
// #define USE_HALCYON_INVOKE_APPLY

#if defined USE_CPP11 || defined USE_CPP14
#define USE_HALCYON_STRING_VIEW
#endif

#if defined USE_CPP11 || defined USE_CPP14
#define USE_HALCYON_ANY
#endif

#ifdef USE_CPP11
#define USE_HALCYON_INDEX_SEQUENCE
#endif

#if defined USE_CPP11 || defined USE_CPP14
#define USE_HALCYON_INVOKE_APPLY
#endif

#endif