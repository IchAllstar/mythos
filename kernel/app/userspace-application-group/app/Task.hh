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

  run() {
    fun(*this);
  }

  template<class FUNCTOR>
  void set(FUNCTOR const&& fun_) {
    static_assert(sizeof(FUNCTOR) <= sizeof(PAYLOAD), "tasklet payload is too big");
    new(PAYLOAD) FUNCTOR(std::move(fun_));
    fun = &wrapper<FUNCTOR>;
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

  template<class FUNCTOR>
  static void wrapper(Task& msg) { msg.get<FUNCTOR>()(msg); }
private:
  FunPtr fun {nullptr};
  char PAYLOAD[64];
public:
  handle_t list_member {this};
};