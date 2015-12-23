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

using namespace synca;

void f(){}

void addLocalNodePort(NodeId id, Port port)
{
    JLOG("add node " << id << " with port " << port);
    single<Nodes>().add(id, Endpoint{port, "127.0.0.1", Endpoint::Type::V4});
}

void broadcastTest()
{
    if (thisNode() == 1)
    {
        go([] {
            JLOG("broadcasting");
            broadcast([] {
                RJLOG("----- hello from node: " << thisNode());
            });
        });
    }
}

void replobTest()
{
    if (thisNode() != 1)
        return;
    go([] {
        single<Replob>().apply([] {
            RJLOG("--- commited for: " << thisNode());
            if (thisNode() != 2)
                return;
            single<Replob>().apply([] {
                RJLOG("--- commited from node 2 for: " << thisNode());
            });
        });
    });
}

int counter = 0;

void applyCounterAsync()
{
    go([] {
        single<Replob>().apply([] {
            ++ counter;
            if (thisNode() == 1)
            {
                applyCounterAsync();
            }
        });
    });
}

void perfTest()
{
    if (thisNode() == 1)
    {
        applyCounterAsync();
        applyCounterAsync();
        applyCounterAsync();
        applyCounterAsync();
        applyCounterAsync();
        applyCounterAsync();
        applyCounterAsync();
        applyCounterAsync();
        applyCounterAsync();
        applyCounterAsync();
        applyCounterAsync();
        applyCounterAsync();
    }
    sleepFor(1000);
    RLOG("Counter: " << counter);
}

void consistencyTest()
{
    using L = std::vector<int>;
    static int val = 0;

    if (thisNode() <= 3)
    {
        for (int i = 0; i < 10; ++ i)
        go([] {
            NodeId src = thisNode();
            int v = val++;
            if (src == 3 && v == 5)
                sleepFor(1000);
            single<Replob>().apply([src, v] {
                single<L>().push_back(int(v*10 + src));
            });
        });
    }
    sleepFor(1000);
    int i = 0;
    //VERIFY(single<L>().size() == 1000, "Invalid size");
    for (int v: single<L>())
    {
        RJLOG("l[" << i++ << "] = " << v);
        //VERIFY(v == i++, "Invalid value");
    }
}

auto tests = {&broadcastTest, &replobTest, &perfTest, &consistencyTest};

int main(int argc, const char* argv[])
{
    try
    {
        RLOG("address: " << (void*)&f);

        VERIFY(argc == 4, "Invalid args");
        NodeId id = std::stoi(argv[1]);
        int nodes = std::stoi(argv[2]);
        size_t nTest = std::stoi(argv[3]);
        VERIFY(nTest < tests.size(), "Invalid test number");
        RLOG("starting service with node " << id << " of " << nodes << ", test: " << nTest);
        ThreadPool tp(1, "net");
        scheduler<DefaultTag>().attach(tp);
        service<NetworkTag>().attach(tp);
        service<TimeoutTag>().attach(tp);

        single<NodesConfig>().setThisNode(id);
        MCleanup {
            cleanupAll();
        };
        for (int i = 1; i <= nodes; ++ i)
            addLocalNodePort(i, 8800 + i);

        MsgListener msg;
        msg.listen();

        tests.begin()[nTest]();

        sleepFor(2000);
        //single<Nodes>().cleanup();
        //msg.cancel();
        //waitForAll();
    }
    catch (std::exception& e)
    {
        RLOG("Error: " << e.what());
        return 1;
    }
    return 0;
}

