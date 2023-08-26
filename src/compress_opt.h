#ifndef LOG_COMPRESS_OPT_H
#define LOG_COMPRESS_OPT_H

#include <log/log_define.h>

#include <base/common/base_define.h>

#ifdef USE_HALCYON_STRING_VIEW
#include <base/string/string_view.h>
#define HALCYON_STRING_VIEW_NS  ::halcyon::base::
#else
#include <string_view>
#define HALCYON_STRING_VIEW_NS  ::std::
#endif

LOG_BEGIN_NAMESPACE

bool compress(HALCYON_STRING_VIEW_NS string_view src, std::string& dst);

bool decompress(HALCYON_STRING_VIEW_NS string_view src, std::string& dst);

LOG_END_NAMESPACE

#endif