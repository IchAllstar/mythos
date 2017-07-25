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

namespace mythos {
  namespace protocol {

    struct SignalableGroup {
      constexpr static uint8_t proto = SIGNALABLE_GROUP;

      enum Methods : uint8_t {
        SIGNAL_ALL,
        ADD_MEMBER,
        REMOVE_MEMBER,
      };

      struct SignalAll : public InvocationBase {
        constexpr static uint16_t label = (proto<<8) + SIGNAL_ALL;
        SignalAll() : InvocationBase(label,getLength(this)) {}

        uint64_t data;
      };

      struct AddMember : public InvocationBase {
        constexpr static uint16_t label = (proto<<8) + ADD_MEMBER;
        AddMember(CapPtr signalable) : InvocationBase(label,getLength(this)) {
           addExtraCap(signalable);
        }
        CapPtr signalable() const { return this->capPtrs[0]; }
      };

      struct RemoveMember : public InvocationBase {
        constexpr static uint16_t label = (proto<<8) + REMOVE_MEMBER;
        RemoveMember(CapPtr signalable) : InvocationBase(label,getLength(this)) {
          addExtraCap(signalable);
        }
        CapPtr signalable() const { return this->capPtrs[0]; }
      };

      struct Create : public KernelMemory::CreateBase {
        Create(CapPtr dst, CapPtr factory, size_t groupSize)
          : CreateBase(dst, factory), groupSize(groupSize)
        {}
        size_t groupSize;
      };

      template<class IMPL, class... ARGS>
      static Error dispatchRequest(IMPL* obj, uint8_t m, ARGS const&...args) {
        switch(Methods(m)) {
          case SIGNAL_ALL: return obj->signalAll(args...);
          case ADD_MEMBER: return obj->addMember(args...);
          case REMOVE_MEMBER: return obj->removeMember(args...);
          default: return Error::NOT_IMPLEMENTED;
        }
      }
    };

  }// namespace protocol
}// namespace mythos
