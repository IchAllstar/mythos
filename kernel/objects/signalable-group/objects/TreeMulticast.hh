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


#include "objects/ISignalable.hh"
#include "objects/ExecutionContext.hh"
#include "objects/CapRef.hh"
#include "objects/mlog.hh"
#include "util/Time.hh"


namespace mythos {


/**
 * Strategy implementation, which constructs a "fibonacci" multicast tree of the group members.
 * Tree roughly looks like:
 * The first nodes receiving its Signal have the most time to distribute it further.
 * Therefore they get a bigger range of nodes to transfer the Signal to.
 * Treeconstruction is dependent on the LATENCY parameter, which has to be adapted for different hardware.
 */
struct TreeCastStrategy : public CastStrategy {
    // Latency: inverse ratio of sending overhead and complete transfer time
    // on KNC: ~4000 cycles for delivering signal which includes ~1000 cycles of overhead
    // LATENCY = 2 optimal on KNC if no deep sleep
    // higher latency better if threads are in deep sleep
    static const uint64_t LATENCY = 2;

    size_t from;
    size_t to;

    TreeCastStrategy(SignalableGroup *group_, size_t idx_, size_t from_, size_t to_)
        :CastStrategy(group_, idx_), from(from_), to(to_)
    {}

    // Helper functions to calculate fibonacci tree for optimal multicast
    static size_t F(size_t time) {
        if (time < LATENCY) {
            return 1;
        }
        return F(time - 1) + F(time - LATENCY);
    }

    // index function for F
    static uint64_t f(uint64_t n) {
        if (n==0) {
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
            return i-1;
        }
    }

    // create the tasklet, which contains the necessary information to distribute a signal further.
    // Also serves as interface, so the EC does not need to provide the necessary functionality.
    void create(Tasklet &t) const override {
        // Need to copy variables for capturing in lambda
        auto group_ = group;
        size_t idx_, from_,to_;
        idx_ = idx;
        from_ = from;
        to_ = to;
        // Create Tasklet which will be send to destination hardware thread
        t.set([group_, idx_, from_, to_](Tasklet*) {
            MLOG_DETAIL(mlog::boot, DVAR(idx_), DVAR(from_), DVAR(to_));
            TypedCap<ISignalable> own(group_->getMember(idx_)->cap());
            ASSERT(own);
            ASSERT(group_ != nullptr); // TODO: handle case when group is not valid anymore
            ASSERT(to_ > 0);
            auto to_tmp = to_;

            // Signal own EC, will be scheduled after kernel task handling
            own->signal(0);

            while (true) {
                uint64_t n = to_tmp - from_ + 1;
                if ( n < 2) {
                    return;
                }
                uint64_t j = TreeCastStrategy::F(TreeCastStrategy::f(n) - 1);
                TreeCastStrategy tcs(group_, j + from_, j + from_, to_tmp);
                MLOG_DETAIL(mlog::boot, idx_, "sends to", j+from_);
                TypedCap<ISignalable> dest(group_->getMember(j + from_)->cap());
                if (dest) {
                    if (j + from_ < to_tmp) {
                        dest->multicast(tcs);
                    } else { // if leaf node, which does not forward, just signal it
                        dest->signal(0);
                    }
                }
                to_tmp = from_ + j - 1;
            }
        });
    }
};

class SignalableGroup;
class TreeMulticast
{
public:
    static Error multicast(SignalableGroup *group, size_t groupSize) {
        ASSERT(group != nullptr);
        TypedCap<ISignalable> signalable(group->getMember(0)->cap());
        if (signalable) {
            TreeCastStrategy tcs(group, 0, 0, groupSize - 1); // from and to are included
            signalable->multicast(tcs);
        }
        return Error::SUCCESS;
    }
};



} // namespace mythos
