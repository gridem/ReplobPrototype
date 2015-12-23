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
#include <synca/channel.h>

#include "ut.h"

using namespace synca;

TEST(Channel, 1)
{
    ThreadPool tp(4, "ch");
    scheduler<DefaultTag>().attach(tp);
    Channel<int> c;
    go([&] {
        for (int i = 0; i < 10; ++ i)
            c.put(i);
        c.close();
    });
    go([&] {
        int i = 0;
        for (int v: c)
        {
            ASSERT_EQ(i++, v);
        }
        ASSERT_EQ(10, i);
    });
    waitForAll();
}

TEST(Channel, cancel)
{
    ThreadPool tp(4, "ch");
    scheduler<DefaultTag>().attach(tp);
    Channel<int> c;
    go([&] {
        int i = 0;
        for (int v: c)
        {
            ASSERT_EQ(i++, v);
        }
        ASSERT_TRUE(i > 0);
    });
    go([&] {
        auto _ = closer(c);
        for (int i = 0; i < 10; ++ i)
        {
            c.put(i);
            reschedule();
        }
    }).cancel();
    waitForAll();
}

TEST(Channel, stability)
{
    ThreadPool tp(threadConcurrency(), "ch");
    scheduler<DefaultTag>().attach(tp);

    constexpr int blocks = 100;
    constexpr int consumers = 2;
    constexpr int producers = 2;

    std::vector<Goer> a;
    Channel<int> c;

    auto goInf = [&](Handler h) {
        a.emplace_back(go([&, h] {
            while (true)
            {
                for (int b = 0; b < blocks; ++ b)
                    h();
                reschedule();
            }
        }));
    };

    for (int i = 0; i < consumers; ++ i)
        goInf([&] {c.put(0);});

    for (int i = 0; i < producers; ++ i)
        goInf([&] {c.get();});

    sleepFor(1000*5);
    for (auto&& g: a)
        g.cancel();
    waitForAll();
}

CPPUT_TEST_MAIN
