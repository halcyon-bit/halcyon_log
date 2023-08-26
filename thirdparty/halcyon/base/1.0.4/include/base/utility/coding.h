#ifndef BASE_CODING_H
#define BASE_CODING_H

#include <base/common/base_define.h>

#include<locale>
#include<codecvt>

BASE_BEGIN_NAMESPACE

/**
 * @brief   编码转换
 */
inline std::string utf8_to_gbk(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t> > conv;
    std::wstring tmp_wstr = conv.from_bytes(str);

    //GBK locale name in windows
    const char* kGbkLocaleName = ".936";
    std::wstring_convert<std::codecvt_byname<wchar_t, char, mbstate_t>> convert(new std::codecvt_byname<wchar_t, char, mbstate_t>(kGbkLocaleName));
    return convert.to_bytes(tmp_wstr);
}

inline std::string gbk_to_utf8(const std::string& str)
{
    //GBK locale name in windows
    const char* kGbkLocaleName = ".936";
    std::wstring_convert<std::codecvt_byname<wchar_t, char, mbstate_t>> convert(new std::codecvt_byname<wchar_t, char, mbstate_t>(kGbkLocaleName));
    std::wstring tmp_wstr = convert.from_bytes(str);

    std::wstring_convert<std::codecvt_utf8<wchar_t>> cv2;
    return cv2.to_bytes(tmp_wstr);
}

BASE_END_NAMESPACE

#endif