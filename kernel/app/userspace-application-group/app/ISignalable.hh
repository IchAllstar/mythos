#pragma once

#include "app/Thread.hh"
#include <array>

class ISignalable;

struct Multicast {
	std::atomic<bool> onGoing {false};
	ISignalable **group;
	size_t groupSize {0};
	size_t idx {0}; // index in array for calculation of children
	size_t N {0}; //N-ary Tree

	void set(ISignalable **group_, size_t groupSize_, size_t idx_, size_t N_) {
		while (onGoing.exchange(true) == true) {
			MLOG_ERROR(mlog::app, "Waiting for ongoing cast to end");
		}
		// possible race here with setting and reading the values
		group = group_;
		groupSize = groupSize_;
		idx = idx_;
		N = N_;
	}

	void reset() {
		onGoing.store(false);
		group = nullptr;
		groupSize = 0;
		idx = 0;
	}
};

class ISignalable {
public:
	virtual void signal(void *data) = 0;
	virtual void multicast(ISignalable **group, uint64_t groupSize, uint64_t idx, uint64_t N) = 0;

public:
	Multicast cast;
};