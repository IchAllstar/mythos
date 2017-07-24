#pragma once


class ISignalable;
class Thread;

struct HelperThread {
  Thread *thread {nullptr};
  std::atomic<bool> onGoing {false};
  ISignalable **group {nullptr};
  uint64_t from = 0;
  uint64_t to   = 0;
};
