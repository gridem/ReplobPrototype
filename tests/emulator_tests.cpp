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

void addLocalNodePort(NodeId id, Port port)
{
    JLOG("add node " << id << " with port " << port);
    single<Nodes>().add(id, Endpoint{port, "127.0.0.1", Endpoint::Type::V4});
}

using ResponsesCount = std::unordered_map<NodeId, int>;
using OptDoer = boost::optional<Doer>;

struct NullListener
{
    NullListener(NodeId id, Port port)
    {
        JLOG("listen port " << port << " for node " << id);
        listener_.listen(port, [id](Socket& s) {
            JLOG("accepted for node " << id);
            while (true)
            {
                size_t sz = -1;
                s.read(podToView(sz));
                VERIFY(sz < 10 * 1024 * 1024, "Invalid message size");
                Buffer buf(sz);
                s.read(bufToView(buf));
                JLOG("+1 message for " << id);
                ++ single<ResponsesCount>()[id];
                single<OptDoer>().value().done();
            }
        });
    }

    ~NullListener()
    {
        listener_.cancel();
    }

private:
    Listener listener_;
};

struct EmuNodes
{
    void add(NodeId id, Port port)
    {
        addLocalNodePort(id, port);
        if (id != 1) // only add for emulated nodes
            listeners_.emplace_back(new NullListener{id, port});
    }

private:
    std::vector<std::unique_ptr<NullListener>> listeners_;
};

void emulate(Handler h, int nodes = 3)
{
    ThreadPool tp(1, "net");
    scheduler<DefaultTag>().attach(tp);
    service<NetworkTag>().attach(tp);
    service<TimeoutTag>().attach(tp);

    single<NodesConfig>().setThisNode(1);

    MCleanup {
        cleanupAll();
        single<ResponsesCount>().clear();
        single<OptDoer>() = boost::none;
    };

    bool isDone = false;

    Goer parent;
    parent = go([h, nodes, &isDone, &parent] {
        EmuNodes emus;
        for (int i = 1; i <= nodes; ++ i)
            emus.add(i, 8900 + i);

        Goer g = go([h, &isDone, &parent] {
            h();
            isDone = true;
            parent.cancel();
            cleanupAll();
        });
        sleepa(1000);
        g.cancel();
        cleanupAll();
    });
    waitForAll();
    VERIFY(isDone, "Test failed");
}

Replob& replob()
{
    return single<Replob>();
}

void waitUntil(const ResponsesCount& count)
{
    JLOG("wait until");
    while (single<ResponsesCount>() != count)
        waitForDone();
    JLOG("wait until done");
    single<ResponsesCount>().clear();
}

TEST(Emulator, 1)
{
    emulate([] {
        single<OptDoer>().emplace();
        bool applied = false;
        auto msg = [&] { applied = true; };
        CarryMsg carryMsg{msg, 0, genUniqueId()};
        replob().vote(carryMsg, 1);
        waitUntil({{2, 1}, {3, 1}});
        replob().vote(carryMsg, 2);
        replob().vote(carryMsg, 3);
        waitUntil({{2, 1}, {3, 1}});
        VERIFY(applied, "the message wasn't commited");
    }, 3);
}

CPPUT_TEST_MAIN
