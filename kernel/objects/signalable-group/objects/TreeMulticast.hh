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
 *
 * Can be seen as shared context to hold necessary values, that are needed to
 * transfer to the dest node.
 */

struct TreeCastStrategy : public CastStrategy {
    // Latency: inverse ratio of sending overhead and complete transfer time
    // LATENCY = 2 optimal on KNC if no deep sleep
    // higher latency better if threads are in deep sleep
    // could check everything for deep sleep?
    // could skip deep sleep nodes and signal children from own context
    static const uint64_t LATENCY = 10;
    static int64_t tmp[20];
    size_t from;
    size_t to;


    TreeCastStrategy(SignalableGroup *group_, size_t idx_, size_t from_, size_t to_)
        :CastStrategy(group_, idx_), from(from_), to(to_)
    {}

    // Helper functions to calculate fibonacci tree for optimal multicast
    // saves result of previous calcs to optimize expensive recursion
    static size_t F(size_t time) {
        if (time < LATENCY) {
            return 1;
        }
        if (time < 20 && tmp[time] > 0) {
          return tmp[time];
        }
        auto ret = F(time - 1) + F(time - LATENCY);
        if (time < 20) {
          tmp[time] = ret;
        }
        return ret;
    }

    // index function for F
    static uint64_t f(uint64_t n) {
        Timer t;
        t.start();
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
    // Also serves as interface, so the EC just calls creates and executes the tasklet on its hardware thread.
    void create(Tasklet &t) const override {
        // Need to copy variables for capturing in lambda
        auto group_ = group;
        size_t idx_, from_,to_;
        idx_ = idx;
        from_ = from;
        to_ = to;
        // Create Tasklet which will be send to destination hardware thread
        t.set([group_, idx_, from_, to_](Tasklet*) {
            MLOG_DETAIL(mlog::boot,DVAR(group_), DVAR(idx_), DVAR(from_), DVAR(to_));
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
                // calculates the split of the range depending on LATENCY
                // left range is handled by this, right by other node
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

    static void multicast(SignalableGroup *group, uint64_t idx, uint64_t from, uint64_t to) {
        auto t = group->getTasklet(idx);
        TypedCap<ISignalable> own(group->getMember(idx)->cap());
        if (not own) PANIC("signalable not valid anymore");
        auto sleepState = own->getSleepState();
        if (sleepState < 2) {
            t->set([group, idx, from, to](Tasklet*) {
                MLOG_DETAIL(mlog::boot,DVAR(group), DVAR(idx), DVAR(from), DVAR(to));
                TypedCap<ISignalable> own(group->getMember(idx)->cap());
                ASSERT(own);
                ASSERT(group != nullptr); // TODO: handle case when group is not valid anymore
                ASSERT(to > 0);
                auto to_tmp = to;

                // Signal own EC, will be scheduled after kernel task handling
                own->signal(0);

                while (true) {
                    uint64_t n = to_tmp - from + 1;
                    if ( n < 2) {
                        return;
                    }
                    // calculates the split of the range depending on LATENCY
                    // left range is handled by this, right by other node
                    uint64_t j = TreeCastStrategy::F(TreeCastStrategy::f(n) - 1);
                    auto destID = j+from;
                    if (destID < to_tmp) {
                        multicast(group, destID, destID, to_tmp);
                    } else { // if leaf node, which does not forward, just signal it
                        TypedCap<ISignalable> dest(group->getMember(destID)->cap());
                        dest->signal(0);
                    }
                    to_tmp = destID - 1;
                }
            });
            auto sched = own->getScheduler();
            if (sched) {
                sched->run(t);
            }
        } else {
            auto to_tmp = to;

            // Signal own EC, will be scheduled after kernel task handling
            own->signal(0);

            while (true) {
                uint64_t n = to_tmp - from + 1;
                if ( n < 2) {
                    return;
                }
                // calculates the split of the range depending on LATENCY
                // left range is handled by this, right by other node
                uint64_t j = TreeCastStrategy::F(TreeCastStrategy::f(n) - 1);
                auto destID = j+from;
                if (destID < to_tmp) {
                    multicast(group, destID, destID, to_tmp);
                } else { // if leaf node, which does not forward, just signal it
                    TypedCap<ISignalable> dest(group->getMember(destID)->cap());
                    dest->signal(0);
                }
                to_tmp = destID - 1;
            }
        }
    }

};


/**
 * N-Ary tree for comparison with the Fibonacci Tree approach
 */
struct NaryTree : public CastStrategy {
    NaryTree(SignalableGroup *group_, uint64_t idx_, uint64_t size_)
      :CastStrategy(group_, idx_), size(size_)
    {}

    void create(Tasklet &t) const override {
        // Need to copy variables for capturing in lambda
        auto group_ = group;
        uint64_t idx_ = idx;
        uint64_t size_ = size;
        uint64_t N_ = 5;
        // Create Tasklet which will be send to destination hardware thread
        t.set([group_, idx_, size_, N_](Tasklet*) {
            MLOG_DETAIL(mlog::boot, DVAR(group_), DVAR(idx_), DVAR(size_));
            ASSERT(idx_ < size_);
            ASSERT(group_ != nullptr); // TODO: handle case when group is not valid anymore
            TypedCap<ISignalable> own(group_->getMember(idx_)->cap());
            ASSERT(own);

            // Signal own EC, will be scheduled after kernel task handling
            own->signal(0);

            for (uint64_t i = 0; i < N_; i++) {
              auto child_idx = idx_ * N_ + i + 1;
              if (child_idx >= size_) {
                return;
              }

              TypedCap<ISignalable> dest(group_->getMember(child_idx)->cap());
              if (dest) {
                MLOG_DETAIL(mlog::boot, idx_, "signals", child_idx);
                NaryTree nt(group_, child_idx, size_);
                dest->multicast(nt);
              } else {
                PANIC("No Signalable anymore.");
              }
            }
        });
    }


    uint64_t size;
};

class TreeMulticast
{
public:
    static Error multicast(SignalableGroup *group, size_t groupSize) {
        ASSERT(group != nullptr);
        /*
        TypedCap<ISignalable> signalable(group->getMember(0)->cap());

        if (signalable) {
            TreeCastStrategy tcs(group, 0, 0, groupSize - 1); // from and to are included
            signalable->multicast(tcs);
            //NaryTree nt(group, 0, groupSize);
            //signalable->multicast(nt);
        }
        */
        TreeCastStrategy::multicast(group, 0, 0, groupSize-1);
        return Error::SUCCESS;
    }
};



} // namespace mythos
