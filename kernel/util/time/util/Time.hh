#pragma once

#include <cstdint>

namespace mythos {

uint64_t getTime() {
	uint64_t hi, lo;
	asm volatile("rdtsc":"=a"(lo), "=d"(hi));
	return (lo) | (hi << 32);
}

} // namespace mythos

