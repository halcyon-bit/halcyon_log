#include "log_file.h"

#include <base/file/file_opt.h>

#include <log/fmt/chrono.h>

#include <cstdio>
#include <cassert>
#include <chrono>

using namespace halcyon::log;

constexpr int kDayOfSeconds = 24 * 60 * 60;

LogFile::LogFile(HALCYON_STRING_VIEW_NS string_view filename)
{
    written_bytes_ = 0;
#ifdef WINDOWS
    // 这样不会独占文件
    fp_ = ::_fsopen(filename.data(), "wb", _SH_DENYNO);
    if (fp_ != nullptr) {
        ::setvbuf(fp_, buffer_, _IOFBF, sizeof(buffer_));
    }
#elif defined LINUX
    fp_ = ::fopen(filename.data(), "wb");
    if (fp_ != nullptr) {
        ::setbuffer(fp_, buffer_, sizeof(buffer_));
    }
#endif
}

LogFile::~LogFile()
{
    if (fp_ != nullptr) {
        ::fclose(fp_);
    }
}

void LogFile::append(HALCYON_STRING_VIEW_NS string_view logline)
{
    if (fp_ == nullptr) {
        return;
    }

    const char* start = logline.data();
    const size_t len = logline.size();

    size_t n = write(start, len);
    size_t remain = len - n;
    while (remain > 0) {
        size_t x = write(start + n, remain);
        if (x == 0) {
            int err = ferror(fp_);
            if (err != 0)
                break;
        }
        n += x;
        remain -= x;
    }

    written_bytes_ += len;
}

void LogFile::flush()
{
    if (fp_ == nullptr) {
        return;
    }

    ::fflush(fp_);
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


LogFileManager::LogFileManager(HALCYON_STRING_VIEW_NS string_view dir, HALCYON_STRING_VIEW_NS string_view prefix,
    size_t max_size, size_t max_file, size_t flush_interval, bool thread_safe)
    : max_size_(max_size), max_file_(max_file)
    , flush_interval_(flush_interval), thread_safe_(thread_safe)
{
    prefix_.append(dir.data()).append("/");
    prefix_.append(prefix.data());

    if (!base::file::exists(dir.data())) {
        base::file::createDir(dir.data());
        cur_file_count_ = 0;
    }
    else {
        std::vector<std::string> dirs;
        std::vector<std::string> files;
        base::file::listDir(dir.data(), dirs, files);

        for (auto& each : files) {
            if (each.find(prefix.data()) != std::string::npos) {
                ++cur_file_count_;
                std::string tmp;
                tmp.reserve(128);
                tmp.append(dir.data()).append("/");
                tmp.append(std::move(each));
                names_.push_back(std::move(tmp));
            }
        }
    }

    rollFile();
}

LogFileManager::~LogFileManager() = default;

void LogFileManager::append(HALCYON_STRING_VIEW_NS string_view logline)
{
    if (thread_safe_) {
        std::unique_lock<std::mutex> locker(mutex_);
        append_unlocked(logline);
    }
    else {
        append_unlocked(logline);
    }
}

void LogFileManager::flush()
{
    if (thread_safe_) {
        std::unique_lock<std::mutex> lock(mutex_);
        file_->flush();
    }
    else {
        file_->flush();
    }
}

inline void LogFileManager::rollFile()
{
    time_t now = ::time(nullptr);
    std::string filename = genLogFileName();

    while (cur_file_count_ >= max_file_) {
        // 删除多余文件
        auto& name = names_.front();
        base::file::removeFile(name);
        names_.pop_front();
        --cur_file_count_;
    }

    names_.push_back(filename);
    ++cur_file_count_;

    start_of_time_ = now / kDayOfSeconds * kDayOfSeconds;
    last_flush_ = now;

    file_.reset(new LogFile(filename));
}

inline size_t LogFileManager::maxLogSize()
{
    return (max_size_ > 0 && max_size_ < 4096) ? max_size_ : 1;
}

void LogFileManager::append_unlocked(HALCYON_STRING_VIEW_NS string_view logline)
{
    // 将信息写入缓冲区
    file_->append(logline);

    if (file_->writtenBytes() >> 10U >= maxLogSize()) {
        // 当某个文件写满时，重写创建一个文件
        rollFile();
    }
    else {
        time_t now = ::time(nullptr);
        time_t today = now / kDayOfSeconds * kDayOfSeconds;
        if (today != start_of_time_) {
            // 每天 0 点滚动日志
            rollFile();
        }
        else if (size_t(now - last_flush_) > flush_interval_) {
            last_flush_ = now;
            file_->flush();
        }
    }
}

std::string LogFileManager::genLogFileName()
{
    std::string filename;
    filename.reserve(prefix_.size() + 64);
    filename.append(prefix_);

    auto tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    uint64_t milli = tp.time_since_epoch().count();
    filename.append(fmt::format("_{:%Y%m%d_%H%M%S}.{:03d}", fmt::localtime(milli / 1000), milli % 1000));
    filename.append(".log");
    return filename;
}
