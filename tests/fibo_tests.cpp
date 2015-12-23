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

#include "ut.h"

using namespace synca;

template<typename T>
int fibo(int v)
{
    if (v < 2)
        return v;
    std::shared_ptr<int> v1 = std::make_shared<int>();
    std::shared_ptr<int> v2 = std::make_shared<int>();
    T{}
            .go([v, v1] {*v1 = fibo<T>(v-1);})
            .go([v, v2] {*v2 = fibo<T>(v-2);})
            .wait();
    JLOG(v << ", " << *v1 << ", " << *v2);
    return *v1 + *v2;
}

TEST(Awaiter, Fibo)
{
    // TODO: verify that the first coro completed before others
    ThreadPool tp(threadConcurrency(), "tp");
    scheduler<DefaultTag>().attach(tp);
    auto c = go([] {
        std::cout << "fibo: " << fibo<DetachableAwaiter>(5) << std::endl;
    });
    sleepFor(1);
    c.cancel();
    sleepFor(10);
    c.cancel();
    waitForAll();
}

CPPUT_TEST_MAIN
