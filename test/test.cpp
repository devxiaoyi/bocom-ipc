#include "bocom_ipc.h"
#include "gtest/gtest.h"

#include <vector>

TEST(BCOMTest, QueueTest)
{
    constexpr auto maxElementSize = 1024;
    constexpr auto queueSize = 3;
    st_QUEUE_INFO queueInfo = {"test", maxElementSize, queueSize, Polling};
    auto pubContext = BOCOM_CreateQueue(&queueInfo);
    ASSERT_NE(pubContext, nullptr);

    // Publish data to the queue.
    const auto pubElement1 = std::vector<char>(maxElementSize / 2, 0xf);
    const auto pubElement2 = std::vector<char>(maxElementSize, 0xff);
    ASSERT_EQ(BOCOM_PublishQueue(pubContext, pubElement1.data(), pubElement1.size()), Success);
    ASSERT_EQ(BOCOM_PublishQueue(pubContext, pubElement2.data(), pubElement2.size()), Success);

    auto subContext = BOCOM_JoinQueue("test");
    ASSERT_NE(subContext, nullptr);

    // Retrieve data.
    auto subElement1 = std::vector<char>(maxElementSize / 2, 1);
    auto subElement2 = std::vector<char>(maxElementSize, 2);
    auto elementSize1 = static_cast<unsigned int>(subElement1.size());
    auto elementSize2 = static_cast<unsigned int>(subElement2.size());
    ASSERT_EQ(BOCOM_RetrieveQueue(subContext, subElement1.data(), &elementSize1), Success);
    ASSERT_EQ(BOCOM_RetrieveQueue(subContext, subElement2.data(), &elementSize2), Success);
    ASSERT_EQ(elementSize1, subElement1.size());
    ASSERT_EQ(elementSize2, subElement2.size());
    ASSERT_EQ(pubElement1, subElement1);
    ASSERT_EQ(pubElement2, subElement2);
}
