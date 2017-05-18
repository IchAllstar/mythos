#pragma once

#include "objects/SchedulingCoordinator.hh"
#include "util/ThreadMutex.hh"

namespace mythos {
using mythos::async::Place;
using mythos::SchedulingContext;

/**
 * Represents a processor core with its number of hardware threads. Because of power 
 * management related reasons, it is useful togo to sleep as a core and wakeup as a core.
 * This class coordinates the sleep and wakeup intentions between the different hardware
 * threads, which are represented by the SchedulingCoordinator.
 *
 */
class CoreGroup {
public:
    /** Wakes up every SchedulingCoordinator in this group,
      * that has its SLEEP_FLAG set 
      */
    void wakeup();

    /** Returns true if all group members currently intending to sleep */
    bool full();

    /** Returns true, if the given SchedulingCoordinator is currently intending to sleep */
    bool contains(SchedulingCoordinator *sc);

    /** Sets SLEEP_FLAG of all group member to true, leading to sleep on next kernel main loop*/
    void group_sleep();

    /** registers a sleep intent for the given SchedulingCoordinator
     *  Locked by ThreadMutex.
     */
    void sleep_intent(SchedulingCoordinator *sc);

    /** unregisters a sleep intent for the given SchedulingCoordinator.
     *  Locked by ThreadMutex.
     */
    void sleep_unintent(SchedulingCoordinator *sc);

private:
    static const constexpr int HWTHREADS_PER_CORE = 2;
    SchedulingCoordinator* group[HWTHREADS_PER_CORE] = {nullptr};
    bool intents[HWTHREADS_PER_CORE] = {false};
    ThreadMutex mutex;
};

} // namespace mythos