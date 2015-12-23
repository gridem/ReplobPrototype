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

TEST(teleport, 1)
{
    ThreadPool tp1(1, "tp1");
    ThreadPool tp2(1, "tp2");
    scheduler<DefaultTag>().attach(tp1);

    go([&] {
        ASSERT_EQ("tp1", name());
        JLOG("TELEPORT 1");
        teleport(tp2);
        ASSERT_EQ("tp2", name());
        JLOG("TELEPORT 2");
    });
    waitForAll();
}

TEST(teleport, 2)
{
    ThreadPool tp1(3, "tp1");
    ThreadPool tp2(2, "tp2");
    scheduler<DefaultTag>().attach(tp1);

    go([&] {
        ASSERT_EQ("tp1", name());
        JLOG("TELEPORT 1");
        teleport(tp2);
        ASSERT_EQ("tp2", name());
        JLOG("TELEPORT 2");
        teleport(tp1);
        ASSERT_EQ("tp1", name());
        JLOG("TELEPORT 3");
        teleport(tp1);
        ASSERT_EQ("tp1", name());
        JLOG("TELEPORT 4");
    });
    waitForAll();
}

CPPUT_TEST_MAIN
