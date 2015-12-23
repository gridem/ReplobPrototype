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

// TODO: add GC forward declaration?? is it possible?

namespace synca {

namespace detail {

struct TypeAction
{
    using Fn = void(void*);

    void* val;
    Fn* action;

    void operator()() const;
};

template<typename T>
struct Deleter
{
    static void action(void* v)
    {
        delete static_cast<T*>(v);
    }
};

void gcadd(TypeAction action);

}

template<typename T, typename... V>
T* gcnew(V&&... v)
{
    detail::gcadd({new T(std::forward<V>(v)...), &detail::Deleter<T>::action});
}

}
