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

#include "objects/InterruptControl.hh"
#include "mythos/protocol/InterruptControl.hh"
#include "objects/mlog.hh"

namespace mythos {

static mlog::Logger<mlog::FilterAny> interruptLog("InterruptControl");

optional<void> InterruptControl::deleteCap(Cap self, IDeleter& del) {
    if (self.isOriginal()) {
        del.deleteObject(del_handle);
    }
    RETURN(Error::SUCCESS);
}

optional<void const*> InterruptControl::vcast(TypeId id) const {
    if (id == typeId<IKernelObject>()) return static_cast<IKernelObject const*>(this);
    THROW(Error::TYPE_MISMATCH);
}

void InterruptControl::invoke(Tasklet* t, Cap self, IInvocation* msg)
{
    monitor.request(t, [ = ](Tasklet * t) {
        Error err = Error::NOT_IMPLEMENTED;
        switch (msg->getProtocol()) {
            case protocol::KernelObject::proto:
                err = protocol::KernelObject::dispatchRequest(this, msg->getMethod(), self, msg);
                break;
            case protocol::InterruptControl::proto:
                err = protocol::InterruptControl::dispatchRequest(this, msg->getMethod(), t, self, msg);
                break;
        }
        if (err != Error::INHIBIT) {
            msg->replyResponse(err);
            monitor.requestDone();
        }
    } );
}

void InterruptControl::bind(optional<ISignalable*>) {
    interruptLog.error("bind");
}

void InterruptControl::unbind(optional<ISignalable*>) {
    interruptLog.error("unbind");
}

Error InterruptControl::getDebugInfo(Cap self, IInvocation* msg)
{
    return writeDebugInfo("InterruptControl", self, msg);
}

Error InterruptControl::registerForInterrupt(Tasklet *t, Cap self, IInvocation *msg) {
    auto data = msg->getMessage()->read<protocol::InterruptControl::Register>();
    auto ec = data.ec();
    auto interrupt = data.interrupt;
    MLOG_ERROR(mlog::boot, "invoke registerForInterrupt", DVAR(t), DVAR(self),DVAR(ec), DVAR(interrupt));
    return Error::SUCCESS;
}
Error InterruptControl::unregisterForInterrupt(Tasklet *t, Cap self, IInvocation *msg) {
    auto data = msg->getMessage()->read<protocol::InterruptControl::Register>();
    auto ec = data.ec();
    auto interrupt = data.interrupt;
    MLOG_ERROR(mlog::boot, "invoke registerForInterrupt", DVAR(t), DVAR(self),DVAR(ec), DVAR(interrupt));
    return Error::SUCCESS;
}

} // namespace mythos