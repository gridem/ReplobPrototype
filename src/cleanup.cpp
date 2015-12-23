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

#include "synca_impl.h"

namespace synca {

struct Cleanuper
{
    void add(Handler clean)
    {
        cleanupers_.push_back(clean);
    }

    void cleanup()
    {
        for (auto&& c: cleanupers_)
            c();
    }

private:
    std::vector<Handler> cleanupers_;
};

WithCleanup::WithCleanup()
{
    single<Cleanuper>().add([this] {
        cleanup();
    });
}

void cleanupAll()
{
    go([] {
        RJLOG("cleanuping all objects");
        single<Cleanuper>().cleanup();
        RJLOG("cleanup completed");
    });
}

}

