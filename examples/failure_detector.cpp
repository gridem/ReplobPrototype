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

#include "server.h"

using namespace std::chrono_literals;

using Clock = std::chrono::steady_clock;
using TimeStamp = Clock::time_point;

#define DLOG(D_msg)         JLOG("DETECTOR: " << D_msg)

struct Detector : WithCleanup
{
    Detector()
    {
        auto now = Clock::now();
        for (NodeId n: single<NodesConfig>().otherNodes())
        {
            nodesTs[n] = now;
        }
    }

    void start()
    {
        timer.reset(1000, [this] {
            pulse();
        });
    }

    void pulse()
    {
        DLOG("pulse");
        MCleanup {
            start();
        };
        NodeId id = thisNode();
        broadcast([id] {
            single<Detector>().update(id);
        });
        check();
    }

    void update(NodeId id)
    {
        DLOG("update node: " << id);
        nodesTs[id] = Clock::now();
    }

    void check()
    {
        auto now = Clock::now();
        std::vector<NodeId> toRemove;
        for (auto&& n: nodesTs)
        {
            auto&& id = n.first;
            auto&& ts = n.second;
            if (now - ts > 2s)
                toRemove.push_back(id);
        }
        for (NodeId n: toRemove)
            removeNode(n);
    }

    void removeNode(NodeId id)
    {
        DLOG("removing node: " << id);
        nodesTs.erase(id);
        replobApply([id] {
            // rescheduling is not allowed inside transaction!
            // thus we need to do it in async way
            // TODO: add MAsync
            go([id] {
                single<Detector>().removeNodeComplete(id);
            });
        });
    }

    void removeNodeComplete(NodeId id)
    {
        JLOG("completing removing node: " << id);
        single<Nodes>().remove(id);
        single<NodesConfig>().removeNode(id);
    }

private:
    void cleanup() override
    {
        timer.reset();
    }

    Timer timer;
    std::unordered_map<NodeId, TimeStamp> nodesTs;
};

void test()
{
    go ([] {
        NodeId src = thisNode();
        MReplobTransact {
            JLOG("@" << src);
        };
    });
    sleepFor(1000);
}

void detector()
{
    go ([] {
        single<Detector>().start();
    });
    sleepFor(60 * 1000);
}

int main(int argc, const char* argv[])
{
    try
    {
        Server::serve(detector);
    }
    catch (std::exception& e)
    {
        RLOG("Error: " << e.what());
        return 1;
    }
    return 0;
}
