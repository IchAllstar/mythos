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

#include "runtime/PortalBase.hh"
#include "mythos/protocol/Example.hh"
#include "runtime/KernelMemory.hh"
#include "mythos/init.hh"

namespace mythos {

  class Example : public KObject
  {
  public:
    Example(CapPtr cap) : KObject(cap) {}

    PortalFuture<void> create(PortalLock pr, KernelMemory kmem,
                              CapPtr factory = init::EXAMPLE_FACTORY) {
      return pr.invoke<protocol::Example::Create>(kmem.cap(), _cap, factory);
    }

    PortalFuture<void> printMessage(PortalLock pr, char const* str, size_t bytes) {
      return pr.invoke<protocol::Example::PrintMessage>(_cap, str, bytes);
    }

    PortalFuture<void> ping(PortalLock pr, size_t wait_cycles) {
      return pr.invoke<protocol::Example::Ping>(_cap, wait_cycles);
    }

    PortalFuture<void> moveHome(PortalLock pr, size_t new_location) {
      return pr.invoke<protocol::Example::MoveHome>(_cap, new_location);
    }
  };

  class ExampleHome : public KObject
  {
  public:
    ExampleHome(CapPtr cap) : KObject(cap) {}

    PortalFuture<void> create(PortalLock pr, KernelMemory kmem,
                              CapPtr factory = init::EXAMPLE_HOME_FACTORY) {
      return pr.invoke<protocol::ExampleHome::Create>(kmem.cap(), _cap, factory);
    }

    PortalFuture<void> printMessage(PortalLock pr, char const* str, size_t bytes) {
      return pr.invoke<protocol::ExampleHome::PrintMessage>(_cap, str, bytes);
    }

    PortalFuture<void> ping(PortalLock pr, size_t wait_cycles) {
      return pr.invoke<protocol::ExampleHome::Ping>(_cap, wait_cycles);
    }

    PortalFuture<void> moveHome(PortalLock pr, size_t new_location) {
      return pr.invoke<protocol::ExampleHome::MoveHome>(_cap, new_location);
    }
  };
} // namespace mythos
