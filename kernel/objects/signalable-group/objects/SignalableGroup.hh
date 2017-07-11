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
#include "objects/TreeBroadcast.hh"

namespace mythos {


/**
 * A Group of ISignalable objects with a maximum group size. When signalAll() is invoked, a signal is
 * send to every valid group member. Some data can be passed through CapData data. It is the responsibility
 * of the receiver to handle (aggregate or save in a list) the received data.
 * TODO handle correct deletion of kernel object -> free the array
 */
class SignalableGroup
    : public IKernelObject
{
public: // Constructor
    SignalableGroup(IAsyncFree* mem, CapRef<SignalableGroup, ISignalable> *arr, size_t groupSize_)
        :_mem(mem), member(arr), groupSize(groupSize_)
        {
            MLOG_ERROR(mlog::boot, "Create Group with size", groupSize);
            // TODO maybe better way to get pseudo static array
            CapRef<SignalableGroup, ISignalable>* obj IGNORE_UNUSED;
            for (uint64_t i = 0; i < groupSize; i++) {
               obj = new (&member[i]) CapRef<SignalableGroup, ISignalable>();
            }
        }
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
    Error removeMember(Tasklet *t, Cap self, IInvocation *msg);
public:
    void bind(optional<ISignalable*>);
    void unbind(optional<ISignalable*>);
private:
    IAsyncFree* _mem;
    /** list handle for the deletion procedure */
    LinkedList<IKernelObject*>::Queueable del_handle = {this};
    async::NestedMonitorDelegating monitor;

    // allocate when group size is known
    CapRef<SignalableGroup, ISignalable> *member {nullptr};
    size_t groupSize {0};

    // Signaling Broadcast mechanism
    TreeBroadcast tree;
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
