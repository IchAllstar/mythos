/* -*- mode:C++; indent-tabs-mode:nil; -*- */
/* MIT License -- MyThOS: The Many-Threads Operating System
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright 2016 Randolf Rotta, Robert Kuban, and contributors, BTU Cottbus-Senftenberg
 */
#pragma once

#include "mythos/protocol/common.hh"
#include "mythos/protocol/KernelMemory.hh"
#include "util/error-trace.hh"
#include "app/mlog.hh"

namespace mythos {
  namespace protocol {

    struct IdleManagement {
      constexpr static uint8_t proto = IDLE_MANAGEMENT;

      enum Methods : uint8_t {
        SET_POLLING_DELAY,
        SET_LITE_SLEEP_DELAY,
      };

      struct SetPollingDelay : public InvocationBase {
        constexpr static uint16_t label = (proto<<8) + SET_POLLING_DELAY;
        SetPollingDelay(uint32_t delay_) : InvocationBase(label,getLength(this)) {
          delay = delay_;
        }
        uint64_t delay;
      };

      struct SetLiteSleepDelay : public InvocationBase {
        constexpr static uint16_t label = (proto<<8) + SET_LITE_SLEEP_DELAY;
        SetLiteSleepDelay(uint64_t delay_) : InvocationBase(label,getLength(this)) {
          delay = delay_;
        }
        uint64_t delay;
      };

      template<class IMPL, class... ARGS>
      static Error dispatchRequest(IMPL* obj, uint8_t m, ARGS const&...args) {
        switch(Methods(m)) {
          case SET_POLLING_DELAY: return obj->setPollingDelay(args...);
          case SET_LITE_SLEEP_DELAY: return obj->setLiteSleepDelay(args...);
          default: return Error::NOT_IMPLEMENTED;
        }
      }
    };

  }// namespace protocol
}// namespace mythos
