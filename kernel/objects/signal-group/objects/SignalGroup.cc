#include "objects/SignalGroup.hh"
#include "objects/mlog.hh"
#include "objects/TreeMulticast.hh"
#include "objects/SequentialMulticast.hh"
#include "objects/HelperMulticast.hh"

namespace mythos {

SignalGroup::SignalGroup(IAsyncFree* mem, CapRef<SignalGroup, ISignalable> *arr, Tasklet *tasklets_, size_t groupSize_)
    : _mem(mem), member(arr), tasklets(tasklets_), groupSize(groupSize_)
{
    MLOG_DETAIL(mlog::boot, "Create Group with size", groupSize);

    // initialize arrays in place
    CapRef<SignalGroup, ISignalable>* obj IGNORE_UNUSED;
    for (uint64_t i = 0; i < groupSize; i++) {
        obj = new (&member[i]) CapRef<SignalGroup, ISignalable>();
    }

    Tasklet* tasklet IGNORE_UNUSED;
    for (uint64_t i = 0; i < groupSize; i++) {
        tasklet = new (&tasklets[i]) Tasklet();
    }
}

optional<void> SignalGroup::deleteCap(Cap self, IDeleter& del) {
    if (self.isOriginal()) {
        del.deleteObject(del_handle);
    }
    RETURN(Error::SUCCESS);
}

void SignalGroup::deleteObject(Tasklet* t, IResult<void>* r)  {
    monitor.doDelete(t, [ = ](Tasklet * t) {
        for (uint64_t i = 0; i < actualSize; i++) {
            member[i].reset();
        }
        auto size = sizeof(CapRef<SignalGroup, ISignalable>) * groupSize;
        size += sizeof(Tasklet) * groupSize;
        size += sizeof(SignalGroup);
        _mem->free(t, r, this, size);
    });
}

optional<void const*> SignalGroup::vcast(TypeId id) const {
    if (id == typeId<IKernelObject>()) return static_cast<IKernelObject const*>(this);
    THROW(Error::TYPE_MISMATCH);
}

void SignalGroup::invoke(Tasklet* t, Cap self, IInvocation* msg)
{
    monitor.request(t, [ = ](Tasklet * t) {
        Error err = Error::NOT_IMPLEMENTED;
        switch (msg->getProtocol()) {
            case protocol::KernelObject::proto:
                err = protocol::KernelObject::dispatchRequest(this, msg->getMethod(), self, msg);
                break;
            case protocol::SignalGroup::proto:
                err = protocol::SignalGroup::dispatchRequest(this, msg->getMethod(), t, self, msg);
                break;
        }
        if (err != Error::INHIBIT) {
            msg->replyResponse(err);
            monitor.requestDone();
        }
    } );
}

void SignalGroup::bind(optional<ISignalable*>) {
    MLOG_DETAIL(mlog::boot, "bind");
}

void SignalGroup::unbind(optional<ISignalable*>) {
    MLOG_DETAIL(mlog::boot, "unbind");
}

Error SignalGroup::getDebugInfo(Cap self, IInvocation* msg)
{
    return writeDebugInfo("SignalGroup", self, msg);
}

optional<SignalGroup*>
SignalGroupFactory::factory(CapEntry* dstEntry, CapEntry* memEntry, Cap memCap,
                            IAllocator* mem, message_type* data)
{
    static Alignment<64> align;
    auto size = align.round_up(sizeof(CapRef<SignalGroup, ISignalable>) * data->groupSize);
    size += align.round_up(sizeof(Tasklet) * data->groupSize);
    size += align.round_up(sizeof(SignalGroup));
    auto ptr = mem->alloc(size, 64);
    if (not ptr) RETHROW(ptr);
    memset(*ptr, 0, size);
    uint64_t point = (uint64_t) (*ptr);
    auto *group = (CapRef<SignalGroup, ISignalable>*)(point + align.round_up(sizeof(SignalGroup)));
    auto *tasklets = (Tasklet*)(point + align.round_up(sizeof(SignalGroup)) + align.round_up(sizeof(CapRef<SignalGroup, ISignalable>) * data->groupSize));
    auto obj = new(*ptr) SignalGroup(mem, group, tasklets, data->groupSize);
    auto cap = Cap(obj);
    auto res = cap::inherit(*memEntry, *dstEntry, memCap, cap);
    if (not res) {
        mem->free(*ptr, size);
        RETHROW(res);
    }
    return obj;
}

// recursive memorize
int64_t TreeCastStrategy::tmp[TreeCastStrategy::RECURSIVE_SIZE] = {0};

Error SignalGroup::signalAll(Tasklet *t, Cap self, IInvocation *msg) {
    MLOG_DETAIL(mlog::boot, "signalAll()", DVAR(t), DVAR(self), DVAR(msg), DVAR(actualSize));
    ASSERT(member != nullptr);
    switch (strategy) {
        case SEQUENTIAL:
            return SequentialMulticast::multicast(this, actualSize);
        case TREE:
            return TreeMulticast::multicast(this, actualSize);
        case HELPER:
            return HelperMulticast::multicast(this, actualSize);
        default:
            return SequentialMulticast::multicast(this, actualSize);
    }
}

Error SignalGroup::setCastStrategy(Tasklet*, Cap, IInvocation *msg) {
    auto data = msg->getMessage()->read<protocol::SignalGroup::SetCastStrategy>();
    strategy = data.strategy;
    return Error::SUCCESS;
}

Error SignalGroup::addMember(Tasklet *t, Cap self, IInvocation *msg) {

    auto data = msg->getMessage()->read<protocol::SignalGroup::AddMember>();
    MLOG_DETAIL(mlog::boot, "addMember()", DVAR(t), DVAR(self), DVAR(msg));

    auto capEntry = msg->lookupEntry(data.signalable());
    TypedCap<ISignalable> obj(capEntry);
    if (!obj) return Error::INVALID_CAPABILITY;

    // look if it is in the group
    for (uint64_t i = 0; i < groupSize; i++) {
        if (member[i].cap().getPtr() == obj.cap().getPtr()) {
            return Error::INVALID_ARGUMENT;
        }
    }
    for (uint64_t i = 0; i < groupSize; i++) {
        if (!member[i].isUsable()) {
            member[i].set(this, *capEntry, obj.cap());
            actualSize++;
            return Error::SUCCESS;
        }
    }
    return Error::INSUFFICIENT_RESOURCES;
}

Error SignalGroup::addHelper(Tasklet*, Cap, IInvocation *msg) {
    auto data = msg->getMessage()->read<protocol::SignalGroup::AddHelper>();
    auto capEntry = msg->lookupEntry(data.cpuThread());
    TypedCap<HWThread> obj(capEntry);
    if (!obj) return Error::INVALID_CAPABILITY;
    helper[obj->getApicID()] = true;
    actualHelper++;
    MLOG_DETAIL(mlog::boot, "addHelper()", DVAR(obj->getApicID()));
    return Error::SUCCESS;
}

HWThread* SignalGroup::getHelper(uint64_t i) {
    uint64_t tmp = 0;
    for (uint64_t j = 0; j < MYTHOS_MAX_THREADS; j++) {
        if (helper[j] == true) tmp++;
        if (tmp == i + 1) {
            return &mythos::boot::getHWThread(j);
        }
    }
    PANIC("Not enough helper threads.");
    return nullptr;
}

} // namespace mythos
