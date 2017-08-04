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
	virtual void sleepIntention(uint8_t depth) = 0;
	//NORETURN virtual  void sleep() = 0;
};

constexpr uint32_t MAX_UINT32 = (uint32_t) - 1;

/**
 * Configurable Delay Idle Management
 * Let the user define two values:
 * delay_polling, which defines the duration the hardware thread wil poll the kernel and user task queues before going to lite sleep
 * delay_lite_sleep, which is the time the hardware thread waits in lite sleep until finally going to deep sleep
 * If delay_lite_sleep == 0 then alwys deep sleep, MAX_UINT32 for never deep sleep
 * If delay_polling == MAX_UINT32 then the hardware thread is always polling without entering lite sleep, delay_poling==0 no polling
 */
class IdleManagement : public IIdleManagement {
public:
	enum SLEEP_STATES {
		CC0 = 0,
		CC1 = 1,
		CC6 = 6,
	};

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
		if (timer.exchange(false) == true) {
			if (irq == 0x22) {
				timer_interrupt.store(true);
			} else { // we woke up from another reason but timer was configured, so disable it to avoid useless interrupt
				mythos::lapic.disableTimer();
			}
		}
		MLOG_ERROR(mlog::boot, "wokeupFromInterrupt", DVAR(irq));
	}
	void enteredFromSyscall() override {
		MLOG_ERROR(mlog::boot, "enteredFromSyscall");
	}
	void enteredFromInterrupt(uint8_t irq) override {
		MLOG_ERROR(mlog::boot, "enteredFromInterrupt", DVAR(irq));
	}
	void sleepIntention(uint8_t depth) override {
		MLOG_ERROR(mlog::boot, "received sleep intention", DVAR(depth));
		if (depth == 1 && getDelayLiteSleep() != MAX_UINT32) {
			MLOG_ERROR(mlog::boot, "set timer interrupt", DVAR(getDelayLiteSleep()));
			timer.store(true);
			mythos::lapic.enableOneshotTimer(0x22, getDelayLiteSleep());
		}
	}

public:
	bool shouldDeepSleep() {
		if (timer_interrupt.exchange(false) == true && delay_lite_sleep != MAX_UINT32) {
			return true;
		}
		return false;
	}

	bool shouldLiteSleep() {
		return delay_polling != MAX_UINT32;
	}

	uint8_t getSleepDepth() {
		if (shouldDeepSleep()) {
			return CC6;
		}
	}
private:
	uint32_t delay_polling = 10000; // MAX_UINT32 for polling only
	uint32_t delay_lite_sleep = 10000000; // 0 for always deep sleep, MAX_UINT32 for no deep sleep

	std::atomic<bool> timer {false};
	std::atomic<bool> timer_interrupt {false};
};

} // namespace mythos