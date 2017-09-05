#pragma once

#include "objects/IdleManagement.hh"

namespace mythos {

optional<void> IdleManagement::deleteCap(Cap self, IDeleter& del) {
	if (self.isOriginal()) {
		del.deleteObject(del_handle);
	}
	RETURN(Error::SUCCESS);
}

optional<void const*> IdleManagement::vcast(TypeId id) const {
	if (id == typeId<IKernelObject>()) return static_cast<IKernelObject const*>(this);
	THROW(Error::TYPE_MISMATCH);
}

void IdleManagement::invoke(Tasklet* t, Cap self, IInvocation* msg)
{
	monitor.request(t, [ = ](Tasklet * t) {
		Error err = Error::NOT_IMPLEMENTED;
		switch (msg->getProtocol()) {
			case protocol::KernelObject::proto:
				err = protocol::KernelObject::dispatchRequest(this, msg->getMethod(), self, msg);
				break;
			case protocol::IdleManagement::proto:
				err = protocol::IdleManagement::dispatchRequest(this, msg->getMethod(), t, self, msg);
				break;
		}
		if (err != Error::INHIBIT) {
			msg->replyResponse(err);
			monitor.requestDone();
		}
	} );
}

Error IdleManagement::getDebugInfo(Cap self, IInvocation* msg)
{
	return writeDebugInfo("IdleManagement", self, msg);
}

// actual implementation stuff

void IdleManagement::wokeup(size_t reason) {
	MLOG_DETAIL(mlog::boot, "wokeup because other HWT on core woke up from cc6", DVAR(reason));
}

void IdleManagement::wokeupFromInterrupt(uint8_t irq) {
	if (timer.exchange(false) == true) {
		if (irq == 0x22) {
			//timer_interrupt.store(true);
      if (delay_lite_sleep != MAX_UINT32) {
        MLOG_ERROR(mlog::boot, "nothing happend");
        //mythos::idle::sleep(6);
        should_deep_sleep.store(true);
      }
		} else { // we woke up from another reason but timer was configured, so disable it to avoid useless interrupt
			mythos::lapic.disableTimer();
		}
	}
	MLOG_DETAIL(mlog::boot, "wokeupFromInterrupt", DVAR(irq));
}

void IdleManagement::enteredFromSyscall() {
	MLOG_DETAIL(mlog::boot, "enteredFromSyscall");
}

void IdleManagement::enteredFromInterrupt(uint8_t irq) {
	MLOG_DETAIL(mlog::boot, "enteredFromInterrupt", DVAR(irq));
}

/*
void IdleManagement::sleepIntention(uint8_t depth) {
	MLOG_DETAIL(mlog::boot, "received sleep intention", DVAR(depth));
	if (depth == 1 && getDelayLiteSleep() != MAX_UINT32) {
		MLOG_DETAIL(mlog::boot, "set timer interrupt", DVAR(getDelayLiteSleep()));
		timer.store(true);
		mythos::lapic.enableOneshotTimer(0x22, getDelayLiteSleep());
	}
}
*/

void IdleManagement::sleep() {
  if (alwaysPoll()) return;
  auto start = startPolling.load();
  if (start != 0) {
    if (getTime() < start + delay_polling) {
      return;
    }
    startPolling.store(0);
  }

  if (delay_polling.load() > 100) {
    startPolling.store(getTime());
    return;
  }
  if (alwaysDeepSleep()) mythos::idle::sleep(6);
	if (getDelayLiteSleep() != MAX_UINT32) {
		MLOG_ERROR(mlog::boot, "set timer interrupt", DVAR(getDelayLiteSleep()));
		timer.store(true);
		mythos::lapic.enableOneshotTimer(0x22, getDelayLiteSleep());
	}
  mythos::idle::sleep(1);
}

bool IdleManagement::shouldDeepSleep() {
  return should_deep_sleep.load();
}

bool IdleManagement::alwaysDeepSleep() {
	return delay_lite_sleep.load() == 0;
}

bool IdleManagement::alwaysPoll() {
	return delay_polling.load() == MAX_UINT32;
}

Error IdleManagement::setPollingDelay(Tasklet*, Cap /*self*/, IInvocation* msg) {
	auto data = msg->getMessage()->cast<protocol::IdleManagement::SetPollingDelay>();
	delay_polling.store(data->delay);
	MLOG_DETAIL(mlog::boot, "Set Polling Delay", DVAR(this), DVAR(delay_polling));
	return Error::SUCCESS;
}

Error IdleManagement::setLiteSleepDelay(Tasklet*, Cap /*self*/, IInvocation* msg) {
	auto data = msg->getMessage()->cast<protocol::IdleManagement::SetLiteSleepDelay>();
	delay_lite_sleep.store(data->delay);
	MLOG_DETAIL(mlog::boot, "Set Lite Sleep Delay", DVAR(this), DVAR(delay_lite_sleep));
	return Error::SUCCESS;
}

} // namespace mythos
