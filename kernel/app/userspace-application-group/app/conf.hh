#pragma once
#include <cstddef>

const size_t NUM_THREADS = 240;
const size_t NUM_HELPER =  19; // declare placement of helpers in HelperThread.cc
const size_t PAGE_SIZE = 2 * 1024 * 1024;
const size_t STACK_SIZE = 1 * PAGE_SIZE;
const size_t REPETITIONS = 1000;