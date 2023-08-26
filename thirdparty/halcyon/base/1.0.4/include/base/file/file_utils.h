#ifndef BASE_FILE_OPT_H
#define BASE_FILE_OPT_H

#include <base/common/base_define.h>

#ifdef USE_HALCYON_STRING_VIEW
#include <base/string/string_view.h>
#else
#include <string_view>
#endif

#include <vector>

BASE_BEGIN_NAMESPACE

#ifndef USE_HALCYON_STRING_VIEW
using std::string_view;
#endif

namespace file
{
    /**
     * @brief       判断文件夹或文件是否存在
     * @param[in]   文件或文件夹路径
     * @return      是否存在
     */
    extern "C" HALCYON_BASE_API bool exists(string_view filename);

    /**
     * @brief       创建文件夹
     * @param[in]   文件夹路径(./test/test)
     * @return      是否成功
     * @ps          路径最后不能有 '/' 或 '\\'
     */
    extern "C" HALCYON_BASE_API bool createDir(string_view dir);

    /**
     * @brief       删除文件夹
     * @param[in]   文件夹路径
     * @return      是否成功
     */
    extern "C" HALCYON_BASE_API bool removeDir(string_view dir);

    /**
     * @brief       获取文件夹下的目录或文件
     * @param[in]   文件夹路径
     * @param[out]  当前目录下的文件夹名
     * @param[out]  当前目录下的文件名
     */
    extern "C" HALCYON_BASE_API void listDir(string_view dir, std::vector<std::string>& dirs, std::vector<std::string>& files);

    /**
     * @brief       删除文件
     * @param[in]   文件路径
     * @return      是否成功
     */
    extern "C" HALCYON_BASE_API bool removeFile(string_view file);
}

BASE_END_NAMESPACE

#endif