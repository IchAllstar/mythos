#pragma once

#include <array>
#include "app/ISignalable.hh"
#include "app/Mutex.hh"
#include "app/mlog.hh"
#include "app/Thread.hh"

class SequentialStrategy {
public:
	void operator() (ISignalable **array, uint64_t size) {
		for (uint64_t i = 0; i < size; ++i) {
			if (array[i] != nullptr) {
				array[i]->signal();
			}
		}
	}
};

class TreeStrategy {
private:
	static constexpr uint64_t N_ARY_TREE = 2;
public:
	void operator() (ISignalable **array, uint64_t size) {
		auto signalable = array[0];
		if (signalable) {
      signalable->cast.set(array,size,0,N_ARY_TREE);
      signalable->signal();
    }
	}
};

extern ThreadManager manager;
class HelperStrategy {
public:
  void operator() (ISignalable **array, uint64_t size) {
    auto divide = size / NUM_HELPER_THREADS;
    uint64_t current = 0;
    for (uint64_t i  = 0; i < NUM_HELPER_THREADS; i++) {

    }
  }
};

template<size_t MAX = 100, typename MULTICAST_STRATEGY = SequentialStrategy>
class SignalableGroup {
private:
public:
	void addMember(ISignalable *t);
	void signalAll();
private:
	std::array<ISignalable*, MAX> member{{nullptr}};
	uint64_t size {0};
};

template<size_t MAX, typename MULTICAST_STRATEGY>
void SignalableGroup<MAX, MULTICAST_STRATEGY>::addMember(ISignalable *t) {
	for (int i = 0; i < MAX; i++) {
		if (member[i] == nullptr) {
			member[i] = t;
			size++;
			return;
		}
	}
	MLOG_ERROR(mlog::app, "Group full");
}

template<size_t MAX, typename MULTICAST_STRATEGY>
void SignalableGroup<MAX, MULTICAST_STRATEGY>::signalAll() {
	MLOG_ERROR(mlog::app, "Start signalAll");
	MULTICAST_STRATEGY()(&member[0], size);
}
