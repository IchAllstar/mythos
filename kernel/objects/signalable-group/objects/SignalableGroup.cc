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

namespace mythos {


optional<void> SignalableGroup::deleteCap(Cap self, IDeleter& del) {
    if (self.isOriginal()) {
        del.deleteObject(del_handle);
    }
    RETURN(Error::SUCCESS);
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
    MLOG_ERROR(mlog::boot, "bind");
}

void SignalableGroup::unbind(optional<ISignalable*>) {
    MLOG_ERROR(mlog::boot, "unbind");
}

Error SignalableGroup::getDebugInfo(Cap self, IInvocation* msg)
{
    return writeDebugInfo("SignalableGroup", self, msg);
}

optional<SignalableGroup*>
SignalableGroupFactory::factory(CapEntry* dstEntry, CapEntry* memEntry, Cap memCap,
                                IAllocator* mem, message_type* data)
{
    auto group = mem->alloc(sizeof(CapRef<SignalableGroup, ISignalable>) * data->groupSize, 64);
    if (!group) {
        dstEntry->reset();
        RETHROW(group);
    }
    memset(*group, 0, sizeof(CapRef<SignalableGroup, ISignalable>) * data->groupSize);
    auto obj = mem->create<SignalableGroup>(
                   (CapRef<SignalableGroup, ISignalable>*) *group, data->groupSize);
    if (!obj) {
        mem->free(*group, 64);
        dstEntry->reset();
        RETHROW(obj);
    }
    Cap cap(*obj);
    auto res = cap::inherit(*memEntry, *dstEntry, memCap, cap);
    if (!res) {
        mem->free(*obj); // mem->release(obj) goes throug IKernelObject deletion mechanism
        RETHROW(res);
    }
    return *obj;
}

Error SignalableGroup::signalAll(Tasklet *t, Cap self, IInvocation *msg) {
    /*TypedCap<ISignalable> signalable(msg->lookupEntry(ec));
    //MLOG_ERROR(mlog::boot, DVAR(signalable));
    signalable.obj()->signal(0);
    */
    
    //auto data = msg->getMessage()->read<protocol::SignalableGroup::SignalAll>();
    MLOG_ERROR(mlog::boot, "signalAll()", DVAR(t), DVAR(self), DVAR(msg));
    ASSERT(member != nullptr);
    for (uint64_t i = 0; i < groupSize; i++) {
        if (member[i].isUsable()) {
            TypedCap<ISignalable> signalable(member[i].cap());
            if (signalable) {
                signalable.obj()->signal(0);
            }
        }
    }

    return Error::SUCCESS;
}

Error SignalableGroup::addMember(Tasklet *t, Cap self, IInvocation *msg) {

    auto data = msg->getMessage()->read<protocol::SignalableGroup::AddMember>();
    MLOG_ERROR(mlog::boot, "addMember()", DVAR(t), DVAR(self), DVAR(msg));

    auto capEntry = msg->lookupEntry(data.signalable());
    TypedCap<ISignalable> obj(capEntry);
    if (!obj) return Error::INVALID_CAPABILITY;
    for (uint64_t i = 0; i < groupSize; i++) {
        if (!member[i].isUsable()) {
            MLOG_ERROR(mlog::boot, "try to set at place ", DVAR(i), DVARhex(*capEntry), DVARhex(obj.cap().asReference().value()));
            member[i].set(this, *capEntry, obj.cap());
            return Error::SUCCESS;
        }
    }

    return Error::SUCCESS;
}

Error SignalableGroup::removeMember(Tasklet *t, Cap self, IInvocation *msg) {

    auto data = msg->getMessage()->read<protocol::SignalableGroup::RemoveMember>();
    MLOG_ERROR(mlog::boot, "removeMember()", DVAR(t), DVAR(self), DVAR(msg));
    data.signalable();
    return Error::SUCCESS;
}

} // namespace mythos