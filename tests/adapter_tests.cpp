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

#include "ut.h"

#include <synca/adapter.h>
#include <synca/log.h>

#include <boost/preprocessor/stringize.hpp>

template<typename T, typename T_mutex, typename T_value = T>
using MutexAdapter = Adapter<T, BaseLocker<BaseValue<T_value>, T_mutex>>;

template<typename T, typename T_mutex, typename T_ref = T>
using MutexAdapterRef = Adapter<T, BaseLocker<BaseRef<T_ref>, T_mutex>>;

template<typename T, typename T_mutex, typename T_ref>
auto mutexAdapterRef(T_ref& t)
{
    return MutexAdapterRef<T, T_mutex, T_ref>(t);
}

struct MyMutex
{
    void lock()
    {
        LOG("lock");
    }

    void unlock()
    {
        LOG("unlock");
    }
};

struct I
{
    virtual ~I() {}
    virtual void f(int) = 0;
};

struct X : I
{
    void f(int v)
    {
        x = v;
        LOG("v: " << v);
    }

    int get()
    {
        return x;
    }

    int x;
};

DECL_ADAPTER(X, f, get)

/*
template<typename T_base> struct Adapter<X, T_base> : private T_base
{
    template<typename... V> Adapter(V&&... v) : T_base{std::forward<V>(v)...} {}

    template<typename... V>
    auto f(V&&... v)
    {
        return this->call([](auto& t, V&&... v) {
            return t.f(std::forward<decltype(v)>(v)...);
        }, std::forward<V>(v)...);
    }
};*/

struct Y
{
    void f()
    {
        LOG("f()");
    }
    void f() const
    {
        LOG("f() const");
    }
};

template<typename T, typename T_base>
struct AdapterTest;

template<typename T_base>
struct AdapterTest<X, T_base> : T_base
{
    template<typename... V>
    AdapterTest(V&&... v) : T_base{std::forward<V>(v)...} {}

    template<typename... V>
    auto f(V&&... v)
    {
        return this->call([](auto& t, V&&... v) {
            return t.f(std::forward<decltype(v)>(v)...);
        }, std::forward<V>(v)...);
    }
};

template<typename T_base>
struct AdapterTest<Y, T_base>
{
    template<typename V>
    auto f(V&& v)
    {
        return [](auto& t) {
            return t.f();
        };
    }

    template<typename V>
    auto const_f(V&& v)
    {
        return [](const auto& t) {
            return t.f();
        };
    }

    template<typename V>
    auto f2(V&& v)
    {
        return [](auto& t) {
            return t.f();
        };
    }
};

TEST(Adapter, 1)
{
    MutexAdapter<X, MyMutex> a;
    a.f(2);
    X x;
    Adapter<X, BaseLocker<BaseRef<I>, MyMutex>> a2(x);
    a2.f(5);
    LOG("x: " << x.x);
    auto a3 = mutexAdapterRef<X, MyMutex>(a);
    int val = a3.get();
    LOG("val: " << val);
    a3.f(1);
    val = a3.get();
    LOG("val: " << val);
}

TEST(Adapter, str)
{
    std::cout << BOOST_PP_STRINGIZE((DECL_ADAPTER(X, f))) << std::endl;
}

TEST(Adapter, Y)
{
    AdapterTest<Y, Y> a;
    auto f1 = a.f(2);
    auto f2 = a.const_f(2);
    auto f3 = a.f2(2);
    LOG("f1: " << (void*)&f1 << ", " << typeid(decltype(f1)).name());
    LOG("f2: " << (void*)&f2 << ", " << typeid(decltype(f2)).name());
    LOG("f3: " << (void*)&f3 << ", " << typeid(decltype(f3)).name());
}

CPPUT_TEST_MAIN
