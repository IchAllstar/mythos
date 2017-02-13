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

#include "objects/Portal.hh"

namespace mlog {
  Logger<MLOG_PORTAL> portal("portal");
} // namespace mlog

namespace mythos {

  Portal::Portal(IAsyncFree* memory)
    : ib(nullptr), ibNew(nullptr), uctx(0), portalState(OPEN), memory(memory)
  {
  }

  optional<void> Portal::setInvocationBuf(optional<CapEntry*> fe, uint32_t offset)
  {
    mlog::portal.info("Portal::setInvocationBuf", DVAR(fe), DVAR(offset));
    TypedCap<IFrame> frame(fe);
    if (!frame) return frame;
    auto info = frame.getFrameInfo();
    if (!info.start.kernelmem() || !info.writable) return Error::INVALID_CAPABILITY;
    if (offset+sizeof(InvocationBuf) >= info.size) return Error::INSUFFICIENT_RESOURCES;
    ibNew = reinterpret_cast<InvocationBuf*>(info.start.logint()+offset);
    return _ib.set(this, *fe, frame.cap());
  }

  void Portal::bind(optional<IFrame*> obj)
  {
    mlog::portal.info("Portal::setInvocationBuf bind");
    if (obj) ib = ibNew;
  }

  void Portal::unbind(optional<IFrame*>)
  {
    mlog::portal.info("Portal::setInvocationBuf unbind");
    ib = nullptr; // but do not overwrite ibNew before the bind() method!
  }

  optional<void> Portal::setOwner(optional<CapEntry*> ece, uintptr_t uctx)
  {
    mlog::portal.info("Portal::setOwner", DVAR(ece), DVARhex(uctx));
    TypedCap<IPortalUser> ec(ece);
    if (!ec) return ec;
    return _owner.set(this, *ece, ec.cap());
  }

  void Portal::unbind(optional<IPortalUser*> obj)
  {
    mlog::portal.info("Portal::setOwner unbind");
    if (obj) obj->denotify(&notificationHandle);
  }

  optional<CapEntryRef> Portal::lookupRef(CapPtr ptr, CapPtrDepth ptrDepth, bool writable)
  {
    auto owner = _owner.get();
    if (!owner) return owner.state();
    return owner->lookupRef(ptr, ptrDepth, writable);
  }

  optional<CapEntry*> Portal::lookupEntry(CapPtr ptr, CapPtrDepth ptrDepth, bool writable)
  {
    auto ref = this->lookupRef(ptr, ptrDepth, writable);
    if (!ref) return ref.state();
    return ref->entry;
  }

  optional<void> Portal::sendInvocation(Cap self, CapPtr dest, uint64_t user)
  {
    mlog::portal.info("Portal::sendInvocation", DVAR(self), DVAR(dest));
    TypedCap<IPortalUser> owner(_owner.cap());
    if (!owner) return owner.state();
    auto dref = owner->lookupRef(dest, 32, false);
    if (!dref) return dref.state();
    currentDest = *dref;
    destCap = currentDest.entry->cap();
    if (!destCap.isUsable()) return Error::NO_LOOKUP;

    uint8_t expected = OPEN;
    if (!portalState.compare_exchange_strong(expected, INVOKING)) return Error::PORTAL_NOT_OPEN;

    currentDest.acquire();
    this->uctx = user;
    destCap.getPtr()->invoke(&mytask, destCap, this);
    return Error::SUCCESS;
  }

  void Portal::replyResponse(optional<void> error)
  {
    mlog::portal.info("Portal::replyResponse", DVAR(error.state()));
    ASSERT(portalState == INVOKING);
    currentDest.release();
    portalState = REPLYING;
    auto owner = _owner.get();
    if (owner) {
      replyError = error.state();
      owner->notify(&notificationHandle);
    }
  }

  void Portal::deletionResponse(CapEntry* entry, bool delRoot)
  {
    mlog::portal.info("Portal::deletionResponse", DVAR(delRoot));
    ASSERT(portalState == INVOKING);
    currentDest.release();
    if (delRoot) {
      revokeOp.deleteCap(&mytask, this, *entry, this);
    } else {
      revokeOp.revokeCap(&mytask, this, *entry, this);
    }
  }

  void Portal::response(Tasklet*, optional<void> res)
  {
    mlog::portal.info("Portal::deletion response:", res.state());
    ASSERT(portalState == INVOKING);
    portalState = REPLYING;
    auto owner = _owner.get();
    if (owner) {
      replyError = res.state();
      owner->notify(&notificationHandle);
    }
  }

  optional<void> Portal::deleteCap(Cap self, IDeleter& del)
  {
    mlog::portal.detail("delete cap", self);
    if (self.isOriginal()) {
      if (!revokeOp.acquire()) {
        // we are currently deleting, so abort in order to prevent a deadlock
        // this should be resolvable (if no livelocking) when restarted from user level
        return Error::RETRY;
      } // otherwise, no new deletes/revokes can be started now

      _ib.reset();
      _owner.reset();
      del.deleteObject(del_handle);
    }
    return Error::SUCCESS;
  }

  void Portal::invoke(Tasklet* t, Cap self, IInvocation* msg)
  {
    mlog::portal.info("Portal::invoke", DVAR(t), DVAR(self), DVAR(msg));
    monitor.request(t, [=](Tasklet* t){
        Error err = Error::NOT_IMPLEMENTED;
        switch (msg->getProtocol()) {
        // case protocol::KernelObject::proto:
        //   err = protocol::KernelObject::dispatchRequest(this, msg->getMethod(), t, self, msg);
        //   break;
        case protocol::Portal::proto:
          err = protocol::Portal::dispatchRequest(this, msg->getMethod(), t, self, msg);
          break;
        }
        if (err != Error::INHIBIT) {
          msg->replyResponse(err);
          monitor.requestDone();
        }
      } );
  }

  Error Portal::invokeBind(Tasklet*, Cap, IInvocation* msg)
  {
    auto data = msg->getMessage()->read<protocol::Portal::Bind>();

    if (data.ib() == delete_cap) { unsetInvocationBuf(); }
    else if (data.ib() != null_cap) {
      auto err = setInvocationBuf(msg->lookupEntry(data.ib()), data.offset);
      if (!err) return err.state();
    }

    if (data.owner() == delete_cap) { unsetOwner(); }
    else if (data.owner() != null_cap) {
      auto err = setOwner(msg->lookupEntry(data.owner()), data.uctx);
      if (!err) return err.state();
    }
    return Error::SUCCESS;
  }
  
  optional<Portal*>
  PortalFactory::factory(CapEntry* dstEntry, CapEntry* memEntry, Cap memCap, IAllocator* mem)
  {
    auto obj = mem->create<Portal>();
    if (!obj) return obj.state();
    Cap cap(*obj); /// @todo should have Portal specific rights
    auto res = cap::inherit(*memEntry, *dstEntry, memCap, cap);
    if (!res) {
      mem->free(*obj); // mem->release(obj) goes throug IKernelObject deletion mechanism
      return res.state();
    }
    return *obj;
  }
} // namespace mythos
