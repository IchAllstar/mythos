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

#include "async/NestedMonitorDelegating.hh"
#include "objects/IKernelObject.hh"
#include "objects/IFactory.hh"
#include "mythos/protocol/KernelObject.hh"
#include "mythos/protocol/HelperThreadManager.hh"
#include "boot/mlog.hh"

namespace mythos {

class HelperThreadManager
  : public IKernelObject {

//IKernelObject interface
public:
  optional<void> deleteCap(Cap self, IDeleter& del) override;
  void invoke(Tasklet* t, Cap self, IInvocation* msg) override;
  optional<void const*> vcast(TypeId id) const override;
  friend struct protocol::KernelObject;
  Error getDebugInfo(Cap self, IInvocation* msg);
  Error registerHelper(Tasklet*, Cap self, IInvocation* msg);
public: // CapRef interface
  void bind(optional<IScheduler*>);
  void unbind(optional<IScheduler*>);

private:
  // kernel object stuff
  IDeleter::handle_t del_handle = {this};
  async::NestedMonitorDelegating monitor;

  // possible helpers
  bool member[MYTHOS_MAX_THREADS];
  uint64_t actualSize {0};
};

} // namespace mythos
