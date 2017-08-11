#pragma once

//#include "app/Thread.hh"
//#include "app/Mutex.hh"
#include "app/Task.hh"
#include <array>

class ISignalable {
public:
	virtual void signal() = 0;

  // used for recursive multicast operations
  virtual void addTask(Task::list_t::Queueable *q) = 0;
  virtual ~ISignalable() {}
};
