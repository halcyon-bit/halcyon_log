#include "log_file.h"
#include "log_define.h"
#include "base/common/platform.h"
#include "base/utility/utility.h"

#include <chrono>
#include <cassert>
#include <filesystem>

// 最大日志文件大小（以MB为单位）,超过会对文件进行分割  FLAGS_max_log_size
DECLARE_uint32(max_log_size);

// 日志保存目录  FLAGS_log_dir
DECLARE_string(log_dir);

using namespace halcyon;
using namespace halcyon::log;

using std::chrono::system_clock;
using std::chrono::time_point;
using std::chrono::milliseconds;

LogFile::LogFile(std::string_view filename)
{
    if (!std::filesystem::exists(FLAGS_log_dir)) {
        std::filesystem::create_directories(FLAGS_log_dir);
    }

    fp_ = ::fopen((FLAGS_log_dir + filename.data()).c_str(), "a");
    if (fp_ != nullptr) {
        ::setbuf(fp_, buffer_);
    }
    writtenBytes_ = 0;
}

LogFile::~LogFile()
{
    if (fp_ != nullptr) {
        ::fclose(fp_);
    }
}

void LogFile::append(const char* logline, const size_t len)
{
    if (fp_ == nullptr) {
        return;
    }

    size_t written = 0;
    while (written != len) {
        size_t remain = len - written;
        size_t n = write(logline, len);
        if (n != remain) {
            int err = ferror(fp_);
            if (err) {
                break;
            }
        }
        written += n;
    }
    writtenBytes_ += written;
}

void LogFile::flush()
{
    if (fp_ != nullptr) {
        ::fflush(fp_);
    }
}

size_t LogFile::write(const char* logline, size_t len)
{
    assert(fp_ != nullptr);
#ifdef WINDOWS
    return ::_fwrite_nolock(logline, 1, len, fp_);
#elif defined LINUX
    return ::fwrite_unlocked(logline, 1, len, fp_);
#endif
}



LogFileManager::LogFileManager(const std::string& namePrefix, bool threadSafe)
    : namePrefix_(namePrefix)
{
    threadSafe_ = threadSafe;

    assert(namePrefix.find('/') == std::string::npos);
    rollFile();
}

LogFileManager::~LogFileManager() = default;

void LogFileManager::append(const char* logline, size_t len)
{
    if (threadSafe_) {
        std::unique_lock<std::mutex> lock(mutex_);
        append_unlocked(logline, len);
    }
    else {
        append_unlocked(logline, len);
    }
}

void LogFileManager::flush()
{
    if (threadSafe_) {
        std::unique_lock<std::mutex> lock(mutex_);
        file_->flush();
    }
    else {
        file_->flush();
    }
}

inline void LogFileManager::rollFile()
{
    std::string filename = getLogFileName();
    file_.reset(new LogFile(filename));
}

inline uint32_t LogFileManager::maxLogSize()
{
    return (FLAGS_max_log_size > 0 && FLAGS_max_log_size < 4096 ? FLAGS_max_log_size : 1);
}

void LogFileManager::append_unlocked(const char* logline, const size_t len)
{
    if (file_->writtenBytes() >> 20U >= maxLogSize()) {
        // 当某个文件写满时，重写创建一个文件
        rollFile();
    }

    // 将信息写入缓冲区
    file_->append(logline, len);
}

std::string LogFileManager::getLogFileName()
{
    std::string filename;
    filename.reserve(namePrefix_.size() + 64);
    filename = namePrefix_;

    time_point<system_clock, milliseconds> tp = std::chrono::time_point_cast<milliseconds>(system_clock::now());
    uint64_t milliTime = tp.time_since_epoch().count();

    filename += base::formatTime("_%Y%m%d_%H%M%S.", milliTime / 1000);
    filename += base::format("%03d", milliTime % 1000);
    filename += ".log";
    return filename;
}
