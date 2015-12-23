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

void g()
{
    throw 1;
}

void f() noexcept
{
    g();
}

TEST(f, f)
{
    try
    {
        f();
    }
    catch(...)
    {
        LOG("catch ...");
    }
}

int fibo2_do(int v)
{
    if (v < 2)
        return v;
    int v1, v2;
    goWait2({
        [v, &v1] { v1 = fibo2_do(v-1); },
        [v, &v2] { v2 = fibo2_do(v-2); }
    });
    JLOG(v << ", " << v1 << ", " << v2);
    return v1 + v2;
}

TEST(goWait2, fibo)
{
    ThreadPool tp(threadConcurrency(), "tp");
    scheduler<DefaultTag>().attach(tp);
    auto c = go([] {
        std::cout << "fibo: " << fibo2_do(5) << std::endl;
    });
    sleepFor(1);
    c.cancel();
    sleepFor(1);
    c.cancel();
    waitForAll();
}

int fibo1_do(int v)
{
    if (v < 2)
        return v;
    int v1, v2;
    goWait({
        [v, &v1] { v1 = fibo1_do(v-1); },
        [v, &v2] { v2 = fibo1_do(v-2); }
    });
    JLOG(v << ", " << v1 << ", " << v2);
    return v1 + v2;
}

/*
TEST(goWait, fibo)
{
    ThreadPool tp(threadConcurrency(), "tp");
    scheduler<DefaultTag>().attach(tp);
    auto c = go([] {
        std::cout << "fibo: " << fibo1_do(5) << std::endl;
    });
    sleepFor(1);
    c.cancel();
    sleepFor(1);
    c.cancel();
    waitForAll();
}
*/

CPPUT_TEST_MAIN
