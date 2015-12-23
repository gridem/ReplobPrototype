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

struct CarryMsg
{
    AnyMsg msg;
    StepId stepId;
    MsgId msgId;
};

// add global cleanup'er to close the connections and stops the timers
struct Replob : WithCleanup
{
    void apply(AnyMsg msg);

private:
    enum struct StepState
    {
        Initial, // can vote only in initial state
        Voted,
        Completed,
    };

    struct MsgData // to reapply not commited messages
    {
        std::unordered_set<NodeId> votedSet;
        AnyMsg msg;
    };

    struct StepData
    {
        StepState state = StepState::Initial;
        std::map<MsgId, MsgData> votedNodes; // map <= need to preserve the sequence for uniform application
        std::vector<CarryMsg> msgs;
        Timer availabilityTimer;

        MsgData& getMsg(const CarryMsg& msg);
        std::unordered_set<NodeId> votedNodesSet() const;
        bool mayCommit() const;
    };

    static size_t quorumSize();
    void cleanup() override;
    StepData* getStep(StepId id); // returns nullptr on outdated step id
    StepId nextStepId();
    void updateCurrentStepId(StepId id);

public: // FIXME: remove after tests
    void vote(const CarryMsg& msg, NodeId voted);
    void commit(const std::vector<CarryMsg>& msgs);
    void onTimeout(StepId id);
    void disconnectNodes(const std::vector<NodeId>& nodes);
private: // FIXME: remove after tests
    void tryProcess(StepId id);
    void tryProcess();

    StepId nextStepId_ = 0;
    StepId commitingStepId_ = 0;
    std::unordered_map<StepId, StepData> steps_;
};

template<typename T>
std::set<T> operator-(const std::set<T>& s1, const std::set<T>& s2)
{
    std::set<T> res;
    for (auto&& t: s1)
        if (s2.count(t) == 0)
            res.insert(t);
    return res;
}

template<typename T>
void operator|=(std::set<T>& s, const T& t)
{
    s.insert(t);
}

template<typename T>
void operator|=(std::set<T>& s, const std::set<T>& t)
{
    s.insert(t.begin(), t.end());
}

template<typename T>
void operator&=(std::set<T>& s, const std::set<T>& t)
{
    std::set<T> newSet;
    for (auto&& e: s)
        if (t.count(e) == 1)
            newSet |= e;
    s = std::move(newSet);
}

template<typename T>
std::set<T> operator|(const std::set<T>& s1, const std::set<T>& s2)
{
    std::set<T> res = s1;
    res |= s2;
    return res;
}

// TODO: consider using either unordered_set or set only
template<typename T>
void operator|=(std::unordered_set<T>& s, const T& t)
{
    s.insert(t);
}

struct CarryMsg2
{
    AnyMsg msg;
    MsgId msgId;

    bool operator<(const CarryMsg2& m) const
    {
        return msgId < m.msgId;
    }
};

using CarrySet = std::set<CarryMsg2>;

#define VLOG(D_msg)             JLOG("$" << stepId() << " " << voted_ << "<=" << nodes_ << D_msg)
#define RVLOG(D_msg)            RJLOG("$" << stepId() << " " << voted_ << "<=" << nodes_ << D_msg)
#define VSLOG(D_msg)            VLOG("<- " << srcNode << " " << D_msg)
#define RVSLOG(D_msg)           RVLOG("<- " << srcNode << " " << D_msg)

template<typename T>
std::ostream& operator<<(std::ostream& o, const std::set<T>& s)
{
    o << "(";
    bool rest = false;
    for (auto&& t: s)
    {
        if (rest)
            o << ", ";
        else
            rest = true;
        o << t;
    }
    return o << ")";
}

inline std::ostream& operator<<(std::ostream& o, const CarryMsg2& m)
{
    return o << "{" << m.msgId.first << "," << m.msgId.second << "}";
}

struct Voting : Phantomer<Voting>
{
    enum struct State
    {
        Initial, // can vote only in initial state
        Voted,
        Completed,
    };

