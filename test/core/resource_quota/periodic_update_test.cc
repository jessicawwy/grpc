// Copyright 2021 gRPC authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/core/lib/resource_quota/periodic_update.h"

#include <thread>

#include <gtest/gtest.h>

#include "absl/synchronization/notification.h"

#include "src/core/lib/iomgr/exec_ctx.h"

namespace grpc_core {
namespace testing {

TEST(PeriodicUpdateTest, SimpleTest) {
  std::unique_ptr<PeriodicUpdate> upd;
  Timestamp start;
  // Create a periodic update that updates every second.
  {
    ExecCtx exec_ctx;
    upd = absl::make_unique<PeriodicUpdate>(Duration::Seconds(1));
    start = exec_ctx.Now();
  }
  // Wait until the first period has elapsed.
  while (true) {
    ExecCtx exec_ctx;
    if (upd->Tick()) break;
  }
  // Ensure that took at least 1 second.
  {
    ExecCtx exec_ctx;
    EXPECT_GE(exec_ctx.Now() - start, Duration::Seconds(1));
    start = exec_ctx.Now();
  }
  // Do ten more update cycles
  for (int i = 0; i < 10; i++) {
    while (true) {
      ExecCtx exec_ctx;
      if (upd->Tick()) break;
    }
    // Ensure the time taken was between 1 and 1.5 seconds - we make a little
    // allowance for the presumed inaccuracy of this type.
    {
      ExecCtx exec_ctx;
      EXPECT_GE(exec_ctx.Now() - start, Duration::Seconds(1));
      EXPECT_LE(exec_ctx.Now() - start, Duration::Milliseconds(1500));
      start = exec_ctx.Now();
    }
  }
}

TEST(PeriodicUpdate, ThreadTest) {
  std::unique_ptr<PeriodicUpdate> upd;
  std::atomic<int> count(0);
  Timestamp start;
  // Create a periodic update that updates every second.
  {
    ExecCtx exec_ctx;
    upd = absl::make_unique<PeriodicUpdate>(Duration::Seconds(1));
    start = exec_ctx.Now();
  }
  // Run ten threads all updating the counter continuously, for a total of ten
  // update cycles.
  // This allows TSAN to catch threading issues.
  std::vector<std::thread> threads;
  for (size_t i = 0; i < 10; i++) {
    threads.push_back(std::thread([&]() {
      while (count.load() < 10) {
        ExecCtx exec_ctx;
        if (upd->Tick()) count.fetch_add(1);
      }
    }));
  }

  // Finish all threads.
  for (auto& th : threads) {
    th.join();
  }
  // Ensure our ten cycles took at least 10 seconds, and no more than 15.
  {
    ExecCtx exec_ctx;
    EXPECT_GE(exec_ctx.Now() - start, Duration::Seconds(10));
    EXPECT_LE(exec_ctx.Now() - start, Duration::Seconds(15));
  }
}

}  // namespace testing
}  // namespace grpc_core

// Hook needed to run ExecCtx outside of iomgr.
void grpc_set_default_iomgr_platform() {}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  gpr_log_verbosity_init();
  gpr_time_init();
  return RUN_ALL_TESTS();
}
