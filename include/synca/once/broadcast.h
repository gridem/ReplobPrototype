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

struct Broadcast
{
    // AnyMsg - can be invoked on remote node
    // Handler - only locally
    void send(AnyMsg msg, Handler onTimeout);
};

struct BaseReturner
{
protected:
    template<typename F>
    auto call(F f)
    {
        return f;
    }
};

struct BaseInvoker
{
protected:
    template<typename F>
    void call(F f)
    {
        f();
    }
};

struct BaseBroadcast
{
protected:
    template<typename F>
    void call(F f)
    {
        broadcast(f);
    }
};

template<typename T_objector, typename T_base = BaseReturner>
struct BaseObjector : T_base
{
protected:
    template<typename F, typename... V>
    auto call(F f, V&&... v)
    {
        return T_base::call([f, v...]() mutable {
            f(single<T_objector>(), std::move(v)...);
        });
    }
};

template<typename T_phantom, typename T_base = BaseReturner>
struct BasePhantom : T_base
{
    explicit BasePhantom(StepId id) : stepId_{id} {}

protected:
    template<typename F, typename... V>
    auto call(F f, V&&... v)
    {
        auto id = stepId_;
        return T_base::call([id, f, v...]() mutable {
            T_phantom* p = phantomPtr<T_phantom>(id);
            if (p)
                f(*p, std::move(v)...);
        });
    }

private:
    StepId stepId_;
};

// TODO: avoid using default BaseReturner
template<typename T>
using Objector = Adapter<T, BaseObjector<T>>;

template<typename T_objector>
auto broadcastObjector()
{
    return Adapter<T_objector, BaseObjector<T_objector, BaseBroadcast>>();
}

template<typename T_phantom>
auto broadcastPhantom(T_phantom& p)
{
    return Adapter<T_phantom, BasePhantom<T_phantom, BaseBroadcast>>(p.stepId());
}

template<typename T_phantom>
auto broadcastCreatedPhantom()
{
    return broadcastPhantom(createPhantom<T_phantom>());
}

template<typename T>
struct Phantomer : IPhantom
{
    auto broadcast()
    {
        return broadcastPhantom(static_cast<T&>(*this));
    }
};

}
