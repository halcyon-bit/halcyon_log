#include "log_file.h"

#include "gtest/gtest.h"

#include "base/file/file_opt.h"
#include "base/time/timestamp.h"

#include <string>
#include <memory>

using namespace halcyon;

TEST(LogFileTest, baseTest)
{
    {
        log::LogFile file("./log1.txt");

        std::string str = "Youth is not a time of life; it is a state of mind; it is not a matter of rosy cheeks, red lips and supple knees; it is a matter of the will, a quality of the imagination, a vigor of the emotions; it is the freshness of the deep springs of life."
            "Youth means a temperamental predominance of courage over timidity, of the appetite for adventure over the love of ease. This often exists in a man of 60 more than a boy of 20. Nobody grows old merely by a number of years. We grow old by deserting our ideals."
            "Years may wrinkle the skin, but to give up enthusiasm wrinkles the soul. Worry, fear, self-distrust bows the heart and turns the spirit back to dust."
            "Whether 60 or 16, there is in every human being’s heart the lure of wonders, the unfailing appetite for what’s next and the joy of the game of living. In the center of your heart and my heart, there is a wireless station; so long as it receives messages of beauty, hope, courage and power from man and from the infinite, so long as you are young."
            "When your aerials are down, and your spirit is covered with snows of cynicism and the ice of pessimism, then you’ve grown old, even at 20; but as long as your aerials are up, to catch waves of optimism, there’s hope you may die young at 80.\n";

        file.append(str);
        file.flush();

        file.append(str);
        file.append(str);

        EXPECT_EQ(str.size() * 3, file.writtenBytes());

        std::string str1(1024 * 1024, 'a');
        file.append(str1);

        EXPECT_EQ(str.size() * 3 + str1.size(), file.writtenBytes());
    }
    base::file::removeFile("./log1.txt");
}

TEST(LogFileTest, efficiency)
{
    {
        std::string str = "Youth is not a time of life; it is a state of mind; it is not a matter of rosy cheeks, red lips and supple knees; it is a matter of the will, a quality of the imagination, a vigor of the emotions; it is the freshness of the deep springs of life."
            "Youth means a temperamental predominance of courage over timidity, of the appetite for adventure over the love of ease. This often exists in a man of 60 more than a boy of 20. Nobody grows old merely by a number of years. We grow old by deserting our ideals."
            "Years may wrinkle the skin, but to give up enthusiasm wrinkles the soul. Worry, fear, self-distrust bows the heart and turns the spirit back to dust."
            "Whether 60 or 16, there is in every human being’s heart the lure of wonders, the unfailing appetite for what’s next and the joy of the game of living. In the center of your heart and my heart, there is a wireless station; so long as it receives messages of beauty, hope, courage and power from man and from the infinite, so long as you are young."
            "When your aerials are down, and your spirit is covered with snows of cynicism and the ice of pessimism, then you’ve grown old, even at 20; but as long as your aerials are up, to catch waves of optimism, there’s hope you may die young at 80.\n";
        str += str;

        log::LogFile file("./log2.txt");
        for (size_t i = 0; i < 100000; ++i) {
            file.append(str);
        }
    }
    base::file::removeFile("./log2.txt");
}

TEST(LogFileManagerTest, singleThread)
{
    {
        log::LogFileManager manager("./log", "single_test", 1024, 10, 3, false);

        std::string str = "Youth is not a time of life; it is a state of mind; it is not a matter of rosy cheeks, red lips and supple knees; it is a matter of the will, a quality of the imagination, a vigor of the emotions; it is the freshness of the deep springs of life."
            "Youth means a temperamental predominance of courage over timidity, of the appetite for adventure over the love of ease. This often exists in a man of 60 more than a boy of 20. Nobody grows old merely by a number of years. We grow old by deserting our ideals."
            "Years may wrinkle the skin, but to give up enthusiasm wrinkles the soul. Worry, fear, self-distrust bows the heart and turns the spirit back to dust."
            "Whether 60 or 16, there is in every human being’s heart the lure of wonders, the unfailing appetite for what’s next and the joy of the game of living. In the center of your heart and my heart, there is a wireless station; so long as it receives messages of beauty, hope, courage and power from man and from the infinite, so long as you are young."
            "When your aerials are down, and your spirit is covered with snows of cynicism and the ice of pessimism, then you’ve grown old, even at 20; but as long as your aerials are up, to catch waves of optimism, there’s hope you may die young at 80.\n";

        for (size_t i = 0; i < 10; ++i) {
            base::sleep(500);
            for (size_t j = 0; j < 1000; ++j) {
                manager.append(str);
            }
        }
    }
    base::file::removeDir("./log");
}

void test1(std::shared_ptr<log::LogFileManager> manager)
{
    std::string str = "When I picked up the phone I was greeted by a chorus of squalls, like a raging tempest on a warm summer night. "
        "I was used to bad connection on the weathered Harkwright County lines, and was just about to hang up, when I heard my own name "
        "amid the interference. | © Joe Zabel\n";

    for (size_t i = 0; i < 10; ++i) {
        base::sleep(50);
        for (size_t j = 0; j < 1000; ++j) {
            manager->append(str);
        }
    }
}

void test2(std::shared_ptr<log::LogFileManager> manager)
{
    std::string str = "Maybe we expected the sun to rise from the west, or the north or the south. Anything seemed possible. "
        "A male cardinal’s song, his proclamation of territory and of his own sexual fitness, dominated the early morning. "
        "Brian and I were drinking, as there was little else to do, and we tried not to think or talk about it. | © Jeff Dupuis\n";

    for (size_t i = 0; i < 10; ++i) {
        base::sleep(40);
        for (size_t j = 0; j < 1000; ++j) {
            manager->append(str);
        }
    }
}

void test3(std::shared_ptr<log::LogFileManager> manager)
{
    std::string str = "There was no clock on the nightstand between the two beds. Just an analogue phone and a brochure I’d "
        "taken from the front desk that advertised two free steak dinners down at the lounge. | © Abigail Stillwell\n";

    for (size_t i = 0; i < 10; ++i) {
        base::sleep(30);
        for (size_t j = 0; j < 1000; ++j) {
            manager->append(str);
        }
    }
}

void test4(std::shared_ptr<log::LogFileManager> manager)
{
    std::string str = "Crystal’s day was going terrible. That morning she and her mother met with the people at Welfare so her "
        "check wouldn’t be cut. She met with another representative in New Mexico Human Services so they could issue her an EBT "
        "card for emergency food stamps. She was squatting against the fake marble pillar in front of Bedlam, the for-profit "
        "college in Albuquerque’s South Valley where David Shimamura taught classes in Business Euphemism and Obfuscation 101. "
        "He noticed her as he came to work that afternoon. She looked forlorn. | © Richard Read Oyama\n";

    for (size_t i = 0; i < 10; ++i) {
        base::sleep(20);
        for (size_t j = 0; j < 1000; ++j) {
            manager->append(str);
        }
    }
}

TEST(LogFileManagerTest, multiThread)
{
    {
        std::shared_ptr<log::LogFileManager> manager = std::make_shared<log::LogFileManager>("./log", "multi_test", 1024, 10, 3, true);

        std::vector<std::thread> threads;
        threads.push_back(std::thread(&test1, manager));
        threads.push_back(std::thread(&test2, manager));
        threads.push_back(std::thread(&test3, manager));
        threads.push_back(std::thread(&test4, manager));

        for (size_t i = 0; i < threads.size(); ++i) {
            threads[i].join();
        }
    }
    base::file::removeDir("./log");
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
