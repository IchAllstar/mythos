#pragma once
#include "boot/mlog.hh"
#include "app/Mutex.hh"

namespace mythos {

struct AvailableState {
	uint64_t id; // corresponds to the depth given to the sleep call; CC6 = 6 and so on
	const char* name;
	uint64_t exit_latency;

	AvailableState(uint64_t id_, const char* name_, uint64_t exit_latency_)
		:id(id_), name(name_), exit_latency(exit_latency_) {}
};

AvailableState available[2] = 
{
	{
		/*.id =*/			 1,
		/*.name = */		 "C1",
		/*.exit_latency = */ 20,
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
	void delay(uint64_t cycles) {
		mythos::hwthread_pause(cycles);
	}

	uint64_t setState(uint64_t threadID, uint64_t depth);
	uint64_t getState(uint64_t threadID);
	bool validState(uint64_t depth);
};

struct CoreState {
	std::atomic<uint64_t> cstate[SleepEmulator::HWTHREADS] {{0}}; // 16 bits for 4 hardware threads to indicate everyones state
	std::atomic<bool> locked = {false};

	void lock() { while(locked.exchange(true) == true); }
	void unlock() { locked.store(false); }
};
extern CoreState states[];

void SleepEmulator::sleep(uint64_t threadID, uint64_t depth) {
	auto core = threadID / HWTHREADS;
	while(states[core].locked.exchange(true) == true);
	//MLOG_ERROR(mlog::boot, "Sleep", DVAR(threadID), DVAR(depth));
	setState(threadID, depth);
	//MLOG_ERROR(mlog::boot, DVAR(getState(threadID)));
	states[core].locked.store(false);
}

void SleepEmulator::wakeup(uint64_t threadID) {
	auto core = threadID / HWTHREADS;
	//while(states[core].locked.exchange(true) == true);
	//MLOG_ERROR(mlog::boot, "wakeup", DVAR(threadID));
	setState(threadID, 0);
	//states[core].locked.store(false);
}

uint64_t SleepEmulator::setState(uint64_t threadID, uint64_t depth) {
	ASSERT(depth < 256);
	uint64_t core = threadID / HWTHREADS;
	uint64_t hwthread = threadID % HWTHREADS;
	auto prev = states[core].cstate[hwthread].exchange(depth);
	//MLOG_ERROR(mlog::boot, "Set State", DVAR(prev), DVAR(depth));
	return prev;
}

uint64_t SleepEmulator::getState(uint64_t threadID) {
	uint64_t core = threadID / HWTHREADS;
	uint64_t hwthread = threadID % HWTHREADS;
	auto state = states[core].cstate[hwthread].load();
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

} // namespace mythos