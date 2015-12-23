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

#include <vector>
#include <cstring>
#include <chrono>

#include <synca/synca.h>
#include <synca/log.h>

#include "ut.h"

using Ptr = Byte*;
using PtrArray = Ptr*;
using IntArray = int*;
using Buf = std::vector<Byte>;

constexpr size_t ptrSize = sizeof(Ptr);
constexpr size_t intSize = sizeof(int);

struct View
{
    Ptr data;
    size_t size;
};

Buf viewToBuf(View view)
{
    return {view.data, view.data + view.size};
}

struct IAllocator : IObject
{
    virtual void* alloc(size_t sz) = 0;
    virtual void dealloc(void *p) = 0;
};

struct DefaultAllocator
{
    static void* alloc(size_t sz)
    {
        void *p = malloc(sz);
        VERIFY(p != nullptr, "Allocation failed");
        return p;
    }

    static void dealloc(void *p) noexcept
    {
        free(p);
    }
};

thread_local Buf t_buffer(1024*1024*10);
thread_local IAllocator* t_allocator = nullptr;

struct MemoryAllocator : IAllocator
{
    MemoryAllocator()
    {
        t_allocator = this;
    }

    ~MemoryAllocator()
    {
        t_allocator = nullptr;
        if (nDealloc_)
            LOG("Deallocations: " << nDealloc_ << ": " << dp_);
    }

    void* alloc(size_t sz) override
    {
        Ptr p = t_buffer.data() + offset_;
        size_t diff = sz % ptrSize;
        if (diff)
            sz += ptrSize - diff;
        offset_ += sz;
        VERIFY(offset_ <= t_buffer.size(), "MemoryAllocator: allocation failed: oversize");
        return p;
    }

    void dealloc(void *p) override
    {
        if (p >= t_buffer.data() && p < t_buffer.data() + t_buffer.size())
        {
            ++ nDealloc_;
            dp_ = p;
        }
        else
        {
            DefaultAllocator::dealloc(p);
        }
    }

    size_t size() const
    {
        return offset_;
    }

    void setSize(size_t size)
    {
        offset_ = size;
    }

private:
    size_t offset_ = 0;
    int nDealloc_ = 0;
    void* dp_ = nullptr;
};

void* operator new(size_t sz)
{
    return t_allocator ? t_allocator->alloc(sz) : DefaultAllocator::alloc(sz);
}

void operator delete(void *p) noexcept
{
    t_allocator ? t_allocator->dealloc(p) : DefaultAllocator::dealloc(p);
}

void setAllocator(IAllocator* a)
{
    t_allocator = a;
}

IAllocator* getAllocator()
{
    return t_allocator;
}

template<typename T>
Buf serialize(const T& t)
{
    View v {t_buffer.data(), 0};
    {
        MemoryAllocator a;
        new T(t);
        v.size = a.size();
    }
    Buf buf = viewToBuf(v);
    std::memset(v.data, 0, v.size); // need to be zeroized to avoid diff collisions
    return buf;
}

template<typename T>
T deserialize(const Buf& buf)
{
    VERIFY(buf.size() <= t_buffer.size(), "Buffer is too large");
    std::memcpy(t_buffer.data(), buf.data(), buf.size());
    return *(T*)t_buffer.data();
}

template<typename T>
Buf serializeRelative(const T& t)
{
    View v {t_buffer.data(), 0};
    size_t total;
    {
        MemoryAllocator a;
        new T(t);
        v.size = a.size();
        new T(t);
        total = a.size();
    }

    VERIFY(v.size % ptrSize == 0, "Unaligned data"); // carefully!!! verify allocates the memory
    VERIFY(v.size * 2 == total, "Unpredictable copy constructor");

    int pCount = v.size / ptrSize;
    int diffIndex = v.size / intSize;
    auto data = PtrArray(v.data);
    auto diffData = IntArray(v.data);
    for (int i = 0; i < pCount; ++ i)
    {
        int diff = data[i + pCount] - data[i];
        if (diff)
        {
            //LOG("Diff: " << i << ", " << diff << ", " << diffIndex);
            VERIFY(diff == v.size, "Invalid pointers shift");
            data[i] -= intptr_t(v.data);
            diffData[diffIndex ++] = i;
        }
    }
    diffData[diffIndex ++] = v.size / intSize;
    v.size = diffIndex * intSize;
    //LOG("diff index created: " << v.size << ", " << diffIndex << ", " << pCount);
    Buf buf = viewToBuf(v);
    std::memset(v.data, 0, total);
    return buf;
}

