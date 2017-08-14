#pragma once
#include "boot/mlog.hh"
#include "util/optional.hh"

namespace mythos {

// Defines a virtual Core State
struct VirtualIdleState {
  uint64_t id; // corresponds to the depth given to the sleep call; CC6 = 6 and so on
  const char* name;
  uint64_t exit_latency;

  VirtualIdleState(uint64_t id_, const char* name_, uint64_t exit_latency_)
    :id(id_), name(name_), exit_latency(exit_latency_) {}
};

/**
* Straight outa Intel MPSS Linux Kernel: Latencies for KNC
* Should be in u sec, so multiply *1000 for KNC cycles
static struct mic_cpuidle_states mic_cpuidle_data[MAX_KNC_CSTATES] = {
  {     //data for Core C1
    .latency = 20,
    .power = 0,
    .desc = "CC1 :Core C1 idle state"},
  {     // data for Core C6 state
    .latency = 4000,
    .power = 0,
    .desc = "CC6: Core C6 idle state"},
  {     //data for Package C6
    .latency = 800000,
    .power = 0,
    .desc = "PC3: Package C3 idle state"},
};
*/

// Define available States
VirtualIdleState available[3] =
{
  {
    /*.id =*/			 0,
    /*.name = */		 "C0",
    /*.exit_latency = */ 0,
  },
  {
    /*.id =*/			 1,
    /*.name = */		 "C1",
    /*.exit_latency = */ 0,	// hlt state available in real hardware
  },
  {
    /*.id =*/			 6,
    /*.name = */		 "C6",
    /*.exit_latency = */ 4000000,
  }
};

/**
 * Sleep Emulation supports different sleep states defined in available[].
 */
class SleepEmulator {
  public:
    void sleep(uint64_t threadID, uint64_t depth);
    void wakeup(uint64_t threadID);

  public:
    // HW threads per core, maybe dynamically detectable? cpuid?
    static const uint64_t HWTHREADS = 4;
  private:
    static inline uint64_t getCore(uint64_t threadID) { return threadID / HWTHREADS; }
    static inline uint64_t getHWThreadOnCore(uint64_t threadID) { return threadID % HWTHREADS; }

    void delay(uint64_t depth);
    uint64_t setState(uint64_t threadID, uint64_t depth);
    uint64_t getState(uint64_t threadID);
    bool validState(uint64_t depth);
    uint64_t minState(uint64_t core);
    optional<VirtualIdleState*> getStateWithID(uint64_t id);
};

/**
 * Virtual C-State tracked for every hardware thread.
 */
struct CoreState {
  std::atomic<bool> locked = {false};
  /**
   * If two hw threads on a core wake up simultaneously, just one would get the wakeup delay.
   * The second will poll this flag, which simulates the wakeup delay of the whole core.
   */
  std::atomic<bool> sleep = {false};
  volatile uint64_t cstate[SleepEmulator::HWTHREADS] {0}; // lock if access

  void lock() { while(locked.exchange(true) == true); }
  void unlock() { locked.store(false); }
};

} // namespace mythos
