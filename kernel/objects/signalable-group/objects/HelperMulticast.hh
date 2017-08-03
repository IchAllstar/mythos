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
 * Helper Strategy, which tells the target helper thread which application threads it has to signal.
 */
struct HelperCastStrategy : public CastStrategy {
	size_t from;
	size_t to;

	HelperCastStrategy(SignalableGroup *group_, size_t idx_, size_t from_, size_t to_)
		: CastStrategy(group_, idx_), from(from_), to(to_)
	{}


	void create(Tasklet &t) const override {
		// Need to copy variables for capturing in lambda
		auto group_ = group;
		size_t from_, to_;
		from_ = from;
		to_ = to;
		// Create Tasklet which will be send to destination hardware thread
		t.set([group_, from_, to_](Tasklet*) {
			MLOG_ERROR(mlog::boot, "In Helper Thread:", DVAR(from_), DVAR(to_));
			ASSERT(group_ != nullptr);
			ASSERT(to_ > 0);

			for (uint64_t i = from_; i <= to_; i++) {
				MLOG_ERROR(mlog::boot, "Wakeup", DVAR(i));
				TypedCap<ISignalable> dest(group_->getMember(i)->cap());
				if (dest) {
					dest->signal(0);
				}
			}
		});
	}

};



std::array<uint64_t, 5> helper = {1, 2, 3, 4, 5};

class SignalableGroup;
class HelperMulticast
{
public:
	static Error multicast(SignalableGroup *group, size_t groupSize) {
		ASSERT(group != nullptr);
		uint64_t threads = groupSize - 1;
		uint64_t availableHelper = sizeof(helper) / sizeof(uint64_t);
		uint64_t optimalHelper = num_helper(threads);
		uint64_t usedHelper = (availableHelper < optimalHelper) ? availableHelper : optimalHelper;
		uint64_t optimalThreadNumber = (usedHelper * (usedHelper + 1)) / 2;
		int64_t  diffThreads = threads - optimalThreadNumber;
		uint64_t current = 0;
		uint64_t diff = diffThreads / usedHelper;
		uint64_t mod = diffThreads % usedHelper; // never bigger than usedHelper

		HelperCastStrategy hcs(group, 0, 0, groupSize - 1);
		for (uint64_t i = 0; i < usedHelper; i++) {
			TypedCap<ISignalable> signalable(group->getMember(helper[i])->cap());
			if (!signalable) {
				PANIC("Signalable not valid anymore.");
			}
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
			hcs.from = current;
			hcs.to = current + base;
			hcs.idx = i;
			MLOG_ERROR(mlog::boot, "helper ", i, " from:", current, " to: ", current + base);
			signalable->multicast(hcs);
			current += base;
		}


		return Error::SUCCESS;
	}

private:
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

