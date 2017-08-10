#pragma once

#include <array>
#include "app/ISignalable.hh"
#include "app/Mutex.hh"
#include "app/mlog.hh"
#include "app/Thread.hh"
#include "app/HelperThread.hh"
#include "util/Time.hh"
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
  //Multicast& getCast(uint64_t i) { return casts[i]; }
  Task* getTask(uint64_t i) { return &tasks[i]; }

private:
  std::array<ISignalable*, MAX> member{{nullptr}};
  //std::array<Multicast, MAX> casts;
  std::array<Task, MAX> tasks;
  //Tasklet t[210];
  uint64_t size {0};
  SpinMutex mtx;
};

class SequentialStrategy {
public:
  void operator() (SignalableGroup *group, ISignalable **array, uint64_t size) {
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
    static void prepareTask(SignalableGroup *group, uint64_t idx, uint64_t size) {
        auto *group_ = group;
        auto idx_ = idx;
        auto size_ = size;
        group->getTask(idx)->set([group_, idx_, size_](Task&) {
            for (size_t i = 0; i < 2; ++i) { // for all children in tree
                size_t child_idx = idx_ * 2 + i + 1;
                if (child_idx >= size_) {
                  return;
                }
                //MLOG_ERROR(mlog::app, DVAR(this->id), DVAR(mc.idx), DVAR(child_idx));
                ISignalable *signalable = group_->getMember(child_idx); // child
                if (signalable) {
                  //MLOG_ERROR(mlog::app, "Signal", DVAR(child_idx));
                  TreeStrategy::prepareTask(group_, child_idx, size_);
                  signalable->addTask(&group_->getTask(child_idx)->list_member);
                  signalable->signal();
                } else {
                  MLOG_ERROR(mlog::app, "Child not there", DVAR(child_idx));
                }
            }
        });
    }

  void operator() (SignalableGroup *group, ISignalable **array, uint64_t size) {
    MLOG_ERROR(mlog::app, "Start signalAll Tree");
    auto signalable = array[0];
    if (signalable) {
      //signalable->cast.set(array,size,0,N_ARY_TREE);
      //signalable->group = group;
       /* for (int i = 0; i < size; i++) {
            auto *sign = group->getMember(i);
            MLOG_ERROR(mlog::app, i, " ", DVAR(sign));
            sign->signal();
        }
        */
      TreeStrategy::prepareTask(group, 0, size);
      signalable->addTask(&group->getTask(0)->list_member);
      signalable->signal();
    }
  }
};

extern const size_t NUM_HELPER;
extern HelperThread helpers[];
class HelperStrategy {
public:

  void operator() (SignalableGroup *group, ISignalable **array, uint64_t size) {
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
    while (value <= groupSize) {
      i++;
      value = (i * (i + 1)) / 2;
    }
    return (i-1 > 0) ? i-1 : 0;
  }
};




void SignalableGroup::addMember(ISignalable *t) {
      for (int i = 0; i < MAX; i++) {
        //MLOG_ERROR(mlog::app, "Add ", i);
            if (member[i] == nullptr) {
                member[i] = t;
                size++;
                return;
            }
        }
        MLOG_ERROR(mlog::app, "Group full");
  }

void SignalableGroup::signalAll() {
	TreeStrategy()(this, &member[0], size);
}
