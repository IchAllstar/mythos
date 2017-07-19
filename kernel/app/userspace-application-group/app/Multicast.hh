#pragma once

#include <array>
#include "app/ISignalable.hh"
#include "app/Mutex.hh"
#include "app/mlog.hh"

class SequentialStrategy {
public:
	void operator() (void* data, ISignalable **array, uint64_t size) {
		for (uint64_t i = 0; i < size; ++i) {
			if (array[i] != nullptr) {
				array[i]->signal(data);
			}
		}
	}
};

class TreeStrategy {
private:
	static constexpr uint64_t N_ARY_TREE = 2;
public:
	void operator() (void* data, ISignalable **array, uint64_t size) {
		auto signalable = array[0];
		if (signalable) {
            signalable->multicast(array, size, 0, N_ARY_TREE);
            signalable->signal(data);
        }
	}
};

template<typename MULTICAST_STRATEGY = SequentialStrategy>
class SignalableGroup {
private:
	static constexpr uint64_t MAX = 10;
public:
	void addMember(ISignalable *t);
	void signalAll(void *data);
private:
	std::array<ISignalable*, MAX> member{{nullptr}};
	uint64_t size {0};
};

template<typename MULTICAST_STRATEGY>
void SignalableGroup<MULTICAST_STRATEGY>::addMember(ISignalable *t) {
	for (int i = 0; i < MAX; i++) {
		if (member[i] == nullptr) {
			member[i] = t;
			size++;
			return;
		}
	}
	MLOG_ERROR(mlog::app, "Group full");
}

template<typename MULTICAST_STRATEGY>
void SignalableGroup<MULTICAST_STRATEGY>::signalAll(void *data) {
	MLOG_ERROR(mlog::app, "Start signalAll");
	MULTICAST_STRATEGY()(data, &member[0], size);
}