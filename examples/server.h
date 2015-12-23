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

#include <cstdlib>

const char* const c_nodes = "NODES";
const char* const c_nodeId = "NODE_ID";

std::string getEnv(const char* name)
{
    return std::getenv(name);
}

int getEnvInt(const char* name)
{
    return std::stoi(getEnv(name));
}

using namespace synca;

namespace synca {
void dumpStats();
}

struct Server
{
    static void serve(Handler h)
    {
        Server s;
        s.start();
        h();
        s.shutdown();
        waitForAll();
    }

    void shutdown()
    {
        cleanupAll();
    }

    void start()
    {
        scheduler<DefaultTag>().attach(tp);
        service<NetworkTag>().attach(tp);
        service<TimeoutTag>().attach(tp);

        initNodes();
        listener_.listen();
    }

private:
    void addLocalNodePort(NodeId id, Port port)
    {
        JLOG("add node " << id << " with port " << port);
        single<Nodes>().add(id, Endpoint{port, "127.0.0.1", Endpoint::Type::V4});
    }

    void initNodes()
    {
        int nodes = getEnvInt(c_nodes);
        VERIFY(nodes > 0, "Invalid number of nodes");
        VERIFY(nodes <= 20, "Number of nodes is too high");

        NodeId nodeId = getEnvInt(c_nodeId);
        VERIFY(nodeId > 0, "Invalid node id");
        VERIFY(nodeId <= nodes, "Node id not in correct range");

        RLOG("starting service with node " << nodeId << " of " << nodes);
        for (int i = 1; i <= nodes; ++ i)
            addLocalNodePort(i, i + 8800);
        single<NodesConfig>().setThisNode(nodeId);
    }

    ThreadPool tp{1};
    MsgListener listener_;
};

