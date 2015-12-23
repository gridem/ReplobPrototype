/*
 * Copyright 2015 Grigory Demchenko (aka gridem)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <synca/synca.h>
#include <synca/log.h>
#include <synca/mutex.h>

#include "ut.h"

using namespace synca;

TEST(Mutex, 1)
{
    ThreadPool tp(4, "ch");
    scheduler<DefaultTag>().attach(tp);
    Mutex m;
    go([&] {
        Lock _{m};
    });
    waitForAll();
}

TEST(Channel, stability)
{
    ThreadPool tp(threadConcurrency(), "ch");
    scheduler<DefaultTag>().attach(tp);

    const int workers = threadConcurrency() * 2;

    // TODO: implement group of goers to cancel them, wait, detach (with clear etc)
    std::vector<Goer> a;
    Mutex m;
    Atomic<int> counter;
    for (int i = 0; i < workers; ++ i)
    {
        a.emplace_back(go([&] {
            while (true)
            {
                Lock _{m};
                ASSERT_EQ(1, ++ counter);
                reschedule();
                -- counter;
            }
        }));
    }

    sleepFor(1000*1);
    for (auto&& g: a)
        g.cancel();
    waitForAll();
}

CPPUT_TEST_MAIN
