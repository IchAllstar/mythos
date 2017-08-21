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

struct TreeCastStrategy {
    // Latency: inverse ratio of sending overhead and complete transfer time
    // LATENCY = 2 optimal on KNC if no deep sleep
    // higher latency better if threads are in deep sleep
    // could check everything for deep sleep and choose latency dynamically?
    // could skip deep sleep nodes and signal children from own context ->ok
    static const uint64_t LATENCY = 3;
    static int64_t tmp[20]; //recursive memory

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

    static void signalTo(SignalableGroup *group, uint64_t idx, uint64_t from, uint64_t to) {
            auto to_tmp = to;
            // Signal own EC, should be ready when leaving kernel
            TypedCap<ISignalable> own(group->getMember(idx)->cap());
            while (true) {
                uint64_t n = to_tmp - from + 1;
                if ( n < 2) {
                    break;
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
            own->signal(0);
    }

    static void multicast(SignalableGroup *group, uint64_t idx, uint64_t from, uint64_t to) {
        auto *t = group->getTasklet(idx);
        TypedCap<ISignalable> own(group->getMember(idx)->cap());
        if (!own) PANIC("signalable not valid anymore");
        auto sleepState = own->getSleepState();
        if (sleepState < 2) { // child not in deep sleep send Tasklet
            t->set([group, idx, from, to](Tasklet*) {
              signalTo(group, idx, from, to);
            });
            auto sched = own->getScheduler();
            if (sched) {
                sched->run(t);
            }
        } else { // Send to child yourself because is in deep sleep
            signalTo(group, idx, from, to);
        }
    }

};


/**
 * N-Ary tree for comparison with the Fibonacci Tree approach
 */
struct NaryTree {
    static const uint64_t N = 4;

    static void sendTo(SignalableGroup *group, uint64_t idx, uint64_t size) {
          MLOG_DETAIL(mlog::boot, DVAR(group), DVAR(idx), DVAR(size));
          ASSERT(idx < size);
          ASSERT(group != nullptr); // TODO: parallel deletion of group?
          TypedCap<ISignalable> own(group->getMember(idx)->cap());
          ASSERT(own);

          for (uint64_t i = 0; i < N; i++) {
            auto child_idx = idx * N + i + 1;
            if (child_idx >= size) {
              break;
            }
            //MLOG_ERROR(mlog::boot, idx, "signals", child_idx);
            NaryTree::multicast(group, child_idx, size);
          }
          // Signal own EC, will be scheduled after kernel task handling
          own->signal(0);

    }

    static void multicast(SignalableGroup *group, uint64_t idx, uint64_t size) {
      auto *t = group->getTasklet(idx);
      TypedCap<ISignalable> own(group->getMember(idx)->cap());
      if (not own) PANIC("No own");
      auto sleepState = own->getSleepState();
      if (sleepState < 2) {
        //MLOG_ERROR(mlog::boot, DVAR(group), DVAR(idx), DVAR(size), DVAR(sleepState));
        t->set([group, idx, size](Tasklet*) {
            NaryTree::sendTo(group, idx, size);
        });
        auto sched = own->getScheduler();
        if (sched) {
          sched->run(t);
        }
      } else {
          NaryTree::sendTo(group, idx, size);
      }
    }
};

class TreeMulticast
{
public:
    static Error multicast(SignalableGroup *group, size_t groupSize) {
        ASSERT(group != nullptr);
        //TreeCastStrategy::multicast(group, 0, 0, groupSize-1);
        NaryTree::multicast(group, 0, groupSize);
        return Error::SUCCESS;
    }
};



} // namespace mythos
