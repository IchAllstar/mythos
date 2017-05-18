#include "objects/CoreGroup.hh"

namespace mythos {
void CoreGroup::wakeup() {
    MLOG_ERROR(mlog::boot, "WAKEUP");
    for (uint64_t i = 0; i < HWTHREADS_PER_CORE; i++) {
        if (group[i]->SLEEP_FLAG) {
            group[i]->SLEEP_FLAG = false;
            group[i]->localPlace->wakeup();
            sleep_unintent(group[i]);
        } else {
            // own thread gets reported hier because it turns off his flag
            MLOG_ERROR(mlog::boot, "WAS AWAKE", i);
        }
    }
}

bool CoreGroup::full() {
    for (uint64_t i = 0; i < HWTHREADS_PER_CORE; i++) {
        if (group[i] == nullptr) {
            return false;
        }
    }
    MLOG_ERROR(mlog::boot, "FULL");
    return true;
}

bool CoreGroup::contains(SchedulingCoordinator *sc) {
    for (uint64_t i = 0; i < HWTHREADS_PER_CORE; i++) {
        if (sc == group[i]) {
            return true;
        }
    }
    return false;
}

void CoreGroup::group_sleep() {
    for (uint64_t i = 0; i < HWTHREADS_PER_CORE; i++) {
        group[i]->SLEEP_FLAG = true;
    }
}

void CoreGroup::sleep_intent(SchedulingCoordinator *sc) {
    mutex << [&] () {
        if (!contains(sc)) {
            MLOG_ERROR(mlog::boot, "NEW Sleep intent", DVAR(sc));
            for (uint64_t i = 0; i < HWTHREADS_PER_CORE; i++) {
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

void CoreGroup::sleep_unintent(SchedulingCoordinator *sc) {
    mutex << [&] () {
        for (uint64_t i = 0; i < HWTHREADS_PER_CORE; i++) {
            if (group[i] == sc) {
                MLOG_ERROR(mlog::boot, "Sleep Unintent", DVAR(sc));
                group[i] = nullptr;
            }
        }
    };
}

} // namespace mythos