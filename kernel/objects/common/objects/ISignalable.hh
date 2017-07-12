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

#include "cpu/CoreLocal.hh"
#include <atomic>

namespace mythos {
  class SignalableGroup;
  class ISignalable;

  struct Broadcast {
    std::atomic<bool> onGoing {false};
    CapRef<SignalableGroup, ISignalable> *group {nullptr};
    size_t groupSize {0};
    size_t idx {0}; // index in array for calculation of children
    size_t N {0}; //N-ary Tree

    void set(CapRef<SignalableGroup, ISignalable> *group_, size_t groupSize_, size_t idx_, size_t N_) {
    	if (onGoing.exchange(true) == false) {
    		group = group_;
    		groupSize = groupSize_;
    		idx = idx_;
    		N = N_;
    	}
    }

    void reset() {
    	onGoing.store(false);
    	group = nullptr;
    	groupSize = 0;
    	idx = 0;
    }
  };

  class ISignalable
  {
  public:
    virtual ~ISignalable() {}

    virtual optional<void> signal(CapData data);
    virtual void broadcast(CapRef<SignalableGroup, ISignalable> *group, size_t groupSize, size_t idx, size_t N);
  public:
  	Broadcast bc;
  };


} // namespace mythos
