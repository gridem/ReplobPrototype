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

using MsgId = UniqueId;
using StepId = id_t;

/*
 * TODO: move complete property inside phantom implementation,
 * 2. use callback to ask phantom implementation about completion
 * 3. add instances().{method} to notify other phantom instances, needed to uncomplete
 * 4. consider using global state?
 */

struct IPhantom : IObject
{
    virtual void complete() = 0;
    virtual void uncomplete() = 0;
    virtual StepId stepId() const = 0;
    virtual void onComplete() {}
};

template<typename T>
struct Phantom : WithCleanup
{
    struct WithPhantom : T
    {
        void complete() override
        {
            completed_ = true;
            single<Phantom>().complete(*this);
        }

        void uncomplete() override
        {
            completed_ = false;
        }

        bool tryComplete()
        {
            if (!completed_)
                return false;
            this->onComplete();
            return completed_;
        }

        StepId stepId() const override
        {
            return stepId_;
        }

        StepId stepId_ = 0;
        bool completed_ = false;
    };

    T& create()
    {
        return get(nextStepId_++);
    }

    T& get(StepId stepId)
    {
        T* p = getPtr(stepId);
        VERIFY(p != nullptr, "Invalid step id");
        return *p;
    }

    T* getPtr(StepId stepId)
    {
        if (stepId < currentStepId_)
            return nullptr;
        auto& t = steps_[stepId];
        t.stepId_ = stepId;
        // update next step if we found larger step id from remote node
        if (nextStepId_ <= stepId)
            nextStepId_ = stepId + 1;
        return &t;
    }

    void complete(WithPhantom& p)
    {
        if (p.stepId_ != currentStepId_)
            return;
        if (!p.tryComplete())
            return;
        steps_.erase(currentStepId_);
        while (++currentStepId_ < nextStepId_)
        {
            auto it = steps_.find(currentStepId_);
            if (it == steps_.end())
                break;
            auto&& wp = it->second;
            if (!wp.tryComplete())
                break;
            steps_.erase(it);
        }
    }

private:
    void cleanup() override
    {
        steps_.clear();
        currentStepId_ = std::numeric_limits<StepId>::max();
    }

    StepId nextStepId_ = 0;
    StepId currentStepId_ = 0;
    std::unordered_map<StepId, WithPhantom> steps_;
};

template<typename T>
T& createPhantom()
{
    return single<Phantom<T>>().create();
}

template<typename T>
T& phantom(StepId stepId)
{
    return single<Phantom<T>>().get(stepId);
}

template<typename T>
T* phantomPtr(StepId stepId)
{
    return single<Phantom<T>>().getPtr(stepId);
}

}
