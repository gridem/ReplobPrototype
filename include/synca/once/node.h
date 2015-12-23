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

namespace synca {

DECL_EXC(NodeError, Error, "Node error")

using id_t = uint64_t;
using NodeId = id_t;
using MessageId = id_t;

template<typename T_tag>
id_t nextId()
{
    static Atomic<id_t> id;
    return id.fetch_add(1, std::memory_order_relaxed);
}

/*
struct AnyMsg
{
    template<typename F>
    AnyMsg(F f) : msg{std::move(f)} {}

    MessageId id = nextId<struct MID>();
    Handler msg;
};
*/

using AnyMsg = Handler; // gotcha!

using NodesSet = std::set<NodeId>;

// TODO: add groupId to broadcast message to corresponding group
// one can broadcast to any group (special id == 0)
struct NodesConfig
{
    void addNode(NodeId id, Endpoint);
    bool removeNode(NodeId id);
    Endpoint getInfo(NodeId id) const;
    std::vector<NodeId> nodes() const;
    NodesSet nodesSet() const;
    size_t nodesCount() const;
    std::vector<NodeId> otherNodes() const;
    void setThisNode(NodeId id);
    NodeId thisNode() const;

private:
    NodeId thisNode_;
    std::unordered_map<NodeId, Endpoint> nodes_;
};

struct Nodes : WithCleanup
{
    size_t count() const;
    void add(NodeId id, Endpoint);
    void send(NodeId id, View view);
    bool remove(NodeId id); // returns false if the node has been removed already
    void cleanup() override;

private:
    using SharedConnector = std::shared_ptr<Connector>;

    SharedConnector get(NodeId id);

    std::unordered_map<NodeId, SharedConnector> nodes_;
};

struct MsgListener : WithCleanup
{
    ~MsgListener();

    void listen();
    void cancel();

private:
    void cleanup() override;

    Listener listener_;
};

struct PairHash
{
    template <typename T, typename U>
    std::size_t operator()(const std::pair<T, U>& x) const
    {
        return std::hash<T>()(x.first) ^ std::hash<U>()(x.second);
    }
};

using UniqueId = std::pair<NodeId, MessageId>;

UniqueId genUniqueId();

NodeId thisNode();
void send(NodeId dst, AnyMsg msg);
void broadcast(AnyMsg msg);

}
