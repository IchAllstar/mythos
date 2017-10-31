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


namespace mythos {

class SignalGroup;

class SequentialMulticast
{
public:
    static void sendTo(SignalGroup *group, uint64_t idx, uint64_t size) {
        MLOG_DETAIL(mlog::boot, DVAR(group), DVAR(idx), DVAR(size));
        ASSERT(idx < size);
        ASSERT(group != nullptr); // TODO: parallel deletion of group?
        TypedCap<ISignalable> own(group->getMember(idx)->cap());
        ASSERT(own);
        auto N = 240ul;
        for (uint64_t i = 0; i < N; i++) {
            //t.start();
            auto child_idx = idx * N + i + 1;
            if (child_idx >= size) {
                break;
            }
            multicast(group, child_idx, size);
        }
        // Signal own EC, will be scheduled after kernel task handling
        own->signal(0);
    }

    static void multicast(SignalGroup *group, uint64_t idx, uint64_t size) {
      TypedCap<ISignalable> own(group->getMember(idx)->cap()); // 500 - 1000 cycles
      if (own /* && idx <= (size-2)/N && getSleepState(own->getHWThread()) < 2*/) { // node skipping on / off
          auto *t = group->getTasklet(idx); // 50 cycles
          auto *home = own->getHWThread()->getHome(); //800 - 1600 cycles and up to 2000
          t->set([group, idx, size](TransparentTasklet*) {
              sendTo(group, idx, size);
          });
          home->run(t);
      } else {
          sendTo(group, idx, size);
      }
    }

    static void new_cast(SignalGroup *group, uint64_t size) {
        for (uint64_t i = 0; i < size; i++) {
            auto *tasklet = group->getTasklet(i);
            TypedCap<ISignalable> signalable(group->getMember(i)->cap());
            tasklet->set([group,i, size](TransparentTasklet*) {
              TypedCap<ISignalable> own(group->getMember(i)->cap());
              if (own) {
                own->signal(0);
              } else {
                  PANIC("Signalable not valid anymore");
              }
            });
            auto *home = signalable->getHWThread()->getHome(); //800 - 1600 cycles and up to 2000
            home->run(tasklet);
        }

    }
    static Error multicast(SignalGroup *group, size_t groupSize) {
        ASSERT(group != nullptr);

        SequentialMulticast::new_cast(group, groupSize);
        return Error::SUCCESS;
    }
};

} // namespace mythos
