// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <atomic>
#include <chrono>
#include <thread>

#include "gtest/gtest.h"

#include "cangjie/Utils/Semaphore.h"

using namespace Cangjie;
using namespace Cangjie::Utils;

/**
 * `Semaphore::Get()` is a process-wide singleton, so its count has to be restored from TearDown -
 * a failing EXPECT_* must not leave the rest of the binary running against a mangled count.
 */
class SemaphoreTest : public testing::Test {
protected:
    void SetUp() override
    {
        original = Semaphore::Get().GetCount();
    }
    void TearDown() override
    {
        Semaphore::Get().SetCount(original);
    }
    std::size_t original = 0;
};

TEST_F(SemaphoreTest, SetAndGetCountRoundTrip)
{
    Semaphore& sem = Semaphore::Get();
    EXPECT_GE(original, 1U);

    sem.SetCount(3);
    EXPECT_EQ(sem.GetCount(), 3U);

    sem.Acquire();
    EXPECT_EQ(sem.GetCount(), 2U);
    sem.Release();
    EXPECT_EQ(sem.GetCount(), 3U);
}

TEST_F(SemaphoreTest, AcquireBlocksUntilAnotherThreadReleases)
{
    Semaphore& sem = Semaphore::Get();

    sem.SetCount(1);
    sem.Acquire();
    EXPECT_EQ(sem.GetCount(), 0U);

    std::atomic<bool> acquired{false};
    std::thread waiter([&sem, &acquired]() {
        // Blocks in cv.wait until the main thread releases the only permit.
        sem.Acquire();
        acquired = true;
        sem.Release();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(acquired.load());

    sem.Release();
    waiter.join();
    EXPECT_TRUE(acquired.load());
}
