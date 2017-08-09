#pragma once
#include "cpu/hwthread_pause.hh"
#include <atomic>

class SpinMutex {
public:
	void lock() {
		while (flag.exchange(true) == true) { /*mythos::hwthread_pause();*/ }
	}

	void unlock() {
		flag.store(false);
	}

	template<typename FUNCTOR>
	void operator<<(FUNCTOR fun) {
		lock();
		fun();
		unlock();
	}

private:
	std::atomic<bool> flag;
};


template<typename MUTEX>
class LockGuard {
public:
  LockGuard(MUTEX &m_)
    :m(m_) {
    m.lock();
  }

  ~LockGuard() {
    m.unlock();
  }

private:
    MUTEX &m;
};
