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

#include "cpu/hwthreadid.hh"
#include "objects/SchedulingCoordinator.hh"
#include "objects/mlog.hh"
#include "util/Time.hh"
#include "cpu/LAPIC.hh"

namespace mythos {

static mlog::Logger<mlog::FilterAny> mlogcoord("SchedulingCoordinator");

optional<void> SchedulingCoordinator::deleteCap(Cap self, IDeleter& del) {
  if (self.isOriginal()) {
    del.deleteObject(del_handle);
  }
  RETURN(Error::SUCCESS);
}

optional<void const*> SchedulingCoordinator::vcast(TypeId id) const {
    if (id == typeId<IKernelObject>()) return static_cast<IKernelObject const*>(this);
    THROW(Error::TYPE_MISMATCH);
  }

void SchedulingCoordinator::invoke(Tasklet* t, Cap self, IInvocation* msg)
{
  monitor.request(t, [ = ](Tasklet * t) {
    Error err = Error::NOT_IMPLEMENTED;
    switch (msg->getProtocol()) {
      case protocol::KernelObject::proto:
        err = protocol::KernelObject::dispatchRequest(this, msg->getMethod(), self, msg);
        break;
      case protocol::SchedulingCoordinator::proto:
        err = protocol::SchedulingCoordinator::dispatchRequest(this, msg->getMethod(), t, self, msg);
        break;
    }
    if (err != Error::INHIBIT) {
      msg->replyResponse(err);
      monitor.requestDone();
    }
  } );
}

Error SchedulingCoordinator::getDebugInfo(Cap self, IInvocation* msg)
{
  return writeDebugInfo("SchedulingCoordinator", self, msg);
}

// usefull protocol implementations

Error SchedulingCoordinator::printMessage(Tasklet*, Cap self, IInvocation* msg)
{
  MLOG_INFO(mlog::boot, "invoke printMessage", DVAR(this), DVAR(self), DVAR(msg));
  auto data = msg->getMessage()->cast<protocol::SchedulingCoordinator::PrintMessage>();
  mlogcoord.error("Actual policy", DVAR(policy), mlog::DebugString(data->message, data->bytes));
  return Error::SUCCESS;
}

Error SchedulingCoordinator::setPolicy(Tasklet*, Cap self, IInvocation* msg)
{
  MLOG_INFO(mlog::boot, "invoke set policy", DVAR(this), DVAR(self), DVAR(msg));
  auto data = msg->getMessage()->cast<protocol::SchedulingCoordinator::SpinPolicy>();
  policy = (Policy)data->policy;
  return Error::SUCCESS;
}


// actual functionality

void SchedulingCoordinator::runSleep() {
  localPlace->processTasks(); // executes all available kernel tasks
  auto *ec = localSchedulingContext->tryRunUser();
  if (ec) {
    if (ec->prepareResume()) {
      while (not localPlace->releaseKernel()) {
        localPlace->processTasks();
        //hwthread_pause(50);
      }
      ec->doResume(); //does not return (hopefully)
      MLOG_WARN(mlog::boot, "Returned even prepareResume was successful");
    }
  }
  while (not localPlace->releaseKernel()) {
    localPlace->processTasks();
  }
  sleep();
}

void SchedulingCoordinator::runConfigurableDelays() {
  if (timer.exchange(false) == true) {
    MLOG_ERROR(mlog::boot, "Deep Sleep now");
    sleep();
  }
  localPlace->processTasks(); // executes all available kernel tasks
  auto *ec = localSchedulingContext->tryRunUser();
  if (ec) {
    if (ec->prepareResume()) {
      while (not localPlace->releaseKernel()) { // new kernel tasks
        localPlace->processTasks();
        hwthread_pause(100);
      }
      ec->doResume(); //does not return (hopefully)
      MLOG_WARN(mlog::boot, "Returned even prepareResume was successful");
      sleep();
    }
  }
  uint64_t start = getTime();
  while (start + 1000000 > getTime()) { // poll configured delay
    //MLOG_ERROR(mlog::boot, "Delay");
    localPlace->processTasks();
    auto *ec = localSchedulingContext->tryRunUser();
    if (ec) {
      if (ec->prepareResume()) {
        while (not localPlace->releaseKernel()) { // new kernel tasks
          localPlace->processTasks();
          hwthread_pause(100);
        }
        ec->doResume(); //does not return (hopefully)
        MLOG_WARN(mlog::boot, "Returned even prepareResume was successful");
        sleep();
      }
    }
    hwthread_pause(100);
  }
  while (not localPlace->releaseKernel()) { // release kernel
    localPlace->processTasks();
  }
  timer.store(true);
  MLOG_ERROR(mlog::boot, "Sleeping lite");
  mythos::lapic.enableOneshotTimer(0x22, 10000000);
  sleep();
}

void SchedulingCoordinator::runSpin() {
  while (true) {
    localPlace->enterKernel();
    localPlace->processTasks(); // executes all available kernel tasks
    auto *ec = localSchedulingContext->tryRunUser();
    if (ec) {
      if (ec->prepareResume()) {
        while (not localPlace->releaseKernel()) {
          MLOG_DETAIL(mlog::boot, "Could not release kernel");
          localPlace->processTasks();
          // pause seems to be very slow on qemu !!
          //hwthread_pause(1);
        }
        ec->doResume(); //does not return (hopefully)
        MLOG_WARN(mlog::boot, "Returned even prepareResume was successful");
      }
    }
  }
}

} // namespace mythos