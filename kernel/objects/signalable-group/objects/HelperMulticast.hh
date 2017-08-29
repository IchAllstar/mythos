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
#include "boot/DeployHWThread.hh"
#include "async/Place.hh"


namespace mythos {


class HelperMulticast
{
public:

	static Error multicast(SignalableGroup *group, size_t groupSize) {
		ASSERT(group != nullptr);
		uint64_t threads = groupSize/* - 1*/;
		//uint64_t availableHelper = mythos::boot::helperThreadManager.numHelper();
    uint64_t availableHelper = group->numHelper();
		if (availableHelper == 0) {
			return Error::INSUFFICIENT_RESOURCES;
		}
		uint64_t optimalHelper = num_helper(threads);
		uint64_t usedHelper = (availableHelper < optimalHelper) ? availableHelper : optimalHelper;
		uint64_t optimalThreadNumber = (usedHelper * (usedHelper + 1)) / 2;
		int64_t  diffThreads = threads - optimalThreadNumber;
		uint64_t current = 0;
		uint64_t diff = diffThreads / usedHelper;
		uint64_t mod = diffThreads % usedHelper; // never bigger than usedHelper

		for (uint64_t i = 0; i < usedHelper; i++) {
			//auto *sched = mythos::boot::helperThreadManager.getHelper(i);
      auto *sched = group->getHelper(i);
			uint64_t base = usedHelper - i;
			if (diffThreads > 0) {
				uint64_t tmp = diff;
				if (mod > 0) {
					tmp++;
					mod--;
				}
				base += tmp;
				diffThreads -= tmp;
			}

			auto *tasklet = group->getTasklet(i); // use a tasklet from SignalingGroup, should not be in use
			//MLOG_ERROR(mlog::boot, "send to Helper Thread:", DVAR(current), DVAR(current + base-1));
			tasklet->set([group, i, current, base](Tasklet*) {
				ASSERT(group != nullptr);
        for (uint64_t i = current; i < current + base; i++) {
					TypedCap<ISignalable> dest(group->getMember(i)->cap());
					if (dest) {
						dest->signal(0);
					} else {
						PANIC("Could not reach signalable.");
					}
				}
			});

			sched->getHome()->run(tasklet);
			current += base;
		}

		return Error::SUCCESS;
	}

private:
	// Calculates optimal number of helper threads for given number of threads
	static uint64_t num_helper(uint64_t groupSize) {
		uint64_t value {0};
		uint64_t i {0};
		while (value <= groupSize) {
			i++;
			value = (i * (i + 1)) / 2;
		}
		return (i - 1 > 0) ? i - 1 : 0;
	}
};

} // namespace mythos

