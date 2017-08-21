#include "app/HelperThread.hh"
#include "app/SignalableGroup.hh"

HelperThread helpers[NUM_HELPER];
// Place the helper on following hardware threads
uint64_t helperIDs[NUM_HELPER] = {1, 3, 4, 7, 8, 11, 25, 29, 33, 37};

void HelperThread::init_helper() {
	uint64_t helper_number = 0;
	for (auto i : helperIDs/*uint64_t i = 1; i < NUM_HELPER + 1; ++i*/) {
		manager.deleteThread(*manager.getThread(i));
		manager.initThread(i, &helper_thread);
		ASSERT(manager.getThread(i));
		helpers[helper_number].thread = manager.getThread(i);
		manager.startThread(*manager.getThread(i));
		helper_number++;
	}
}

HelperThread* HelperThread::getHelper(uint64_t threadID) {
	for (auto &h : helpers) {
		if (h.thread->id == threadID) {
			return &h;
		}
	}
	return nullptr;
}

bool HelperThread::contains(uint64_t id) {
	for (auto i : helperIDs) {
		if (id == i) return true;
	}
	return false;
}

void* HelperThread::helper_thread(void *data) {
	ASSERT(data != nullptr);
	auto *thread = reinterpret_cast<Thread*>(data);
	//MLOG_ERROR(mlog::app, "Helper started", DVAR(thread->id));
	auto *helper = HelperThread::getHelper(thread->id);
	ASSERT(helper);
	while (true)  {
		auto prev = thread->SIGNALLED.exchange(false);
		if (prev) {
			//MLOG_ERROR(mlog::app, "Helper: Got Signalled.");
			//do useful stuff
			uint64_t start , end;
			//start = mythos::getTime();
			for (uint64_t i = helper->from; i < helper->to; ++i) {
				//MLOG_ERROR(mlog::app, "wakeup", i);
				helper->group->getMember(i)->signal();
			}
			helper->onGoing.store(false);
			//end = mythos::getTime();
			//MLOG_ERROR(mlog::app, DVAR(end - start));
		}
		Thread::wait(*thread);
	}
}