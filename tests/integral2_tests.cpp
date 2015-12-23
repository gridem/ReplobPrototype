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

using namespace synca;

namespace synca {
void dumpStats();
}

void addLocalNodePort(NodeId id, Port port)
{
    JLOG("add node " << id << " with port " << port);
    single<Nodes>().add(id, Endpoint{port, "127.0.0.1", Endpoint::Type::V4});
}

void replobTest()
{
    if (thisNode() != 1)
        return;
    go([] {
        replobApply([] {
            RJLOG("--- APPLIED from node #1");
            if (thisNode() != 2)
                return;
            replobApply([] {
                RJLOG("--- APPLIED from node #2");
            });
        });
    });
}

int g_counter = 0;

void applyCounterAsync()
{
    go([] {
        replobApply([] {
            ++ g_counter;
            if (thisNode() == 1)
                applyCounterAsync();
        });
    });
}

void perfTest()
{
    if (thisNode() == 1)
        for (int i = 0; i < 10; ++ i)
            applyCounterAsync();
    for (int i = 0; i < 10; ++ i)
    {
        sleepFor(1000);
        RLOG("Counter: " << g_counter);
        g_counter = 0;
    }
}

void consistencyTest()
{
    using L = std::vector<int>;
    static int val = 0;

    if (thisNode() <= 3)
    {
        for (int i = 0; i < 10; ++ i)
        go([] {
            int v = ++val * 100 + thisNode();
            JLOG("replobing: " << v);
            replobApply([v] {
                JLOG("push back: " << v);
                single<L>().push_back(v);
            });
        });
    }
    sleepFor(2000);
    int i = 0;
    RJLOG("=== output array");
    for (int v: single<L>())
    {
        RJLOG("l[" << i++ << "] = " << v);
    }
}

void timeoutTest()
{
    using L = std::vector<int>;
    static int val = 0;

    if (thisNode() <= 3)
    {
        for (int i = 0; i < 10; ++ i)
        go([] {
            int v = ++val * 100 + thisNode();
            if (v == 503)
                sleepFor(1000);
            JLOG("replobing: " << v);
            replobApply([v] {
                JLOG("push back: " << v);
                single<L>().push_back(v);
            });
        });
    }
    sleepFor(2000);
    int i = 0;
    RJLOG("=== output array");
    for (int v: single<L>())
    {
        RJLOG("l[" << i++ << "] = " << v);
    }
}

struct XReplob
{
    void on(const std::string& str, int val)
    {
        RJLOG("--- XReplob executed: " << str << ", " << val);
    }

    int on() const
    {
        RJLOG("--- XReplob const");
        return 10;
    }

    int inc()
    {
        return ++ val;
    }

    int val = 0;
};

void modifierTest()
{
    if (thisNode() != 1)
        return;
    go([] {
        int v = MReplobTransact {
            RJLOG("--- MReplobTransact from node #1");
            return 10;
        };
        RJLOG("--- APPLIED completed with value: " << v);
        int v2 = MReplobTransactLocal {
            RJLOG("--- MReplobTransactLocal from node #1");
            return 100;
        };
        RJLOG("--- MReplobTransactLocal completed with value: " << v2);
        replob<XReplob>().on(std::string("hello"), 2);
        replobLocal<XReplob>().on();
        MReplobTransactInstance(XReplob) {
            $.on("instance", $.on());
        };
        MReplobTransactLocalInstance(XReplob) {
            int v = $.on();
            RJLOG("--- local const method returned: " << v);
        };

        v = MReplobTransact {
            return $.instance<XReplob>().inc();
        };
        RJLOG("--- v: " << v);
        v = MReplobTransact {
            return $.instance<XReplob>().inc();
        };
        RJLOG("--- v: " << v);
        MReplobTransactLocal {
            int v = $.instance<XReplob>().val;
            RJLOG("--- transact local v: " << v);
        };
        MReplobTransact {
            int v = $.instance<XReplob>().val;
            RJLOG("--- transact v: " << v);
        };
        v = MReplobTransactInstance(XReplob) {
            return $.inc();
        };
        RJLOG("--- transact instance value: " << v);
    });
}

auto tests = {&replobTest, &perfTest, &consistencyTest, &timeoutTest, &modifierTest};

int main(int argc, const char* argv[])
{
    try
    {
        RLOG("address: " << (void*)&main);

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
        dumpStats();
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

