#ifndef BASE_VERSION_H
#define BASE_VERSION_H

#include <base/common/base_define.h>

BASE_BEGIN_NAMESPACE

/**
 * @brief       获取版本号
 * @return      版本号
 */
extern "C" HALCYON_BASE_API const char* version();

BASE_END_NAMESPACE

#endif