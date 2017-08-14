/* -*- mode:C++; indent-tabs-mode:nil; -*- */
/* MyThOS: The Many-Threads Operating System
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

#include "objects/SignalableGroup.hh"
#include "objects/mlog.hh"
#include "objects/TreeMulticast.hh"
#include "objects/SequentialMulticast.hh"
#include "objects/HelperMulticast.hh"

namespace mythos {

SignalableGroup::SignalableGroup(IAsyncFree* mem, CapRef<SignalableGroup, ISignalable> *arr, Tasklet *tasklets_, size_t groupSize_)
    : _mem(mem), member(arr), tasklets(tasklets_), groupSize(groupSize_)
{
    MLOG_DETAIL(mlog::boot, "Create Group with size", groupSize);

    // initialize arrays in place
    CapRef<SignalableGroup, ISignalable>* obj IGNORE_UNUSED;
    for (uint64_t i = 0; i < groupSize; i++) {
        obj = new (&member[i]) CapRef<SignalableGroup, ISignalable>();
    }

    Tasklet* tasklet IGNORE_UNUSED;
    for (uint64_t i = 0; i < groupSize; i++) {
        tasklet = new (&tasklets[i]) Tasklet();
    }
}

optional<void> SignalableGroup::deleteCap(Cap self, IDeleter& del) {
    if (self.isOriginal()) {
        del.deleteObject(del_handle);
    }
    RETURN(Error::SUCCESS);
}

void SignalableGroup::deleteObject(Tasklet* t, IResult<void>* r)  {
    monitor.doDelete(t, [ = ](Tasklet * t) {
        for (uint64_t i = 0; i < groupSize; i++) {
            member[i].reset();
        }
        _mem->free(t, r, this, sizeof(SignalableGroup));
    });
}

optional<void const*> SignalableGroup::vcast(TypeId id) const {
    if (id == typeId<IKernelObject>()) return static_cast<IKernelObject const*>(this);
    THROW(Error::TYPE_MISMATCH);
}

void SignalableGroup::invoke(Tasklet* t, Cap self, IInvocation* msg)
{
    monitor.request(t, [ = ](Tasklet * t) {
        Error err = Error::NOT_IMPLEMENTED;
        switch (msg->getProtocol()) {
            case protocol::KernelObject::proto:
                err = protocol::KernelObject::dispatchRequest(this, msg->getMethod(), self, msg);
                break;
            case protocol::SignalableGroup::proto:
                err = protocol::SignalableGroup::dispatchRequest(this, msg->getMethod(), t, self, msg);
                break;
        }
        if (err != Error::INHIBIT) {
            msg->replyResponse(err);
            monitor.requestDone();
        }
    } );
}

void SignalableGroup::bind(optional<ISignalable*>) {
    MLOG_DETAIL(mlog::boot, "bind");
}

void SignalableGroup::unbind(optional<ISignalable*>) {
    MLOG_DETAIL(mlog::boot, "unbind");
}

Error SignalableGroup::getDebugInfo(Cap self, IInvocation* msg)
{
    return writeDebugInfo("SignalableGroup", self, msg);
}

optional<SignalableGroup*>
SignalableGroupFactory::factory(CapEntry* dstEntry, CapEntry* memEntry, Cap memCap,
                                IAllocator* mem, message_type* data)
{
    // Allocate CapRef array to save group members
    auto group = mem->alloc(sizeof(CapRef<SignalableGroup, ISignalable>) * data->groupSize, 64);
    if (!group) {
        dstEntry->reset();
        RETHROW(group);
    }
    memset(*group, 0, sizeof(CapRef<SignalableGroup, ISignalable>) * data->groupSize);

    // Allocate Tasklet array for asynchronous handling of propagation
    auto tasklets = mem->alloc(sizeof(Tasklet) * data->groupSize, 64);
    if (!tasklets) {
        dstEntry->reset();
        RETHROW(tasklets);
    }
    memset(*tasklets, 0, sizeof(Tasklet) * data->groupSize);

    // Create actual kernel object
    auto obj = mem->create<SignalableGroup>(
                   (CapRef<SignalableGroup, ISignalable>*) *group, (Tasklet*) *tasklets, data->groupSize);
    if (!obj) {
        mem->free(*group, 64);
        dstEntry->reset();
        RETHROW(obj);
    }
    Cap cap(*obj);
    auto res = cap::inherit(*memEntry, *dstEntry, memCap, cap);
    if (!res) {
        mem->free(*group, 64);
        mem->free(*obj); // mem->release(obj) goes throug IKernelObject deletion mechanism
        dstEntry->reset();
        RETHROW(res);
    }
    return *obj;
}

Error SignalableGroup::signalAll(Tasklet *t, Cap self, IInvocation *msg) {
    MLOG_ERROR(mlog::boot, "signalAll()", DVAR(t), DVAR(self), DVAR(msg), DVAR(actualSize));
    ASSERT(member != nullptr);
    return HelperMulticast::multicast(this, actualSize);
}

Error SignalableGroup::addMember(Tasklet *t, Cap self, IInvocation *msg) {

    auto data = msg->getMessage()->read<protocol::SignalableGroup::AddMember>();
    MLOG_DETAIL(mlog::boot, "addMember()", DVAR(t), DVAR(self), DVAR(msg));

    auto capEntry = msg->lookupEntry(data.signalable());
    TypedCap<ISignalable> obj(capEntry);
    if (!obj) return Error::INVALID_CAPABILITY;

    // look if it is in the group
    for (uint64_t i = 0; i < groupSize; i++) {
        if (member[i].cap().getPtr() == obj.cap().getPtr()) {
            return Error::INVALID_ARGUMENT;
        }
    }
    for (uint64_t i = 0; i < groupSize; i++) {
        if (!member[i].isUsable()) {
            member[i].set(this, *capEntry, obj.cap());
            actualSize++;
            return Error::SUCCESS;
        }
    }
    return Error::INSUFFICIENT_RESOURCES;
}

} // namespace mythos
