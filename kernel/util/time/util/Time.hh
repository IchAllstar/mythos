#pragma once

namespace mythos {
	
uint64_t getTime() {
	uint64_t hi, lo;
	asm volatile("rdtsc":"=a"(lo), "=d"(hi));
	return ((uint64_t)lo) | ( ((uint64_t)hi) << 32);
}

} // namespace mythos

