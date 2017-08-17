#pragma once

#include "objects/ISignalable.hh"
#include "boot/DeployHWThread.hh"
#include "boot/mlog.hh"
#include "objects/IdleManagement.hh"
#include "objects/IKernelObject.hh"
#include "mythos/protocol/IdleManagement.hh"
#include "mythos/protocol/KernelObject.hh"
#include "async/NestedMonitorDelegating.hh"
#include <atomic>

namespace mythos {

class IIdleManagement {
	virtual void wokeup(size_t reason) = 0; //wokeup from CC6 as side effect of other HWT wakeup
	virtual void wokeupFromInterrupt(uint8_t irq) = 0; // callback when wokeup from interrupt
	virtual void enteredFromSyscall() = 0; // callback when entered from syscall
	virtual void enteredFromInterrupt(uint8_t irq) = 0; // callback when entered from interrupt
	virtual void sleepIntention(uint8_t depth) = 0; // callback when HWT goes to sleep
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
class IdleManagement : public IIdleManagement, public IKernelObject {
public:
	enum SLEEP_STATES {
		CC0 = 0,
		CC1 = 1,
		CC6 = 6,
	};
//IKernelObject interface
public:
	optional<void> deleteCap(Cap self, IDeleter& del) override;
	void invoke(Tasklet* t, Cap self, IInvocation* msg) override;
	optional<void const*> vcast(TypeId id) const override;
	friend struct protocol::KernelObject;
	Error getDebugInfo(Cap self, IInvocation* msg);
	Error registerHelper(Tasklet*, Cap self, IInvocation* msg);
public:
	uint32_t getDelayPolling() { return delay_polling.load(); }
	uint32_t getDelayLiteSleep() { return delay_lite_sleep.load(); }
public: // IIdleManagement Interface
	void wokeup(size_t reason) override;
	void wokeupFromInterrupt(uint8_t irq) override;
	void enteredFromSyscall() override;
	void enteredFromInterrupt(uint8_t irq) override;
	void sleepIntention(uint8_t depth) override;
public:
	Error setPollingDelay(Tasklet*, Cap self, IInvocation* msg);
	Error setLiteSleepDelay(Tasklet*, Cap self, IInvocation* msg);
public:
	bool shouldDeepSleep();
	bool alwaysDeepSleep();
	bool alwaysPoll();
	//uint8_t getSleepDepth();
private:
	IDeleter::handle_t del_handle = {this};
	async::NestedMonitorDelegating monitor;
	// time in cycles we spend polling before going to sleep
  std::atomic<uint32_t> delay_polling = {0};

	// time we wait in lite sleep before going to deep sleep
  //std::atomic<uint32_t> delay_lite_sleep = {MAX_UINT32};
	std::atomic<uint32_t> delay_lite_sleep = {0};

	// indicates a timer interrupt was set
	std::atomic<bool> timer {false};
	// indicates a timer interrupt actually triggered
	std::atomic<bool> timer_interrupt {false};
};

} // namespace mythos
