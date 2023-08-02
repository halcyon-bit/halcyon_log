#include "compress_opt.h"

#include "base/time/elapsed_timer.h"

using namespace halcyon;

std::string randStr(size_t len)
{
    std::string str;
    str.reserve(len + 1);

    for (size_t i = 0; i < len; ++i) {
        char c = 'a' + rand() % 26;
        str.push_back(c);
    }
    return str;
}

int main(int argc, char* argv[])
{
    std::string str1 = randStr(1024);


    std::string str2 = "Narrator: It is raining today.";

    std::string dst1;
    halcyon::log::compress(str1, dst1);
    std::string dst3;
    halcyon::log::decompress(dst1, dst3);
    std::string dst2;
    halcyon::log::compress(str2, dst2);

    dst1 += dst2;

    std::string dst_;
    halcyon::log::decompress(dst1, dst_);
    return 1;
}
