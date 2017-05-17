/* -*- mode:C++; indent-tabs-mode:nil; -*- */
/* MyThOS: The Many-Threads Operating System
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

#include "async/NestedMonitorHome.hh"
#include "cpu/CoreLocal.hh"
#include "objects/ISchedulable.hh"
#include "objects/IKernelObject.hh"
#include "objects/IFactory.hh"
#include "mythos/protocol/KernelObject.hh"
#include "mythos/protocol/SchedulingCoordinator.hh"
#include "boot/mlog.hh"
#include "util/ThreadMutex.hh"

namespace mythos {

using mythos::async::Place;
using mythos::SchedulingContext;

class CoreGroup;
class CoordinatorPolicy;
class GroupPolicy;


/**
 * Class coordinates between Place(kernel task scheduler), SchedulingContext(system thread scheduler),
 * ExecutionContext (system thread) and the decision to go to sleep. Different policies of sleeping can be chosen.
 */
class SchedulingCoordinator
    : public IKernelObject {

    enum Policy {
        SLEEP = 0,
        SPIN,
        GROUP,
    };

    friend class CoreGroup;

//IKernelObject interface
public:

    optional<void> deleteCap(Cap self, IDeleter& del) override;
    void invoke(Tasklet* t, Cap self, IInvocation* msg) override;
    optional<void const*> vcast(TypeId id) const override;

//protocol implementations
protected:
    friend struct protocol::SchedulingCoordinator;
    Error printMessage(Tasklet *t, Cap self, IInvocation *msg);
    Error setPolicy(Tasklet *t, Cap self, IInvocation *msg);

    friend struct protocol::KernelObject;
    Error getDebugInfo(Cap self, IInvocation* msg);


// Actual Methods
public:
    NORETURN void runUser() {
        switch (policy) {
            case SLEEP : runSleep(); break;
            case SPIN  : runSpin();  break;
            case GROUP : runGroup(); break;
            default    : runSleep(); break;
        }
    }

    NORETURN void sleep() {
        MLOG_ERROR(mlog::boot, "going to sleep now");
        mythos::cpu::go_sleeping(); // resets the kernel stack!
    }

    void init(mythos::async::Place *p, mythos::SchedulingContext *sc, CoreGroup *core_) {
        //mlogsc.error("init", p , sc);
        ASSERT(p != nullptr);
        ASSERT(sc != nullptr);
        localPlace = p;
        localSchedulingContext = sc;
        core = core_;
        monitor.setHome(p);
    }

    NORETURN void runSleep();
    NORETURN void runSpin();
    NORETURN void runGroup();
private:
    // kernel object stuff
    IDeleter::handle_t del_handle = {this};
    async::NestedMonitorHome monitor;

    // actual stuff
    Place *localPlace = nullptr;
    SchedulingContext *localSchedulingContext = nullptr;

    Policy policy = {GROUP};

    CoreGroup *core {nullptr};

    volatile bool SLEEP_FLAG = false;
};

class CoreGroup {
private:
    static const constexpr int HWTHREADS_PER_CORE = 2;
    SchedulingCoordinator* group[HWTHREADS_PER_CORE] = {nullptr};
    bool intents[HWTHREADS_PER_CORE] = {false};
    ThreadMutex mutex;
public:

    void sleep_intent(int apicID) {

    }

    void sleep_unintent(int apicID) {

    }

    bool full() {
        for (int i = 0; i < HWTHREADS_PER_CORE; i++) {
            if (group[i] == nullptr) {
                return false;
            }
        }
        MLOG_ERROR(mlog::boot, "FULL");
        return true;
    }

    bool contains(SchedulingCoordinator *sc) {
        for (int i = 0; i < HWTHREADS_PER_CORE; i++) {
            if (sc == group[i]) {
                return true;
            }
        }
        return false;
    }

    void group_sleep() {
        for (int i = 0; i < HWTHREADS_PER_CORE; i++) {
            group[i]->SLEEP_FLAG = true;
        }
    }

    void sleep_intent(SchedulingCoordinator *sc) {
        mutex << [&] () {
            if (!contains(sc)) {
                MLOG_ERROR(mlog::boot, "NEW Sleep intent", DVAR(sc));
                for (int i = 0; i < HWTHREADS_PER_CORE; i++) {
                    if (group[i] == nullptr) {
                        group[i] = sc;
                        break;
                    }
                }
                if (full()) {
                    group_sleep();
                }
            }
        };

    }

    void sleep_unintent(SchedulingCoordinator *sc) {
        mutex << [&] () {
            for (int i = 0; i < HWTHREADS_PER_CORE; i++) {
                if (group[i] == sc) {
                    MLOG_ERROR(mlog::boot, "Sleep Unintent", DVAR(sc));
                    group[i] = nullptr;
                }
            }
        };
    }
};

} // namespace mythos
