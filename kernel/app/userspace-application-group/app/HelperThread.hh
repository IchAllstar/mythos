#pragma once
#include <atomic>
#include "util/assert.hh"
#include "app/Thread.hh"
#include "app/conf.hh"
#include "app/ThreadManager.hh"

class ISignalable;
class SignalableGroup;

struct HelperThread {
	Thread *thread {nullptr};
	std::atomic<bool> onGoing {false};
	SignalableGroup *group {nullptr};
	//ISignalable **group {nullptr};
	uint64_t from = 0;
	uint64_t to   = 0;

	static void init_helper();
	static HelperThread *getHelper(uint64_t threadID);
	static bool contains(uint64_t id);
	static void* helper_thread(void *data);
};

extern ThreadManager manager;
extern HelperThread helpers[NUM_HELPER];
// Place the helper on following hardware threads
extern uint64_t helperIDs[NUM_HELPER];
extern void* helper_thread(void *data);