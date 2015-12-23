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

#include "synca_impl2.h"

#define ELOG(D_msg)         JLOG("replob " << nextStepId_ << ":" << commitingStepId_ << ": " << D_msg)
#define RELOG(D_msg)        JLOG("replob " << nextStepId_ << ":" << commitingStepId_ << " " << msg << ": " << D_msg)
#define RRELOG(D_msg)       RJLOG("replob " << nextStepId_ << ":" << commitingStepId_ << " " << msg << ": " << D_msg)

namespace synca {

constexpr int c_availabilityTimeoutMs = 1000/5;

std::ostream& operator<<(std::ostream& o, CarryMsg msg)
{
    return o << "[" << msg.stepId << "]{" << msg.msgId.first << ":" << msg.msgId.second << "}";
}

template<typename T>
std::ostream& operator<<(std::ostream& o, const std::vector<T>& v)
{
    o << "{";
    bool rest = false;
    for (auto&& t: v)
    {
        if (rest)
            o << ", ";
        else
            rest = true;
        o << t;
    }
    return o << "}";
}

void Replob::apply(AnyMsg msg)
{
    CarryMsg carryMsg{msg, nextStepId(), genUniqueId()};
    // TODO: implement adapter + template broadcast call
    vote(carryMsg, thisNode());
}

size_t Replob::quorumSize()
{
    return single<Nodes>().count()/2 + 1;
}

void Replob::cleanup()
{
    steps_.clear();
    commitingStepId_ = std::numeric_limits<StepId>::max();
}

Replob::StepData* Replob::getStep(StepId id)
{
    if (id < commitingStepId_)
        return nullptr;
    auto& data = steps_[id];
    if (data.state == StepState::Completed)
        return nullptr;
    updateCurrentStepId(id);
    return &data;
}

StepId Replob::nextStepId()
{
    return nextStepId_++;
}

void Replob::updateCurrentStepId(StepId id)
{
    if (nextStepId_ <= id)
        nextStepId_ = id + 1;
}

void Replob::vote(const CarryMsg &msg, NodeId voted)
{
    RELOG("vote: for node " << voted);
    StepData* stepData = getStep(msg.stepId);
    if (stepData == nullptr)
    {
        RELOG("vote: outdated msg");
        return;
    }
    MsgData& msgData = stepData->getMsg(msg);
    msgData.votedSet.insert(voted);
    bool canVote = stepData->state == StepState::Initial;
    if (canVote)
    {
        RELOG("voting");
        stepData->state = StepState::Voted;
        msgData.votedSet.insert(thisNode());
    }
    if (stepData->mayCommit())
    {
        RELOG("commiting");
        std::vector<CarryMsg> msgs;
        for (auto&& vn: stepData->votedNodes)
            msgs.push_back(CarryMsg{vn.second.msg, msg.stepId, vn.first});
        commit(msgs);
    }
    else if (canVote)
    {
        RELOG("need to broadcast the vote to further commit");
        NodeId src = thisNode();
        StepId stepId = msg.stepId;
        stepData->availabilityTimer.reset(c_availabilityTimeoutMs, [stepId] {
            single<Replob>().onTimeout(stepId);
        });
        broadcast([msg, src] {
            single<Replob>().vote(msg, src);
        });
    }
}

void Replob::commit(const std::vector<CarryMsg> &msgs)
{
    ELOG("commiting");
    VERIFY(!msgs.empty(), "Zero messages to commit");
    StepId id = msgs.front().stepId;
    StepData* stepData = getStep(id);
    if (stepData == nullptr)
    {
        ELOG("commit: outdated msg");
        return;
    }
    // TODO: add verification: step id, correct node list taken from votedNodes
    stepData->msgs = msgs;
    stepData->votedNodes.clear();
    stepData->state = StepState::Completed;
    stepData->availabilityTimer.reset();
    broadcast([msgs] {
        single<Replob>().commit(msgs);
    });
    tryProcess(id);
}

void Replob::onTimeout(StepId id)
{
    ELOG("on timeout step: " << id);
    StepData* stepData = getStep(id);
    if (stepData == nullptr)
    {
        ELOG("on timeout: outdated msg");
        return;
    }
    auto votedNodes = stepData->votedNodesSet();
    std::vector<NodeId> unavailableNodes;
    for (NodeId n: single<NodesConfig>().nodes())
    {
        if (votedNodes.find(n) == votedNodes.end())
            unavailableNodes.push_back(n);
    }
    if (unavailableNodes.empty())
    {
        ELOG("empty unavailable nodes, skipping step: " << id);
        return;
    }
    disconnectNodes(unavailableNodes);
}

void Replob::disconnectNodes(const std::vector<NodeId> &nodes)
{
    ELOG("disconnect nodes: " << nodes);
    std::vector<NodeId> disconnected;
    for (NodeId n: nodes)
    {
        if (single<Nodes>().remove(n))
            disconnected.push_back(n);
    }
    if (disconnected.empty())
    {
        ELOG("empty disconnected list, skipping");
        return;
    }
    ELOG("new disconnected nodes: " << disconnected);
    broadcast([disconnected] {
        single<Replob>().disconnectNodes(disconnected);
    });
}

Replob::MsgData &Replob::StepData::getMsg(const CarryMsg &msg)
{
    MsgData& msgData = votedNodes[msg.msgId];
    if (msgData.msg == nullptr)
        msgData.msg = msg.msg;
    return msgData;
}

std::unordered_set<NodeId> Replob::StepData::votedNodesSet() const
{
    std::unordered_set<NodeId> res;
    for (auto&& v: votedNodes)
        res.insert(v.second.votedSet.begin(), v.second.votedSet.end());
    return res;
}

bool Replob::StepData::mayCommit() const
{
    size_t nodesCount = 0;
    for (auto&& vn: votedNodes)
        nodesCount += vn.second.votedSet.size();
    // FIXME: check that size has a majority of nodes if you don't need HAT and have more consistency
    return nodesCount == single<NodesConfig>().nodesCount();
}

/*
void Replob::vote(CarryMsg msg, NodeId voted)
{
    RELOG("vote for node: " << voted);
    StepData* stepData = getStep(msg.stepId);
    if (stepData == nullptr)
    {
        RELOG("outdated msg");
        return;
    }
    auto& msgData = stepData->votedNodes[msg.msgId];
    msgData.msg = msg.msg;
    auto& votedSet = msgData.votedSet;
    votedSet.insert(voted);
    bool canVote = stepData->state == StepState::Initial;
    if (canVote)
    {
        RELOG("voting");
        stepData->state = StepState::Voted;
        votedSet.insert(thisNode());
        NodeId src = thisNode();
        broadcast([msg, src] {
            single<Replob>().vote(msg, src);
        });
    }
    tryProcess(msg.stepId);



    if (votedSet.size() >= quorumSize())
    {
        RELOG("has quorum => commiting");
        stepData->state = StepState::Commited;
        tryProcess(msg.stepId);
        //commit(msg);
        broadcast([msg] {
            single<Replob>().commit(msg);
        });
        return;
    }
    if (canVote)
    {
        RELOG("don't have quorum, broadcast my vote");
        NodeId src = thisNode();
        broadcast([msg, src] {
            single<Replob>().vote(msg, src);
        });
        return;
    }
    int votesCount = 0;
    for (auto&& m: stepData->votedNodes)
        votesCount += m.second.votedSet.size();
    int nodesCount = single<NodesConfig>().nodesCount();
    RELOG("already voted, votes count: " << votesCount << " of " << nodesCount);
    if (votesCount != nodesCount)
    {
        RELOG("waiting for votes from another nodes");
        return;
    }
    for (auto&& s: steps_)
    {

    }
    {
        RRELOG("reapply the msg");
        for (auto&& m: stepData->votedNodes)
        {
            vote(CarryMsg{m.second.msg, nextStepId(), m.first}, thisNode());
        }
        stepData->commited = true; // TODO: fake value
    }
    int applied = 0;
    while (true)
    {
        auto it = steps_.find(commitingStepId_);
        if (it == steps_.end())
            break;
        if (!it->second.commited)
            break;
        RELOG("apply message");
        if (it->second.msg != nullptr) // FIXME: not to do this
            it->second.msg();
        ++ applied;
        steps_.erase(commitingStepId_); // TODO: process uncommited entries
        ++commitingStepId_;
    }
    if (applied > 1)
        RRELOG("applied: " << applied);
}
*/

/*
void Replob::commit(CarryMsg msg)
{
    RELOG("commiting");
    StepData* data = getStep(msg.stepId);
    if (data == nullptr)
    {
        RELOG("outdated msg on commit");
        return;
    }
    data->state = true;
    data->msg = msg.msg;
    for (auto&& m: data->votedNodes)
    {
        if (m.first != msg.msgId)
        {
            RRELOG("found uncommited data, reapply");
            vote(CarryMsg{m.second.msg, nextStepId(), m.first}, thisNode());
        }
    }
    data->votedNodes.clear();
    if (commitingStepId_ != msg.stepId)
    {
        RELOG("need to wait for another commit");
        return;
    }
    int applied = 0;
    while (true)
    {
        auto it = steps_.find(commitingStepId_);
        if (it == steps_.end())
            break;
        if (!it->second.commited)
            break;
        RELOG("apply message");
        if (it->second.msg != nullptr) // FIXME: not to do this
            it->second.msg();
        ++ applied;
        steps_.erase(commitingStepId_); // TODO: process uncommited entries
        ++commitingStepId_;
    }
    if (applied > 1)
        RRELOG("applied: " << applied);
}
*/

void Replob::tryProcess(StepId id)
{
    if (id != commitingStepId_)
    {
        ELOG("cannot process: future step: " << id);
        return;
    }
    tryProcess();
}

void Replob::tryProcess()
{
    while (true)
    {
        auto it = steps_.find(commitingStepId_);
        if (it == steps_.end())
            break;
        if (it->second.state != StepState::Completed)
            break;
        ELOG("processing step " << commitingStepId_ << " with messages " << it->second.msgs.size());
        for (auto&& car: it->second.msgs)
        {
            ELOG("invoking msg: " << car);
            car.msg(); // FIXME: what to do on exception?
        }
        steps_.erase(commitingStepId_++);
    }
}

/*
void Replob::tryProcess()
{
    while (true)
    {
        auto it = steps_.find(commitingStepId_);
        if (it == steps_.end())
            break;
        size_t quorum = quorumSize();
        std::vector<AnyMsg> msgs;
        bool commited = false;
        int votesCount = 0;
        for (auto&& m: it->second.votedNodes)
        {
            MsgData& msgData = m.second;
            votesCount += msgData.votedSet.size();
            if (msgData.votedSet.size() >= quorum)
            {
                ELOG("has quorum => commiting");
                msgData.msg();
                commited = true;
            }
            else
            {
                msgs.push_back(msgData.msg);
            }
        }
        if (!commited)
        {
            if (votesCount == single<NodesConfig>().nodesCount())
            {
                ELOG("all nodes voted: apply msg");
                for (auto&& msg: msgs)
                    msg();
            }
        }
        else
        {
            // apply only my own messages (from my node)
        }
        // remove and increase the commiting counter
    }
}
*/

}