// perform in-place transformations
template<typename T>
T& deserializeRelative(Buf& buf)
{
    VERIFY(buf.size() >= intSize, "Invalid buffer size: must be >= intSize");
    VERIFY(buf.size() % intSize == 0, "Invalid buffer size: must be aligned");
    int intCount = buf.size() / intSize - 1; // not to include the diff index
    auto intArray = IntArray(buf.data());
    auto data = PtrArray(buf.data());
    int diffIndex = intArray[intCount];
    VERIFY(diffIndex <= intCount, "Invalid diff index");
    int dataCount = diffIndex * intSize / ptrSize;
    for (int i = diffIndex; i < intCount; ++ i)
    {
        int dataIndex = intArray[i];
        //LOG("Replacing: " << i << ", " << dataIndex);
        VERIFY(dataIndex < dataCount, "Invalid data index");
        data[dataIndex] += intptr_t(buf.data());
    }
    return *(T*)buf.data();
}

template<typename T>
std::ostream& operator<<(std::ostream& o, const std::vector<T>& v)
{
    for (auto&& t: v)
        o << t << " ";
    return o;
}

template<typename F>
void measure(const char* name, F f, size_t count)
{
    auto beg = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < count; ++ i)
        f();
    auto end = std::chrono::high_resolution_clock::now();

    int ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - beg).count() * 1.0 / count;
    LOG(name << ": " << ns << "ns");
}

TEST(serialization, vector)
{
    std::vector<int> v {1, 2, 3, 10, 0x30};
    auto buf = serialize(v);
    auto v2 = deserialize<std::vector<int>>(buf);
    ASSERT_EQ(v, v2);
}

TEST(serialization, relative)
{
    try
    {
        std::vector<int> v {1, 2, 3, 10, 47, 5};
        auto buf = serializeRelative(v);
        auto beg = IntArray(buf.data());
        for (int i = 0; i < buf.size() / intSize; ++ i)
        {
            LOG(i << ": " << beg[i]);
        }
        auto v2 = deserializeRelative<std::vector<int>>(buf);
        for (auto val: v2)
        {
            LOG(val);
        }
        ASSERT_EQ(v, v2);
    }
    catch (std::exception& e)
    {
        LOG("Error: " << e.what());
    }
}

TEST(serialization, relative2)
{
    try
    {
        std::map<std::vector<int>, int> v {{{1, 2}, 1}, {{5, 3, 2}, 5}};
        auto buf = serializeRelative(v);
        auto beg = IntArray(buf.data());
        for (int i = 0; i < buf.size() / intSize; ++ i)
        {
            LOG(i << ": " << beg[i]);
        }
        auto v2 = deserializeRelative<std::map<std::vector<int>, int>>(buf);
        ASSERT_TRUE(v == v2);
    }
    catch (std::exception& e)
    {
        LOG("Error: " << e.what());
    }
}

/*
TEST(serialization, relative2BenchLite)
{
    struct Lite
    {
        int32_t x;
        std::vector<int64_t> y;
    };

    Lite l;
    l.x = 1;
    l.y = {10,11};

    measure("serialize lite", [&]{
        auto buf = serializeRelative(l);
    }, 1000000);
}

Buf toBuf(const char* s)
{
    std::string c(s);
    return Buf{c.begin(), c.begin() + c.size()};
}

TEST(serialization, relative2BenchHeavy)
{
    struct Lite
    {
        int32_t x;
        std::vector<int64_t> y;
    };

    struct Heavy
    {
        std::vector<int32_t> x;
        std::vector<Buf> s;
        std::vector<Lite> z;
    };

    Heavy h;
    h.x = {1,2,3,4,5,6,7,8,9,10};
    h.s = {toBuf("hello"), toBuf("yes"), toBuf("this"), toBuf("is"), toBuf("dog")};
    h.z = {Lite{1, {10,11}}, Lite{2, {32,33,34,35}}};
    measure("serialize heavy", [&]{
        auto buf = serializeRelative(h);
    }, 100000);
}

TEST(deserialization, relative2BenchHeavy)
{
    struct Lite
    {
        int32_t x;
        std::vector<int64_t> y;
    };

    struct Heavy
    {
        std::vector<int32_t> x;
        std::vector<Buf> s;
        std::vector<Lite> z;
    };

    Heavy h;
    h.x = {1,2,3,4,5,6,7,8,9,10};
    h.s = {toBuf("hello"), toBuf("yes"), toBuf("this"), toBuf("is"), toBuf("dog")};
    h.z = {Lite{1, {10,11}}, Lite{2, {32,33,34,35}}};
    Buf buf = serializeRelative(h);
    measure("deserialize heavy", [&]{
        Heavy& h = deserializeRelative<Heavy>(buf);
    }, 10000000);
}
*/

TEST(math, test)
{
    ASSERT_EQ(-2, -2 % 4);
    ASSERT_EQ(-2, -10 % 4);
}

CPPUT_TEST_MAIN
