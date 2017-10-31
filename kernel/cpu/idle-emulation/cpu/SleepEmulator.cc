#include "cpu/CoreLocal.hh"
#include "cpu/SleepEmulator.hh"

namespace mythos {

CoreState states[MYTHOS_MAX_THREADS];

void SleepEmulator::delay(uint64_t depth) {
        ASSERT(validState(depth));
        auto state = getStateWithID(depth);
        if (state && state->exit_latency > 0) {
        	MLOG_DETAIL(mlog::boot, "Simulate exit latency of C-state", depth);
          mythos::delay(state->exit_latency);
        }
	}

void SleepEmulator::sleep(uint64_t threadID, uint64_t depth) {
	if (depth <= 1) return;
  auto core = SleepEmulator::getCore(threadID);
	states[core].lock();
  auto prev = setState(threadID, depth);
  uint64_t min = minState(core);
	MLOG_DETAIL(mlog::boot, "Sleep", DVAR(threadID), DVAR(prev), DVAR(depth), DVAR(min));
	if (min > 0) { // every hw thread wants to join a sleep state
	   states[core].sleep.store(true);
	}
	states[core].unlock();
}

void SleepEmulator::wakeup(uint64_t threadID) {
  auto core = SleepEmulator::getCore(threadID);
	states[core].lock();
  auto min = minState(core);
	auto prev = setState(threadID, 0);
	MLOG_DETAIL(mlog::boot, "wakeup", DVAR(threadID), DVAR(prev), DVAR(min));
  if (prev > 1) {
    if (min > 1) {
          states[core].unlock();
          delay(min);
          states[core].sleep.store(false);
          return;
      }
	}
	states[core].unlock();
	while(states[core].sleep.load() == true) { }
}

uint64_t SleepEmulator::getSleepState(uint64_t threadID) {
    auto core = SleepEmulator::getCore(threadID);
    states[core].lock();
    if (states[core].sleep.load() == true) {
      states[core].unlock();
      return 6;
    }
    auto min = minState(core);
    states[core].unlock();
    return min;
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
	if (getStateWithID(depth)) {
		return true;
	}
	return false;
}

optional<VirtualIdleState*> SleepEmulator::getStateWithID(uint64_t id) {
  for (auto &a : available) {
    if (a.id == id) {
      return &a;
    }
  }
  return {nullptr};
}

uint64_t SleepEmulator::minState(uint64_t core) {
	  uint64_t min = (uint64_t (-1));
    for (uint64_t i = 0; i < HWTHREADS; i++) {
      if (states[core].cstate[i] < min) min = states[core].cstate[i];
    }
    return min;
}

}
