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

#include <synca/synca2.h>
#include <synca/log.h>

#include "ut.h"

using namespace synca;

template<int N>
struct XN
{
    bool on0 = false;
    int on1 = -1;

    void on()
    {
        on0 = true;
    }

    void on(int v)
    {
        on1 = v;
    }
};

template<int N>
struct TestPhantom : IPhantom
{
    ~TestPhantom()
    {
        if (destroyed)
            destroyed->push_back(v);
    }

    void on(int a)
    {
        v = a;
    }

    int v = 0;

    std::vector<int>* destroyed = nullptr;
};

/*
struct ThisScheduler : IScheduler
{
    void schedule(Handler handler) override
    {
        handler();
    }
};
*/

TEST(phantom, 1)
{
    /*
    ThisScheduler s;
    scheduler<DefaultTag>().attach(s);
    MCleanup {
        cleanupAll();
    };
    */
    using P = TestPhantom<1>;
    auto& t1 = createPhantom<P>();
    ASSERT_EQ(t1.v, 0);
    t1.v = 1;
    auto& t2 = createPhantom<P>();
    ASSERT_EQ(t2.v, 0);
    t2.v = 2;

    ASSERT_EQ(phantom<P>(0).v, 1);
    ASSERT_EQ(phantom<P>(1).v, 2);
    ASSERT_EQ(phantom<P>(2).v, 0);
}

TEST(phantom, 2)
{
    using P = TestPhantom<2>;

    auto& p = phantom<P>(2);
    ASSERT_EQ(p.v, 0);
    p.v = 2;
    createPhantom<P>().v = 3;

    ASSERT_EQ(phantom<P>(0).v, 0);
    ASSERT_EQ(phantom<P>(1).v, 0);
    ASSERT_EQ(phantom<P>(2).v, 2);
    ASSERT_EQ(phantom<P>(3).v, 3);
    ASSERT_EQ(phantom<P>(4).v, 0);
}

TEST(phantom, destroy1)
{
    using P = TestPhantom<3>;

    auto& p = createPhantom<P>();
    ASSERT_EQ(p.v, 0);
    p.v = 10;
    std::vector<int> destroyed;
    p.destroyed = &destroyed;
    ASSERT_TRUE(destroyed == std::vector<int>{});
    p.destroy();
    ASSERT_TRUE(destroyed == std::vector<int>{10});
}

TEST(phantom, destroy2)
{
    using P = TestPhantom<4>;

    auto& p = phantom<P>(0);
    ASSERT_EQ(p.v, 0);
    p.v = 10;
    std::vector<int> destroyed;
    p.destroyed = &destroyed;
    ASSERT_TRUE(destroyed == std::vector<int>{});
    ASSERT_NEQ(0, phantomPtr<P>(0));
    p.destroy();
    ASSERT_TRUE(destroyed == std::vector<int>{10});
    ASSERT_EQ(0, phantomPtr<P>(0));
}

TEST(phantom, destroyGroup)
{
    using P = TestPhantom<5>;

    std::vector<int> destroyed;

    auto& p0 = phantom<P>(0);
    p0.v = 10;
    p0.destroyed = &destroyed;

    auto& p1 = phantom<P>(1);
    p1.v = 11;
    p1.destroyed = &destroyed;

    p1.destroy();
    ASSERT_TRUE(destroyed == std::vector<int>{});

    p0.destroy();
    ASSERT_TRUE((destroyed == std::vector<int>{10, 11}));

    ASSERT_EQ(0, phantomPtr<P>(0));
    ASSERT_EQ(0, phantomPtr<P>(1));
}

TEST(phantom, destroyGroup2)
{
    using P = TestPhantom<6>;

    std::vector<int> destroyed;

    auto& p0 = phantom<P>(0);
    p0.v = 10;
    p0.destroyed = &destroyed;

    auto& p1 = phantom<P>(1);
    p1.v = 11;
    p1.destroyed = &destroyed;

    auto& p2 = phantom<P>(2);
    p2.destroyed = &destroyed;
    p2.destroy();
    p1.destroy();
    ASSERT_TRUE(destroyed == std::vector<int>{});

    p0.destroy();
    ASSERT_TRUE((destroyed == std::vector<int>{10, 11, 0}));

    ASSERT_EQ(0, phantomPtr<P>(0));
    ASSERT_EQ(0, phantomPtr<P>(1));
}

TEST(adapter, objector)
{
    using X = XN<1>;

    Objector<X> x;
    Handler v = x.on(2);
    ASSERT_FALSE(single<X>().on0);
    ASSERT_EQ(-1, single<X>().on1);
    v();
    ASSERT_FALSE(single<X>().on0);
    ASSERT_EQ(2, single<X>().on1);
    x.on()();
    ASSERT_TRUE(single<X>().on0);
    ASSERT_EQ(2, single<X>().on1);
}

TEST(adapter, objectorInvoke)
{
    using X = XN<2>;

    Adapter<X, BaseObjector<X, BaseInvoker>> x;
    ASSERT_FALSE(single<X>().on0);
    ASSERT_EQ(-1, single<X>().on1);
    x.on(2);
    ASSERT_FALSE(single<X>().on0);
    ASSERT_EQ(2, single<X>().on1);
}

/*
TEST(adapter, phantomer)
{
    using P = TestPhantom<7>;
    auto& p = createPhantom<P>();
    auto b = broadcastPhantom(p);
    ASSERT_EQ(0, p.v);
    auto c = b.on(2);
    ASSERT_EQ(0, p.v);
    c();
    ASSERT_EQ(2, p.v);
}
*/

CPPUT_TEST_MAIN