    void onComplete() override
    {
        VLOG("ON COMPLETE");
        timer_.reset();
        exec();
    }

    void start(const CarryMsg2& carryMsg)
    {
        // time must be before vote because vote can destroy entry on commit
        timer_.reset(500, [this] {
            timeout();
        });
        vote(CarrySet{ carryMsg }, thisNode(), single<NodesConfig>().nodesSet());
    }

    void vote(const CarrySet& carrySet, NodeId srcNode, const NodesSet& nodesSet)
    {
        if (state_ == State::Completed)
            return;
        carries_ |= carrySet;
        VSLOG("VOTE: " << carrySet << " -> " << carries_);
        if (nodes_.empty())
        {
            nodes_ = nodesSet;
        }
        else if (nodes_ != nodesSet)
        {
            VSLOG("CHANGED SET: " << nodesSet);
            state_ = State::Initial;
            nodes_ &= nodesSet;
            voted_.clear(); // must be below nodes_ because nodesSet can be reference to voted_!!!
        }
        voted_ |= srcNode;
        voted_ |= thisNode();
        if (voted_  == nodes_)
        {
            commit(carries_);
        }
        else if (state_ == State::Initial)
        {
            state_ = State::Voted;
            // TODO: use broadcastSet().<method>(nodes_set, ...)
            broadcast().vote(carries_, thisNode(), nodes_);
        }
    }

    // TODO: don't accept commit (and event votes) from node outside the nodes_ set
    // it can lead to inconsistency when the connection returns back with commit message
    void commit(const CarrySet& carrySet)
    {
        if (state_ == State::Completed)
            return;
        VLOG("COMMIT: " << carrySet);
        state_ = State::Completed;
        carries_ = carrySet;
        broadcast().commit(carries_);
        complete();
    }

    bool isConsistent() const
    {
        if (voted_.size() * 2 > nodes_.size())
            return true;
        if (voted_.size() * 2 < nodes_.size())
            return false;
        // check if voted set contains minimal NodeId
        // special useful case when nodes_.size() == 2
        return *voted_.begin() == *nodes_.begin();
    }

    void timeout()
    {
        // TODO: make an option: A vs C in CAP
        RVLOG("TIMEOUT");
        if (isConsistent())
        {
            VLOG("PROCEED TIMEOUT: " << voted_);
            vote(carries_, thisNode(), voted_);
        }
        else
        {
            fail();
        }
    }

    void fail()
    {
        RVLOG("FAIL");
        cleanupAll();
    }

private:
    void exec()
    {
        VLOG("EXEC: " << carries_);
        for (auto&& carry: carries_)
            carry.msg();
    }

    State state_ = State::Initial;
    NodesSet nodes_;
    NodesSet voted_;
    CarrySet carries_;
    Timer timer_;
};

struct Replob2
{
    void apply(AnyMsg msg)
    {
        CarryMsg2 carryMsg{msg, genUniqueId()};
        createPhantom<Voting>().start(carryMsg);
    }
};

const auto replobApply = [](auto msg) { return single<Replob2>().apply(msg); };

template<typename T_result, typename T_handler>
struct ReplobApplySync
{
    explicit ReplobApplySync(T_handler f) : handler_(f)
    {
    }

    T_result result()
    {
        DetachableDoer doer;
        boost::optional<T_result> result;
        doer_ = &doer;
        result_ = &result;
        replobApply(*this);
        waitForDone();
        return std::move(*result);
    }

    void operator()()
    {
        if (src_ == thisNode())
        {
            *result_ = handler_();
            doer_->done();
        }
        else
        {
            handler_();
        }
    }

private:
    T_handler handler_;
    NodeId src_ = thisNode();
    // TODO: use special wrapper to wrap async with result into sync
    // ptr section because it's used only for src_ node
    boost::optional<T_result>* result_; // optional needed if the type doesn't support default construction
    DetachableDoer* doer_;
};

template<typename T_handler>
struct ReplobApplySync<void, T_handler>
{
    explicit ReplobApplySync(T_handler f) : handler_(f)
    {
    }

