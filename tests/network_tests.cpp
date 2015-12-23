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

#include <synca/synca.h>
#include <synca/log.h>

using namespace synca;

/*
TEST(Network, 1)
{
    ThreadPool tp(threadConcurrency(), "net");
    scheduler<DefaultTag>().attach(tp);
    service<NetworkTag>().attach(tp);

    go([] {
        Socket s;
        JLOG("connecting");
        s.connect(Endpoint{8080, "127.0.0.1", Endpoint::Type::V4});
        JLOG("sock conn");
    });
    go([] {
        Acceptor a{8080};
        a.goAccept([](Socket& s) {
            JLOG("connected");
        });
    });
    waitForAll();
}

TEST(Network, SimpleListen)
{
    ThreadPool tp(threadConcurrency(), "net");
    scheduler<DefaultTag>().attach(tp);
    service<NetworkTag>().attach(tp);

    Listener l;
    l.listen(8800, [](Socket& s) {
        int v = -1;
        s.read(podToView(v));
        JLOG("val: " << v);
    });
    go([&] {
        Connector cn{Endpoint{8800, "127.0.0.1", Endpoint::Type::V4}};
        int v = 10;
        cn.write(podToView(v));
        JLOG("written");
        l.cancel();
    });
    waitForAll();
}
*/

/*
TEST(Network, ListenWriteFail)
{
    ThreadPool tp(threadConcurrency(), "net");
    scheduler<DefaultTag>().attach(tp);
    service<NetworkTag>().attach(tp);
    service<TimeoutTag>().attach(tp);

    Listener l;
    l.listen(8800, [](Socket&) {});
    go([&] {
        Connector cn{Endpoint{8800, "127.0.0.1", Endpoint::Type::V4}};
        for (int i = 0; i < 10; ++ i)
            cn.write(podToView(i));
        l.cancel();
    });
    waitForAll();
}
*/

TEST(Network, NodeDisconnect)
{
    ThreadPool tp(1, "net");
    scheduler<DefaultTag>().attach(tp);
    service<NetworkTag>().attach(tp);
    service<TimeoutTag>().attach(tp);

    single<NodesConfig>().addNode(1, Endpoint{8800, "127.0.0.1", Endpoint::Type::V4});
    go([&] {
        single<Nodes>().send(1, []{});
    });
    go([&] {
        single<Nodes>().send(1, []{});
    });
    go([&] {
        sleepa(2000);
        single<Nodes>().remove(1);
    });
    waitForAll();
}

CPPUT_TEST_MAIN
