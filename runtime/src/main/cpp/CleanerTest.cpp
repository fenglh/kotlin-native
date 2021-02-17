/*
 * Copyright 2010-2020 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license
 * that can be found in the LICENSE file.
 */

#include "Cleaner.h"

#include <future>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "Atomic.h"
#include "TestSupport.hpp"
#include "TestSupportCompilerGenerated.hpp"
#include "Types.h"

using namespace kotlin;
using testing::_;

// TODO: Also test disposal. (This requires extracting Worker interface)

class CleanerTest: public testing::Test {
public:
    CleanerTest() {}
    ~CleanerTest() {
        ResetCleanerWorkerForTests();
    }
private:
    InitMemoryForTestsGuard memoryInitGuard_;
};

TEST_F(CleanerTest, ConcurrentCreation) {
    constexpr int threadCount = kotlin::kDefaultThreadCount;
    constexpr KInt workerId = 42;

    auto createCleanerWorkerMock = ScopedCreateCleanerWorkerMock();

    EXPECT_CALL(*createCleanerWorkerMock, Call()).Times(1).WillOnce(testing::Return(workerId));

    int startedThreads = 0;
    bool allowRunning = false;
    KStdVector<std::future<KInt>> futures;
    for (int i = 0; i < threadCount; ++i) {
        auto future = std::async(std::launch::async, [&startedThreads, &allowRunning]() {
            InitMemoryForTestsGuard initGuard;
            atomicAdd(&startedThreads, 1);
            while (!atomicGet(&allowRunning)) {
            }
            // New MM expects running getCleanerWorker in the native thread state.
            mm::CurrentThreadStateGuard stateGuard(mm::ThreadState::kNative);
            return Kotlin_CleanerImpl_getCleanerWorker();
        });
        futures.push_back(std::move(future));
    }
    while (atomicGet(&startedThreads) != threadCount) {
    }
    atomicSet(&allowRunning, true);
    KStdVector<KInt> values;
    for (auto& future : futures) {
        values.push_back(future.get());
    }

    ASSERT_THAT(values.size(), threadCount);
    EXPECT_THAT(values, testing::Each(workerId));
}

TEST_F(CleanerTest, ShutdownWithoutCreation) {
    auto createCleanerWorkerMock = ScopedCreateCleanerWorkerMock();
    auto shutdownCleanerWorkerMock = ScopedShutdownCleanerWorkerMock();

    EXPECT_CALL(*createCleanerWorkerMock, Call()).Times(0);
    EXPECT_CALL(*shutdownCleanerWorkerMock, Call(_, _)).Times(0);
    ShutdownCleaners(true);
}

TEST_F(CleanerTest, ShutdownWithCreation) {
    constexpr KInt workerId = 42;
    constexpr bool executeScheduledCleaners = true;

    auto createCleanerWorkerMock = ScopedCreateCleanerWorkerMock();
    auto shutdownCleanerWorkerMock = ScopedShutdownCleanerWorkerMock();

    {
        // New MM expects running getCleanerWorker in the native thread state.
        mm::CurrentThreadStateGuard stateGuard(mm::ThreadState::kNative);
        EXPECT_CALL(*createCleanerWorkerMock, Call()).WillOnce(testing::Return(workerId));
        Kotlin_CleanerImpl_getCleanerWorker();
    }

    EXPECT_CALL(*shutdownCleanerWorkerMock, Call(workerId, executeScheduledCleaners));
    ShutdownCleaners(executeScheduledCleaners);
}
