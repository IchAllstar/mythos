#pragma once

#include "objects/ISignalable.hh"
#include "boot/DeployHWThread.hh"
#include "objects/mlog.hh"
#include <atomic>

namespace mythos {

class IIdleManagement {
	virtual void init_thread() = 0;
	virtual void wokeup(size_t apicID, size_t reason) = 0;
	virtual void wokeupFromInterrupt(uint8_t irq) = 0;
	virtual void enteredFromSyscall() = 0;
	virtual void enteredFromInterrupt(uint8_t irq) = 0;
	//NORETURN virtual  void sleep() = 0;
};

constexpr uint32_t MAX_UINT32 = (uint32_t)-1;

/**
 * Configurable Delay Idle Management
 * Let the user define two values:
 * delay_polling, which defines the duration the hardware thread wil poll the kernel and user task queues before going to lite sleep
 * delay_lite_sleep, which is the time the hardware thread waits in lite sleep until finally going to deep sleep
 * If delay_lite_sleep == 0 then deep sleep is never entered
 * If delay_polling == MAX_UINT32 then the hardware thread is always polling without entering lite sleep
 */
class IdleManagement : public IIdleManagement {

public:
	uint32_t getDelayPolling() { return delay_polling; }
	uint32_t getDelayLiteSleep() { return delay_lite_sleep; }
public: // IIdleManagement Interface
	void wokeup(size_t apicID, size_t reason) override {
		MLOG_ERROR(mlog::boot, "idle wokeup");
		if (reason == 1) {
			MLOG_ERROR(mlog::boot, "exit cc6", DVAR(apicID));
		}
	}

	void init_thread() override {
		MLOG_ERROR(mlog::boot, "init_thread");
	}
	void wokeupFromInterrupt(uint8_t irq) override {
		if (irq == 0x22 && timer.load() == true) {
			timer_interrupt.store(true);
		} else {
			timer.store(false);
		}
		MLOG_ERROR(mlog::boot, "wokeupFromInterrupt", DVAR(irq));
	}
	void enteredFromSyscall() override {
		MLOG_ERROR(mlog::boot, "enteredFromSyscall");
	}
	void enteredFromInterrupt(uint8_t irq) override {
		MLOG_ERROR(mlog::boot, "enteredFromInterrupt", DVAR(irq));
	}

	bool shouldDeepSleep() {
		if (timer_interrupt.exchange(false) == true && delay_lite_sleep != 0) {
			return true;
		}
		return false;
	}

	void liteSleep() {
		timer.store(true);
	}

	bool shouldLiteSleep() {
		return delay_polling != MAX_UINT32;
	}
private:
	uint32_t delay_polling = 10000;
	uint32_t delay_lite_sleep = 10000000;

	std::atomic<bool> timer {false};
	std::atomic<bool> timer_interrupt {false};
};

} // namespace mythos