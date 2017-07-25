#pragma once

#include <array>
#include "app/ISignalable.hh"
#include "app/Mutex.hh"
#include "app/mlog.hh"
#include "app/Thread.hh"
#include "app/HelperThread.hh"
#include "util/Time.hh"

class SequentialStrategy {
public:
	void operator() (ISignalable **array, uint64_t size) {
	  MLOG_ERROR(mlog::app, "Start signalAll sequential");
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
	  MLOG_ERROR(mlog::app, "Start signalAll Tree");
		auto signalable = array[0];
		if (signalable) {
      signalable->cast.set(array,size,0,N_ARY_TREE);
      signalable->signal();
    }
	}
};

extern const size_t NUM_HELPER;
extern HelperThread helpers[];
class HelperStrategy {
public:
  void operator() (ISignalable **array, uint64_t size) {
    if (size == 0) return;

    uint64_t numHelper = (num_helper(size) < NUM_HELPER) ? num_helper(size) : NUM_HELPER;
    uint64_t numOptimal = (numHelper * (numHelper + 1)) / 2;
    ASSERT(numOptimal <= size);
    uint64_t diff = size - numOptimal;
    //MLOG_ERROR(mlog::app, DVAR(numHelper), DVAR(numOptimal), DVAR(diff));
    auto divide = size / numHelper;
    auto mod    = size % numHelper;
    ASSERT(mod <= numHelper);
    uint64_t current = 0;

    for (uint64_t i = 0; i < numHelper; ++i) {
      auto &helper = helpers[i];
      helper.group = array;
      helper.from = current;

      helper.to = helper.from + numHelper - i;
      if (diff > 0) {
        uint64_t times = (diff / (numHelper - i)) + 1;
        helper.to += times;
        diff -= times;
      }

/*
      helper.to = (current + divide) < size? current + divide : size;
      if (mod > 0) {
        helper.to++;
        mod--;
      }
*/
      if (helper.to > size) helper.to = size;

      current = helper.to;
      //MLOG_ERROR(mlog::app, "Helper", i, DVAR(helper.from), DVAR(helper.to));
      auto prev = helper.onGoing.exchange(true);
      if (prev) {
        MLOG_ERROR(mlog::app, "Ongoing cast");
      }
      helper.thread->signal();
    }
  }

private:
  uint64_t num_helper(uint64_t groupSize) {
    uint64_t value {0};
    uint64_t i {0};
    while (value < groupSize) {
      i++;
      value = (i * (i + 1)) / 2;
    }
    return (i-1 > 0) ? i-1 : 0;
  }
};


template<size_t MAX = 100, typename MULTICAST_STRATEGY = SequentialStrategy>
class SignalableGroup {
private:
public:
	void addMember(ISignalable *t);
	void signalAll();
  uint64_t count() { return size; }
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
	MULTICAST_STRATEGY()(&member[0], size);
}
