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

#include "objects/HelperThreadManager.hh"
#include "objects/mlog.hh"
#include "boot/DeployHWThread.hh"

namespace mythos {


optional<void> HelperThreadManager::deleteCap(Cap self, IDeleter& del) {
    if (self.isOriginal()) {
        del.deleteObject(del_handle);
    }
    RETURN(Error::SUCCESS);
}

optional<void const*> HelperThreadManager::vcast(TypeId id) const {
    if (id == typeId<IKernelObject>()) return static_cast<IKernelObject const*>(this);
    THROW(Error::TYPE_MISMATCH);
}

void HelperThreadManager::bind(optional<IScheduler*>) {
    MLOG_ERROR(mlog::boot, "bind");
}
void HelperThreadManager::unbind(optional<IScheduler*>) {
    MLOG_ERROR(mlog::boot, "unbind");
}

void HelperThreadManager::invoke(Tasklet* t, Cap self, IInvocation* msg)
{
    monitor.request(t, [ = ](Tasklet * t) {
        Error err = Error::NOT_IMPLEMENTED;
        switch (msg->getProtocol()) {
            case protocol::KernelObject::proto:
                err = protocol::KernelObject::dispatchRequest(this, msg->getMethod(), self, msg);
                break;
            case protocol::HelperThreadManager::proto:
                err = protocol::HelperThreadManager::dispatchRequest(this, msg->getMethod(), t, self, msg);
                break;
        }
        if (err != Error::INHIBIT) {
            msg->replyResponse(err);
            monitor.requestDone();
        }
    } );
}

Error HelperThreadManager::getDebugInfo(Cap self, IInvocation* msg)
{
    return writeDebugInfo("SchedulingCoordinator", self, msg);
}

Error HelperThreadManager::registerHelper(Tasklet*, Cap, IInvocation* msg) {
    auto data = msg->getMessage()->read<protocol::HelperThreadManager::RegisterHelper>();
    //MLOG_ERROR(mlog::boot, "registerHelper", DVAR(data.sc));
    if (not member[data.sc]) {
        member[data.sc] = true;
        auto &scheduler = mythos::boot::getSchedulingCoordinator(data.sc);
        scheduler.setPolicy(SchedulingCoordinator::SPIN);
    }

    return Error::SUCCESS;
}

SchedulingContext* HelperThreadManager::getHelper(uint64_t i) {
    uint64_t tmp = 0;
    for (uint64_t j = 0; j < MYTHOS_MAX_THREADS; j++) {
      if (member[j] == true) tmp++;
      if (tmp == i + 1) {
        return &mythos::boot::getScheduler(j);
      }
    }
    PANIC("Not enough helper threads.");
    return nullptr;
}

} // namespace mythos
