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

#ifdef flagGCC_LIKE
#include <cxxabi.h>
#endif

#include "ut.h"

void f() noexcept
{
    //throw 1;
}

TEST(exception, noexceptTest)
{
    try
    {
        f();
    }
    catch(int)
    {

    }
}

#ifdef flagGCC_LIKE
namespace X{
struct A {};
}

std::string demangleImpl(const char* name)
{
    int status;
    char* demangled = abi::__cxa_demangle(name, 0, 0, &status);
    std::string result = demangled;
    free(demangled);
    return result;
}

TEST(demange, type)
{
    const char* n = typeid(X::A).name();
    std::cout << "mangled: " << n << std::endl;
    std::cout << "demangled: " << demangleImpl(n) << std::endl;
}

#endif

struct Y
{
    template<typename T>
    static void fn()
    {
        std::cout << "type: " << typeid(T).name() << std::endl;
    }

    auto test()
    {
        return [] {
            fn<double>();
        };
    }
};

TEST(transact, query)
{
    auto v = []
    {
        Y::fn<int>();
    };
    v();
}

TEST(transact, query2)
{
    auto v = Y().test();
    v();
}

CPPUT_TEST_MAIN
