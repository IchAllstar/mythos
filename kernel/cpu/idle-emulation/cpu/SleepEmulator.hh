#pragma once
#include "boot/mlog.hh"

namespace mythos {

struct AvailableState {
	uint64_t id; // corresponds to the depth given to the sleep call; CC6 = 6 and so on
	const char* name;
	uint64_t exit_latency;

	AvailableState(uint64_t id_, const char* name_, uint64_t exit_latency_)
		:id(id_), name(name_), exit_latency(exit_latency_) {}
};

AvailableState available[3] =
{
  	{
		/*.id =*/			 0,
		/*.name = */		 "C0",
		/*.exit_latency = */ 0,
	},
	{
		/*.id =*/			 1,
		/*.name = */		 "C1",
		/*.exit_latency = */ 0,	// hlt state available in real
	},
	{
		/*.id =*/			 6,
		/*.name = */		 "C6",
		/*.exit_latency = */ 4000,
	}
};

/**
 * Sleep Emulation supports different sleep states defined in available[].
 */
class SleepEmulator {
public:
	void sleep(uint64_t threadID, uint64_t depth);
	void wakeup(uint64_t threadID);

public:
	static const uint64_t HWTHREADS = 4;
private:
	void delay(uint64_t depth) {
        ASSERT(validState(depth));
        MLOG_DETAIL(mlog::boot, "Simulate exit latency of C-state", depth);
        if (available[depth].exit_latency > 0) {
            mythos::hwthread_pause(available[depth].exit_latency);
        }
	}

    static inline uint64_t getCore(uint64_t threadID) { return threadID / HWTHREADS; }
    static inline uint64_t getHWThreadOnCore(uint64_t threadID) { return threadID % HWTHREADS; }

	uint64_t setState(uint64_t threadID, uint64_t depth);
	uint64_t getState(uint64_t threadID);
	bool validState(uint64_t depth);
	uint64_t minState(uint64_t core);
    const AvailableState* getStateWithID(uint64_t id);
};

struct CoreState {
	std::atomic<bool> locked = {false};
	volatile uint64_t cstate[SleepEmulator::HWTHREADS] {0};
  	volatile bool sleep = false;

  	void lock() { while(locked.exchange(true) == true); }
	void unlock() { locked.store(false); }
};
extern CoreState states[];

void SleepEmulator::sleep(uint64_t threadID, uint64_t depth) {
	auto core = SleepEmulator::getCore(threadID);
	states[core].lock();
    auto prev = setState(threadID, depth);
    uint64_t min = minState(core);
	MLOG_DETAIL(mlog::boot, "Sleep", DVAR(threadID), DVAR(prev), DVAR(depth), DVAR(min));
	if (min > 0) { // every hw thread wants to join a sleep state
	   MLOG_DETAIL(mlog::boot, "Go to deep sleep", DVAR(min));
	}
	states[core].unlock();
}

void SleepEmulator::wakeup(uint64_t threadID) {
	auto core = SleepEmulator::getCore(threadID);
	states[core].lock();
    auto min = minState(core);
	auto prev = setState(threadID, 0);
	MLOG_DETAIL(mlog::boot, "wakeup", DVAR(threadID), DVAR(prev), DVAR(min));
  	if (prev > 0) { // wakeup from deep sleep sets other hwthreads into CC1 Lite Sleep
        if (min > 0) {
            states[core].unlock();
            MLOG_DETAIL(mlog::boot, "Wakeup from sleep state", DVAR(min));
            delay(min);
            return;
        }
	}
	states[core].unlock();
}

uint64_t SleepEmulator::setState(uint64_t threadID, uint64_t depth) {
	ASSERT(validState(depth));
	uint64_t core = SleepEmulator::getCore(threadID);
	uint64_t hwthread = threadID % HWTHREADS;
	auto prev = states[core].cstate[hwthread];
    states[core].cstate[hwthread]  = depth;
	return prev;
}

uint64_t SleepEmulator::getState(uint64_t threadID) {
	uint64_t core = SleepEmulator::getCore(threadID);
	uint64_t hwthread = threadID % HWTHREADS;
	auto state = states[core].cstate[hwthread];
	return state;
}

bool SleepEmulator::validState(uint64_t depth) {
	for (const auto & a : available) {
		if (a.id == depth) {
			return true;
		}
	}
	return false;
}

const AvailableState* SleepEmulator::getStateWithID(uint64_t id) {
  for (const auto &a : available) {
    if (a.id == id) {
      return &a;
    }
  }
  return nullptr;
}

uint64_t SleepEmulator::minState(uint64_t core) {
	uint64_t min = (uint64_t (-1));
    for (uint64_t i = 0; i < HWTHREADS; i++) {
      if (states[core].cstate[i] < min) min = states[core].cstate[i];
    }
    return min;
}


} // namespace mythos
