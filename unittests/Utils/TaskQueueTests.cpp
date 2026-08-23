// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "cangjie/Utils/TaskQueue.h"

using namespace Cangjie;
using namespace Cangjie::Utils;

TEST(TaskQueueTest, ZeroThreadsStillExecutesEveryTask)
{
    // threadsNum == 0 must be clamped to 1, otherwise the queued tasks would never run.
    TaskQueue queue(0);
    std::atomic<int> executed{0};
    for (int i = 0; i < 8; ++i) {
        (void)queue.AddTask<void>([&executed]() { ++executed; });
    }
    queue.RunAndWaitForAllTasksCompleted();
    EXPECT_EQ(executed.load(), 8);
}

TEST(TaskQueueTest, TaskResultsAreDeliveredToTheCaller)
{
    TaskQueue queue(4);
    std::vector<TaskResult<int>> results;
    for (int i = 0; i < 16; ++i) {
        results.emplace_back(queue.AddTask<int>([i]() { return i * i; }));
    }
    queue.RunAndWaitForAllTasksCompleted();

    int sum = 0;
    for (auto& r : results) {
        sum += r.get();
    }
    // 0^2 + 1^2 + ... + 15^2
    EXPECT_EQ(sum, 1240);
}

TEST(TaskQueueTest, RunInBackgroundThenWaitCompletesAllTasks)
{
    TaskQueue queue(2);
    std::atomic<int> executed{0};
    for (int i = 0; i < 10; ++i) {
        (void)queue.AddTask<void>([&executed]() {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            ++executed;
        });
    }
    queue.RunInBackground();
    queue.WaitForAllTasksCompleted();
    EXPECT_EQ(executed.load(), 10);

    // Waiting again on a drained queue is harmless.
    queue.WaitForAllTasksCompleted();
    EXPECT_EQ(executed.load(), 10);
}

TEST(TaskQueueTest, EmptyQueueSpawnsNoThreads)
{
    // Both entry points return before creating threads, so the queue is still in its
    // "not started" state and accepting tasks - which is what makes the run below legal
    // (AddTask asserts on !isStarted).
    TaskQueue background(4);
    background.RunInBackground();
    background.WaitForAllTasksCompleted();

    TaskQueue blocking(4);
    blocking.RunAndWaitForAllTasksCompleted();

    std::atomic<int> executed{0};
    auto result = blocking.AddTask<int>([&executed]() {
        ++executed;
        return 7;
    });
    blocking.RunAndWaitForAllTasksCompleted();
    EXPECT_EQ(result.get(), 7);
    EXPECT_EQ(executed.load(), 1);
}

TEST(TaskQueueTest, HigherPriorityTaskRunsFirst)
{
    // A single worker makes the ordering deterministic.
    TaskQueue queue(1);
    std::mutex mtx;
    std::vector<int> order;
    for (int i = 0; i < 4; ++i) {
        (void)queue.AddTask<void>(
            [&mtx, &order, i]() {
                std::lock_guard<std::mutex> lock(mtx);
                order.push_back(i);
            },
            static_cast<uint64_t>(i));
    }
    queue.RunAndWaitForAllTasksCompleted();

    ASSERT_EQ(order.size(), 4U);
    EXPECT_EQ(order.front(), 3);
    EXPECT_EQ(order.back(), 0);
}
