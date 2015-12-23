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

Buffer& tlsBuffer();

constexpr size_t c_ptrSize = sizeof(Ptr);
constexpr size_t c_intSize = sizeof(int);

struct MemoryAllocator// : IAllocator
{
    MemoryAllocator();
    ~MemoryAllocator();

    void* alloc(size_t sz);
    void dealloc(void *p);
    size_t size() const;
    void setSize(size_t size);

private:
    size_t offset_ = 0;
    Buffer& buffer_;
};

// TODO: consider using copy interface and hide the implementation
// need to copy to buffer immediately
template<typename T>
View serialize(const T& obj)
{
    View v {tlsBuffer().data(), 0};
    size_t total;
    {
        MemoryAllocator a;
        new T{obj};
        v.size = a.size();
        new T{obj};
        total = a.size();
    }

    VERIFY(v.size % c_ptrSize == 0, "Unaligned data"); // carefully!!! verify allocates the memory
    VERIFY(v.size * 2 == total, "Unpredictable copy constructor");

    int pCount = v.size / c_ptrSize;
    int diffIndex = v.size / c_intSize;
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
    diffData[diffIndex ++] = v.size / c_intSize;
    v.size = diffIndex * c_intSize;
    //LOG("diff index created: " << v.size << ", " << diffIndex << ", " << pCount);
    return v;
}

// changes the buffer, performs in-place transformation
template<typename T>
T& deserialize(Buffer& buf)
{
    VERIFY(buf.size() >= c_intSize, "Invalid buffer size: must be >= intSize");
    VERIFY(buf.size() % c_intSize == 0, "Invalid buffer size: must be aligned");
    int intCount = buf.size() / c_intSize - 1; // not to include the diff index
    auto intArray = IntArray(buf.data());
    auto data = PtrArray(buf.data());
    int diffIndex = intArray[intCount];
    VERIFY(diffIndex <= intCount, "Invalid diff index");
    int dataCount = diffIndex * c_intSize / c_ptrSize;
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
View podToView(T& t)
{
    return {Ptr(&t), sizeof(T)};
}

void bufInsertView(Buffer& b, View v);

template<typename T>
void bufInsertPod(Buffer& b, const T& obj)
{
    bufInsertView(b, podToView(obj));
}

View bufToView(Buffer& b);

void showBuffer(Buffer& buf);

template<typename T>
Buffer serializeToPacket(T& obj)
{
    View v = serialize(obj);
    //JLOG("view size: " << v.size);
    Buffer b;
    bufInsertPod(b, v.size);
    bufInsertView(b, v);
    JLOG("buf size: " << b.size());
    //showBuffer(b);
    return b;
}

}
