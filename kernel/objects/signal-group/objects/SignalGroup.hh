#pragma once

#include "objects/IFactory.hh"
#include "objects/CapRef.hh"
#include "objects/ISignalable.hh"
#include "mythos/protocol/KernelObject.hh"
#include "mythos/protocol/SignalGroup.hh"


namespace mythos {


/**
 * A Group of ISignalable objects with a maximum group size. When signalAll() is invoked, a signal is somehow
 * send to all group members. The group is constructed with a maximum group size and members can just be added,
 * not deleted. Max Groupsize Tasklets are allocated and serve as function objects to send to the destination
 * hardware threads. They can be used to implemented broadcasts, where nodes are used to forward the cast.
 * Always use the tasklet of the receiver node, because I need to send to multiple children, so cannot use my own
 * tasklet.
 * TODO what to do, if SignalGroup is destroyed while cast is ongoing
 */
class SignalGroup
    : public IKernelObject
{
public: // Constructor
    SignalGroup(IAsyncFree* mem, CapRef<SignalGroup, ISignalable> *arr, Tasklet *tasklets_, size_t groupSize_);

public:
    enum CastStrategy {
      SEQUENTIAL = 0,
      TREE,
      HELPER,

      SIZE,
    };
public: // IKernelObject interface
    optional<void const*> vcast(TypeId id) const override;
    optional<void> deleteCap(Cap self, IDeleter& del) override;
    void deleteObject(Tasklet* t, IResult<void>* r) override;
    void invoke(Tasklet* t, Cap, IInvocation* msg) override;
    Error invokeBind(Tasklet* t, Cap, IInvocation* msg);

public: // protocol
    friend struct protocol::KernelObject;
    Error getDebugInfo(Cap self, IInvocation* msg);
    Error signalAll(Tasklet *t, Cap self, IInvocation *msg);
    Error addMember(Tasklet *t, Cap self, IInvocation *msg);
    Error setCastStrategy(Tasklet *t, Cap self, IInvocation *msg);
    Error addHelper(Tasklet *t, Cap self, IInvocation *msg);
public:
    void bind(optional<ISignalable*>);
    void unbind(optional<ISignalable*>);
public:
    CapRef<SignalGroup, ISignalable>* getMembers() { return member; }
    Tasklet* getTasklets() { return tasklets; }
    size_t getSize() { return actualSize; }
    Tasklet* getTasklet(size_t idx) { ASSERT(idx < actualSize); return &tasklets[idx]; }
    CapRef<SignalGroup, ISignalable>* getMember(size_t idx) { ASSERT(idx < actualSize); return &member[idx]; }
    SchedulingCoordinator* getHelper(uint64_t i);
    uint64_t numHelper() { return actualHelper; }
private:
    IAsyncFree* _mem;
    /** list handle for the deletion procedure */
    LinkedList<IKernelObject*>::Queueable del_handle = {this};
    async::NestedMonitorDelegating monitor;

    // allocate when max group size is known
    CapRef<SignalGroup, ISignalable> *member {nullptr};
    Tasklet *tasklets;
    size_t groupSize {0};
    size_t actualSize {0};

    // helper support, one entry for every hwthread
    bool helper[MYTHOS_MAX_THREADS] {false};
    uint64_t actualHelper {0};

    uint64_t strategy {SEQUENTIAL};
};

class SignalGroupFactory : public FactoryBase
{
public:
    typedef protocol::SignalGroup::Create message_type;

    static optional<SignalGroup*>
    factory(CapEntry* dstEntry, CapEntry* memEntry, Cap memCap, IAllocator* mem, message_type* data = nullptr);

    Error factory(CapEntry* dstEntry, CapEntry* memEntry, Cap memCap,
                  IAllocator* mem, IInvocation* msg) const override {
        auto data = msg->getMessage()->read<message_type>();
        return factory(dstEntry, memEntry, memCap, mem, &data).state();
    }
};

} // namespace mythos
