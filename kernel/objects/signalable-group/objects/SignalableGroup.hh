/* -*- mode:C++; indent-tabs-mode:nil; -*- */
/* MIT License -- MyThOS: The Many-Threads Operating System
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright 2016 Randolf Rotta, Robert Kuban, and contributors, BTU Cottbus-Senftenberg
 */
#pragma once

#include "objects/IFactory.hh"
#include "objects/CapRef.hh"
#include "objects/ISignalable.hh"
#include "mythos/protocol/KernelObject.hh"
#include "mythos/protocol/SignalableGroup.hh"


namespace mythos {


/**
 * A Group of ISignalable objects with a maximum group size. When signalAll() is invoked, a signal is somehow
 * send to all group members. The group is constructed with a maximum group size and members can just be added,
 * not deleted. Max Groupsize Tasklets are allocated and serve as function objects to send to the destination
 * hardware threads. They can be used to implemented broadcasts, where nodes are used to forward the cast.
 * Always use the tasklet of the receiver node, because I need to send to multiple children, so cannot use my own
 * tasklet.
 * TODO what to do, if SignalableGroup is destroyed while cast is ongoing
 */
class SignalableGroup
    : public IKernelObject
{
public: // Constructor
    SignalableGroup(IAsyncFree* mem, CapRef<SignalableGroup, ISignalable> *arr, Tasklet *tasklets_, size_t groupSize_);

public:
    enum CastStrategy {
      SEQUENTIAL = 0,
      TREE,
      HELPER,

      SIZE,
    };
public: // IKernelObject interface
    optional<void const*> vcast(TypeId id) const override;
    optional<void> deleteCap(Cap self, IDeleter& del) override;
    void deleteObject(Tasklet* t, IResult<void>* r) override;
    void invoke(Tasklet* t, Cap, IInvocation* msg) override;
    Error invokeBind(Tasklet* t, Cap, IInvocation* msg);

public: // protocol
    friend struct protocol::KernelObject;
    Error getDebugInfo(Cap self, IInvocation* msg);
    Error signalAll(Tasklet *t, Cap self, IInvocation *msg);
    Error addMember(Tasklet *t, Cap self, IInvocation *msg);
    Error setCastStrategy(Tasklet *t, Cap self, IInvocation *msg);
    Error addHelper(Tasklet *t, Cap self, IInvocation *msg);
public:
    void bind(optional<ISignalable*>);
    void unbind(optional<ISignalable*>);
public:
    CapRef<SignalableGroup, ISignalable>* getMembers() { return member; }
    Tasklet* getTasklets() { return tasklets; }
    size_t getSize() { return actualSize; }
    Tasklet* getTasklet(size_t idx) { ASSERT(idx < actualSize); return &tasklets[idx]; }
    CapRef<SignalableGroup, ISignalable>* getMember(size_t idx) { ASSERT(idx < actualSize); return &member[idx]; }
    SchedulingCoordinator* getHelper(uint64_t i);
    uint64_t numHelper() { return actualHelper; }
private:
    IAsyncFree* _mem;
    /** list handle for the deletion procedure */
    LinkedList<IKernelObject*>::Queueable del_handle = {this};
    async::NestedMonitorDelegating monitor;

    // allocate when max group size is known
    CapRef<SignalableGroup, ISignalable> *member {nullptr};
    Tasklet *tasklets;
    size_t groupSize {0};
    size_t actualSize {0};

    // helper support, one entry for every hwthread
    bool helper[MYTHOS_MAX_THREADS] {false};
    uint64_t actualHelper {0};

    uint64_t strategy {SEQUENTIAL};
};

class SignalableGroupFactory : public FactoryBase
{
public:
    typedef protocol::SignalableGroup::Create message_type;

    static optional<SignalableGroup*>
    factory(CapEntry* dstEntry, CapEntry* memEntry, Cap memCap, IAllocator* mem, message_type* data = nullptr);

    Error factory(CapEntry* dstEntry, CapEntry* memEntry, Cap memCap,
                  IAllocator* mem, IInvocation* msg) const override {
        auto data = msg->getMessage()->read<message_type>();
        return factory(dstEntry, memEntry, memCap, mem, &data).state();
    }
};

} // namespace mythos
