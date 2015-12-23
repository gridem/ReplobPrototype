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

namespace synca {

#define CONCAT_(D1, D2)     D1 ## D2
#define CONCAT(D1, D2)      CONCAT_(D1, D2)
#define UNIQUE_ID           CONCAT(id__, __COUNTER__)

#define MCleanup            auto UNIQUE_ID = CleanupStart() + [&]

template<typename F>
struct CleanupGuard : DisableCopy
{
    CleanupGuard(F f) : f_{f} {}
    ~CleanupGuard() { f_(); }
private:
    F f_;
};

struct CleanupStart
{
    template<typename F>
    auto operator+(F f)
    {
        return CleanupGuard<F>{f};
    }
};

}
