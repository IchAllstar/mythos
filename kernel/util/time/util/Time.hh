#pragma once

#include <cstdint>

namespace mythos {

uint64_t getTime() {
	uint64_t hi, lo;
	asm volatile("rdtsc":"=a"(lo), "=d"(hi));
	return (lo) | (hi << 32);
}

class Timer {
public:
	void start() {
		start_ = getTime();
	}

	uint64_t end() {
		return getTime() - start_;
	}

private:
	uint64_t start_;
};

} // namespace mythos

