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

// Prepares tasklet with strategy, which can then be send to destination hardware thread
struct CastStrategy {
    SignalableGroup *group;
    size_t idx;
    CastStrategy(SignalableGroup *group_, size_t idx_)
        : group(group_), idx(idx_) {}

    // Take tasklet and inject strategy 
    virtual void create(Tasklet &t) const = 0;
};

/**
 * Interface for signalable objects.
 * Those objects also have to define methods to forward multicasts.
 */
class ISignalable
{
public:
    virtual ~ISignalable() {}

    virtual optional<void> signal(CapData data) = 0;
    virtual void broadcast(Tasklet* t, SignalableGroup *group, size_t idx, size_t groupSize) = 0;
    virtual void multicast(const CastStrategy &cs) = 0;
};

} // namespace mythos
