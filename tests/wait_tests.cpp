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

/*
TEST(wait, 1)
{
    ThreadPool tp(3, "wait");
    scheduler<DefaultTag>().attach(tp);
    go([] {
        goWait({
            [] {
                JLOG("+300");
                sleepFor(300);
                JLOG("-300");
            }, [] {
                JLOG("+500");
                sleepFor(500);
                JLOG("-500");
            }
        });
    });
    waitForAll();
}
*/

TEST(wait, 2)
{
    ThreadPool tp(3, "wait");
    scheduler<DefaultTag>().attach(tp);
    go([] {
        Awaiter w;
        w.wait();
        w.go([] {
            JLOG("+300");
            sleepFor(300);
            JLOG("-300");
        }).go([] {
            JLOG("+500");
            sleepFor(500);
            JLOG("-500");
        });
        JLOG("before wait");
        w.wait();
        JLOG("after wait");
        w.wait();
    });
    waitForAll();
}

CPPUT_TEST_MAIN
