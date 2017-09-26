#pragma once
#include "app/IntrusiveList.hh"
#include <new>
#include <cstddef>
#include <utility>

/**
 * Simplified Task inspired from Tasklet. Queueable in a simply locked queue.
 */
struct Task {
public:
  typedef void(*FunPtr)(Task&);
  typedef mythos::LinkedList<Task*> list_t;
  typedef typename list_t::Queueable handle_t;
  enum STATE {
    USED,
    UNUSED,
  };

  void run() {
    fun(*this);
    state.store(UNUSED);
  }

  template<class FUNCTOR>
  void set(FUNCTOR const&& fun_) {
    static_assert(sizeof(FUNCTOR) <= sizeof(PAYLOAD), "tasklet payload is too big");
    new(PAYLOAD) FUNCTOR(std::move(fun_));
    fun = &wrapper<FUNCTOR>;
    state.store(USED);
  }

  template<class MSG>
  MSG get() const {
    static_assert(sizeof(MSG) <= sizeof(PAYLOAD), "tasklet payload is too big");
    union cast_t {
      char PAYLOAD[64];
      MSG msg;
    };
    cast_t const* caster = reinterpret_cast<cast_t const*>(PAYLOAD);
    return MSG(std::move(caster->msg));
  }

  bool isUnused() { return state.load() == UNUSED; }

  template<class FUNCTOR>
  static void wrapper(Task& msg) { msg.get<FUNCTOR>()(msg); }
public:
  handle_t list_member {this};
private:
  FunPtr fun {nullptr};
  std::atomic<STATE> state {UNUSED};
  char PAYLOAD[64 - sizeof(fun) - sizeof(state) - sizeof(list_member)];
  static_assert(64 - sizeof(fun) - sizeof(state) - sizeof(list_member) < 0x1000, "payload to big");
};
