#include "cpu/idle.hh"
#include "boot/DeployHWThread.hh"

namespace mythos {
namespace idle {


void init_global() {
}

void init_thread() {
    boot::getLocalIdleManagement().init_thread();
}

void sleep(uint8_t depth)
{
	boot::getLocalIdleManagement().sleepIntention(depth);
    cpu_idle_halt();
}

void wokeup(size_t apicID, size_t reason)
{
    boot::getLocalIdleManagement().wokeup(apicID, reason);
}

void wokeupFromInterrupt(uint8_t irq)
{
    boot::getLocalIdleManagement().wokeupFromInterrupt(irq);
}

void enteredFromSyscall() {
    boot::getLocalIdleManagement().enteredFromSyscall();
}

void enteredFromInterrupt(uint8_t irq) {
    boot::getLocalIdleManagement().enteredFromInterrupt(irq);
}

} // namespace idle
} // namespace mythos
