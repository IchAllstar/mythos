#pragma once

#include <array>
#include "app/ISignalable.hh"
#include "app/Mutex.hh"
#include "app/mlog.hh"
#include "app/Thread.hh"
#include "app/HelperThread.hh"
//#include "util/Time.hh"
#include "app/Task.hh"

// TODO
// mutex in addmember seems to lock?
// keep on using the task abstraction to propagate tree and helper
class SignalableGroup {
private:
  static const size_t MAX = 256;
public:
  void addMember(ISignalable *t);
  void signalAll();
  uint64_t count() { return size; }
  ISignalable* getMember(uint64_t i) { return member[i]; }
  Task* getTask(uint64_t i) { return &tasks[i]; }

private:
  std::array<ISignalable*, MAX> member{{nullptr}};
  std::array<Task, MAX> tasks;
  uint64_t size {0};
  SpinMutex mtx;
};

class SequentialStrategy {
public:
  static void cast(SignalableGroup *group, uint64_t idx, uint64_t size) {
    for (uint64_t i = idx; i < size; ++i) {
      auto *member = group->getMember(i);
      if (member) member->signal();
    }
  }
};

extern const size_t NUM_HELPER;
extern HelperThread helpers[];
class HelperStrategy {
public:

  static void cast(SignalableGroup *group, uint64_t idx, uint64_t size) {
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
      helper.group = group;
      helper.from = current;

      helper.to = helper.from + numHelper - i;
      if (diff > 0) {
        uint64_t times = (diff / (numHelper - i)) + 1;
        helper.to += times;
        diff -= times;
      }

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
  static uint64_t num_helper(uint64_t groupSize) {
    uint64_t value {0};
    uint64_t i {0};
    while (value <= groupSize) {
      i++;
      value = (i * (i + 1)) / 2;
    }
    return (i - 1 > 0) ? i - 1 : 0;
  }
};

/**
 * Tree Strategy
 */
class TreeStrategy {
private:
  static const uint64_t LATENCY = 2;
public:
  // Helper functions to calculate fibonacci tree for optimal multicast
  static size_t F(size_t time) {
    if (time < LATENCY) {
      return 1;
    }
    return F(time - 1) + F(time - LATENCY);
  }

  // index function for F
  static uint64_t f(uint64_t n) {
    if (n == 0) {
      return 0;
    }
    uint64_t i = 1;
    uint64_t current = F(i);
    while (current < n) {
      i++;
      current = F(i);
    }

    if (current == n) {
      return i;
    } else {
      return i - 1;
    }
  }

  static void prepareTask(SignalableGroup *group, uint64_t idx, uint64_t from, uint64_t to) {
    auto *group_ = group;
    auto idx_ = idx;
    auto from_ = from;
    auto to_  = to;
    group->getTask(idx)->set([group_, idx_, from_, to_](Task&) {
      ASSERT(group_ != nullptr); // TODO: handle case when group is not valid anymore
      ASSERT(to_ > 0);
      auto to_tmp = to_;
      while (true) {
        uint64_t n = to_tmp - from_ + 1;
        if ( n < 2) {
          return;
        }
        uint64_t j = TreeStrategy::F(TreeStrategy::f(n) - 1);
        auto child_idx = j + from_;
        //MLOG_ERROR(mlog::app, idx_, "sends to", j + from_);
        ISignalable* dest = group_->getMember(child_idx);
        if (dest) {
          if (child_idx < to_tmp) {
            TreeStrategy::prepareTask(group_, child_idx, child_idx, to_tmp);
            dest->addTask(&group_->getTask(child_idx)->list_member);
          }
          dest->signal();
        }
        to_tmp = from_ + j - 1;
      }
    });
  }

  static void cast(SignalableGroup *group, uint64_t idx, uint64_t size) {
    auto *signalable = group->getMember(idx);
    if (signalable) {
      TreeStrategy::prepareTask(group, 0, 0, size - 1);
      signalable->addTask(&group->getTask(0)->list_member);
      signalable->signal();
    }
  }
};


