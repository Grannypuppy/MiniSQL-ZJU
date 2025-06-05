#include "buffer/clock_replacer.h"

#include "gtest/gtest.h"

TEST(CLOCKReplacerTest, SampleTest) {
    CLOCKReplacer clock_replacer(7);

    // Scenario: unpin six elements, i.e. add them to the replacer.
    clock_replacer.Unpin(1);
    clock_replacer.Unpin(2);
    clock_replacer.Unpin(3);
    clock_replacer.Unpin(4);
    clock_replacer.Unpin(5);
    clock_replacer.Unpin(6);
    clock_replacer.Unpin(1);  // 重复unpin，只是重新设置引用位
    EXPECT_EQ(6, clock_replacer.Size());

    // Scenario: get victims from the clock replacer.
    // 第一轮：所有页面引用位都是1，会被设置为0并重新入队
    // 第二轮：找到引用位为0的页面进行替换（按添加顺序）
    int value;
    clock_replacer.Victim(&value);
    EXPECT_EQ(1, value);  // 第一个添加的页面
    clock_replacer.Victim(&value);
    EXPECT_EQ(2, value);  // 第二个添加的页面
    clock_replacer.Victim(&value);
    EXPECT_EQ(3, value);  // 第三个添加的页面

    // Scenario: pin elements in the replacer.
    // Note that 3 has already been victimized, so pinning 3 should have no effect.
    clock_replacer.Pin(3);  // 无效果，因为3已经被移除
    clock_replacer.Pin(4);  // 移除4
    EXPECT_EQ(2, clock_replacer.Size());  // 剩余5,6

    // Scenario: unpin 4. We expect that the reference bit of 4 will be set to 1.
    clock_replacer.Unpin(4);  // 重新添加4，引用位设为1
    EXPECT_EQ(3, clock_replacer.Size());  // 现在有5,6,4

    // Scenario: continue looking for victims.
    clock_replacer.Victim(&value);
    EXPECT_EQ(5, value);  // 5的引用位在第一轮被设为0
    clock_replacer.Victim(&value);
    EXPECT_EQ(6, value);  // 6的引用位在第一轮被设为0
    clock_replacer.Victim(&value);
    EXPECT_EQ(4, value);  // 4刚添加，引用位为1，需要两轮才能被替换

    // 新的测试场景
    CLOCKReplacer clock_replacer_new(5);
    clock_replacer_new.Unpin(1);
    clock_replacer_new.Unpin(2);
    clock_replacer_new.Unpin(3);
    clock_replacer_new.Unpin(4);
    clock_replacer_new.Unpin(5);
    // 容量已满，再unpin会触发victim操作
    clock_replacer_new.Unpin(6);  // 这会先victim一个页面，然后添加6
    EXPECT_EQ(5, clock_replacer_new.Size());

    // 测试基本的victim顺序
    clock_replacer_new.Victim(&value);
    // 刚才Unpin(6)时，1先被设置为0，然后被victim掉了，最后在队末尾添加了6，所以下一步是2
    EXPECT_EQ(2, value);
}