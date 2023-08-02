#include "logging.h"

#include <string>
#include <thread>
#include <windows.h>

using halcyon::log::Fmt;
void threadProc(bool longLog)
{
    int cnt = 0;
    const int kBatch = 100000;
    std::string empty = " ";
    std::string longStr(3000, 'X');
    longStr += " ";

    for (int t = 0; t < 3000; ++t)
    {
        Sleep(5000);
        for (int i = 0; i < kBatch; ++i)
        {
            LOG_TRACE << "0123456789";
            LOG_DEBUG << "abcdefghijklmnopqrstuvwxyz";
            LOG_INFO << "Hello 0123456789" << " abcdefghijklmnopqrstuvwxyz "
                << (longLog ? longStr : empty)
                << cnt;
            LOG_WARN << "abcdefghijklmnopqrstuvwxyz";
            LOG_ERROR << "0123456789";
            ++cnt;
            //Sleep(5000);
        }
    }
    
    double x = 19.82;
    int y = 43;
    //LOG_INFO << Fmt("%8.3f", x) << Fmt("%4d", y);
}

int main(int argc, char* argv[])
{
    halcyon::initLog("test");
    FLAGS_log_dir = R"(D:\logs\log\log\)";

    std::thread t1(threadProc, true);
    std::thread t2(threadProc, false);

    t1.join();
    t2.join();

    getchar();
    halcyon::uninitLog();
    return 0;
}