    void result()
    {
        DetachableDoer doer;
        doer_ = &doer;
        replobApply(*this);
        waitForDone(); // TODO: throw exception on shutdown event!!!
    }

    void operator()()
    {
        handler_();
        if (src_ == thisNode())
        {
            doer_->done();
        }
    }

private:
    T_handler handler_;
    NodeId src_ = thisNode();
    DetachableDoer* doer_;
};

// TODO: add Sync scope - disallow async waiting, needed for replob transactions
// TODO: add async manipulators

/*
const auto replobApplySync = [](auto handler) {
    NodeId src = thisNode();
    DetachableDoer doer;
    Handler h = handler; // workaround for auto deduction and lambda capture issue
    replobApply([src, h, &doer] {
        if (thisNode() == src)
            doer.done();
        else
            h();
    });
    waitForDone();
    return handler(); //FIXME: violates the sequence call
};
*/

template<typename T_handler>
auto replobApplySync(T_handler handler)
{
    // FIXME: use more consistent approach on fail instead of explicit timeout here
    Timeout _(3000);
    return ReplobApplySync<decltype(handler()), T_handler>(handler).result();
}

// TODO: add Alone replob

#define DECL_MODIFIER(D_mod)    D_mod() + []
#define MCreateModifier(D_mod)  \
    struct D_mod {}; \
    template<typename F> \
    auto operator+(D_mod, F $)

/*
#define MReplobTransact             DECL_MODIFIER(ReplobTransact)

MCreateModifier(ReplobTransact)
{
    return replobApplySync($);
}
*/

#define MReplobTransact             ReplobTransact() + [=](ReplobTransact $)

// TODO: consider add source node id
struct ReplobTransact
{
    template<typename T_replob>
    static T_replob& instance()
    {
        return single<T_replob>();
    }

    template<typename F>
    auto operator+(F f)
    {
        return replobApplySync([f] {
            return f({});
        });
    }
};

/*
#define MReplobTransactLocal        DECL_MODIFIER(ReplobTransactLocal)

MCreateModifier(ReplobTransactLocal)
{
    return $();
}
*/

#define MReplobTransactLocal        ReplobTransactLocal() + [&](ReplobTransactLocal $)
struct ReplobTransactLocal
{
    template<typename T_replob>
    static const T_replob& instance()
    {
        return singleConst<T_replob>();
    }

    template<typename F>
    auto operator+(F f)
    {
        return f({});
    }
};

template<typename T_replob>
struct BaseReplob
{
protected:
    template<typename F, typename... V>
    auto call(F f, V&&... v)
    {
        auto fn = [f, v...]() mutable { return f(single<T_replob>(), std::move(v)...); };
        return replobApplySync(fn);
    }
};

template<typename T_replob>
struct BaseReplobLocal
{
protected:
    template<typename F, typename... V>
    auto call(F f, V&&... v)
    {
        auto fn = [f, v...]() mutable { return f(singleConst<T_replob>(), std::move(v)...); };
        return fn();
    }
};

template<typename T_replob>
auto replob()
{
    return Adapter<T_replob, BaseReplob<T_replob>>();
}

template<typename T_replob>
auto replobLocal()
{
    return Adapter<T_replob, BaseReplobLocal<T_replob>>();
}

#define MReplobTransactInstance(D_replob)           ReplobTransactInstance<D_replob>() + [=](D_replob& $)

template<typename T>
struct ReplobTransactInstance {};

template<typename T_replob, typename F>
auto operator+(ReplobTransactInstance<T_replob>, F f)
{
    return replobApplySync([f] {
        return f(single<T_replob>());
    });
}

#define MReplobTransactLocalInstance(D_replob)           ReplobTransactLocalInstance<D_replob>() + [&](const D_replob& $)

template<typename T>
struct ReplobTransactLocalInstance {};

template<typename T_replob, typename F>
auto operator+(ReplobTransactLocalInstance<T_replob>, F f)
{
    return f(singleConst<T_replob>());
}

}

