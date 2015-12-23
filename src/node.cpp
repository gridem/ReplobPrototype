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

#include "synca_impl.h"

namespace synca {

void NodesConfig::addNode(NodeId id, Endpoint e)
{
    nodes_.insert({id, e});
}

bool NodesConfig::removeNode(NodeId id)
{
    return nodes_.erase(id) != 0;
}

Endpoint NodesConfig::getInfo(NodeId id) const
{
    return nodes_.at(id);
}

std::vector<NodeId> NodesConfig::nodes() const
{
    std::vector<NodeId> res;
    for (auto&& n: nodes_)
        res.emplace_back(n.first);
    return res;
}

NodesSet NodesConfig::nodesSet() const
{
    NodesSet res;
    for (auto&& n: nodes_)
        res.insert(n.first);
    return res;
}

size_t NodesConfig::nodesCount() const
{
    return nodes_.size();
}

std::vector<NodeId> NodesConfig::otherNodes() const
{
    std::vector<NodeId> res;
    for (auto&& n: nodes_)
    {
        if (n.first != thisNode_)
            res.emplace_back(n.first);
    }
    return res;
}

void NodesConfig::setThisNode(NodeId id)
{
    thisNode_ = id;
}

NodeId NodesConfig::thisNode() const
{
    return thisNode_;
}

size_t Nodes::count() const
{
    return nodes_.size();
}

void Nodes::add(NodeId id, Endpoint e)
{
    single<NodesConfig>().addNode(id, e);
    nodes_[id] = std::make_shared<Connector>(e);
}

void Nodes::send(NodeId id, View view)
{
    /*
    auto buf = serializeToPacket(msg);
    showBuffer(buf);
    get(id)->write(bufToView(buf));
    */
    get(id)->write(view);
}

bool Nodes::remove(NodeId id)
{
    JLOG("removing node: " << id);
    auto it = nodes_.find(id);
    if (it == nodes_.end() || !it->second)
        return false;
    if (!single<NodesConfig>().removeNode(id))
        return false;
    JLOG("disconnecting node: " << id);
    it->second->disconnect();
    return true;
    // FIXME: cannot remove due to write and possible reconnect.
    // solution: remove from nodes config and fix the logic for Nodes::get to avoid shared_ptr recreation
    //nodes_.erase(it);
}

// TODO: implement class that disconnects automatically
void Nodes::cleanup()
{
    JLOG("disconnecting all nodes");
    for (auto&& ns: nodes_)
    {
        auto& n = ns.second;
        if (n)
        {
            JLOG("disconnecting node: " << ns.first);
            n->disconnect();
            n.reset();
        }
    }
}

Nodes::SharedConnector Nodes::get(NodeId id)
{
    auto it = nodes_.find(id);
    if (it == nodes_.end())
    {
        throw NodeError("Node is absent");
    }
    if (!it->second)
    {
        throw NodeError("Node has been disconnected");
    }
    return it->second;
}

MsgListener::~MsgListener()
{
    cancel();
}

void MsgListener::listen()
{
    listener_.listen(single<NodesConfig>().getInfo(single<NodesConfig>().thisNode()).port,
                     [](Socket& s) {
        while (true)
        {
            size_t sz = -1;
            s.read(podToView(sz));
            VERIFY(sz < 10 * 1024 * 1024, "Invalid message size");
            JLOG("received packet for " << sz << "B");
            Buffer buf(sz);
            s.read(bufToView(buf));
            JLOG("invoking handler, read buffer " << sz << "B");
            //showBuffer(buf);
            deserialize<AnyMsg>(buf)(); // invoke in-place
            JLOG("invoked any msg handler");
        }
    });
}

void MsgListener::cancel()
{
    listener_.cancel();
}

void MsgListener::cleanup()
{
    cancel();
}

UniqueId genUniqueId()
{
    return {single<NodesConfig>().thisNode(), nextId<struct MID>()};
}

NodeId thisNode()
{
    return single<NodesConfig>().thisNode();
}

void send(NodeId dst, AnyMsg msg)
{
    auto buf = serializeToPacket(msg);
    single<Nodes>().send(dst, bufToView(buf));
}

// TODO: add broadcast involving local node
void broadcast(AnyMsg msg)
{
    auto buf = serializeToPacket(msg);
    for (NodeId n: single<NodesConfig>().otherNodes())
    {
        JLOG("async broadcasting message to node: " << n);
        go([n, buf]() mutable {
            JLOG("broadcasting message to node: " << n);
            single<Nodes>().send(n, bufToView(buf));
        });
    }
    //msg();
}

}

