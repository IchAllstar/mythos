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



struct TreeCastStrategy : public CastStrategy {
    static const uint64_t LATENCY = 4;

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

    void create(Tasklet &t) const override {
        size_t idx_, from_,to_;
        idx_ = idx;
        from_ = from;
        to_ = to;
        auto group_ = group;
        t.set([group_, idx_, from_, to_](Tasklet*) {
            MLOG_ERROR(mlog::boot, DVAR(idx_), DVAR(from_), DVAR(to_));
            TypedCap<ISignalable> own(group_->getMember(idx_)->cap());
            ASSERT(own);
            ASSERT(group_ != nullptr);
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
                if (j+from_ > 59) {
                    MLOG_ERROR(mlog::boot, DVAR(j+from_));
                }
                TreeCastStrategy tcs(group_, j + from_, j + from_, to_tmp);
                TypedCap<ISignalable> dest(group_->getMember(j + from_)->cap());
                if (dest) {
                    dest->multicast(tcs);
                }
                to_tmp = from_ + j - 1;
            }
        });
    }

};

class SignalableGroup;
std::atomic<uint64_t> counter {0};
class TreeMulticast
{
public:
    static Error multicast(SignalableGroup *group, size_t idx, size_t groupSize) {
        ASSERT(group != nullptr);
        TypedCap<ISignalable> signalable(group->getMember(0)->cap());
        if (signalable) {
            //signalable->broadcast(group->getTasklet(0), group, 0, groupSize);
            TreeCastStrategy tcs(group, 0, 0, groupSize - 1);
            signalable->multicast(tcs);
        }
        return Error::SUCCESS;
    }
private:
    static constexpr uint64_t N_ARY_TREE = 2;
};



} // namespace mythos
