#pragma once

#include "app/Task.hh"
#include <array>

class Thread;
class ISignalable {
public:
	virtual void signal() = 0;

  // used for recursive multicast operations
  virtual void addTask(Task::list_t::Queueable *q) = 0;
  virtual uint64_t getID() = 0;
  virtual ~ISignalable() {}
};
