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

#include "mythos/init.hh"
#include "mythos/invocation.hh"
#include "mythos/protocol/CpuDriverKNC.hh"
#include "mythos/PciMsgQueueMPSC.hh"
#include "runtime/Portal.hh"
#include "runtime/ExecutionContext.hh"
#include "runtime/CapMap.hh"
#include "runtime/Example.hh"
#include "runtime/InterruptControl.hh"
#include "runtime/PageMap.hh"
#include "runtime/KernelMemory.hh"
#include "runtime/SimpleCapAlloc.hh"
#include "runtime/SignalGroup.hh"
#include "runtime/IdleManagement.hh"
#include "app/mlog.hh"
#include <cstdint>
#include "util/optional.hh"
#include "util/Time.hh"
#include "app/Thread.hh"
#include "app/ThreadManager.hh"
#include "app/TreeMulticastBenchmark.hh"
#include "app/HelperMulticastBenchmark.hh"
#include "app/SequentialMulticastBenchmark.hh"
#include "app/TreeCombining.hh"
#include "runtime/HWThread.hh"
#include "app/conf.hh"
#include "util/Logger.hh"


mythos::InvocationBuf* msg_ptr asm("msg_ptr");
int main() asm("main");

constexpr uint64_t stacksize = 8 * 4096;
char initstack[stacksize];
char* initstack_top = initstack + stacksize;

mythos::Portal portal(mythos::init::PORTAL, msg_ptr);
mythos::CapMap cs(mythos::init::CSPACE);
mythos::PageMap as(mythos::init::PML4);
mythos::KernelMemory kmem(mythos::init::KM);
mythos::SimpleCapAllocDel caps(portal, cs, mythos::init::APP_CAP_START,
                               mythos::init::SIZE - mythos::init::APP_CAP_START);

mythos::TreeCombining<NUM_THREADS, 5> tc;
std::atomic<uint64_t> counter {0};
ThreadManager manager(portal, cs, as, kmem, caps);

// memory for value buffering when benchmarking
uint64_t values1[NUM_THREADS];
uint64_t values2[NUM_THREADS];

uint64_t values3[REPETITIONS];
uint64_t values4[REPETITIONS];

int main()
{
    //TreeMulticastBenchmark tmb(portal);
    //tmb.test_multicast();

    //HelperMulticastBenchmark hmb(portal);
    //hmb.test_multicast();

    SequentialMulticastBenchmark smb(portal);
    smb.test_multicast();
    return 0;
}
