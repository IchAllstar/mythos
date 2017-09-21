#pragma once

#include <array>
#include "app/mlog.hh"

namespace mythos {

struct ALIGNED(cpu::CACHELINESIZE) combine_node {
    std::atomic<uint64_t> value {{0}};
    combine_node *parent {nullptr};


    char padding[cpu::CACHELINESIZE - sizeof(value) - sizeof(parent)];

    void dec() {
        if (--value == 0) {
            if (parent != nullptr) {
                //MLOG_ERROR(mlog::app, "Dec Parent");
                parent->dec();
            } else {
              //MLOG_ERROR(mlog::app, "Finished");
            }
        }
    }
};


/**
 * The Tree Combining should eliminate the bottleneck of a global variable beeing accessed
 */
template<size_t MAX_LEAFS, size_t FANOUT>
class TreeCombining {
    static_assert(MAX_LEAFS > 0 && FANOUT > 0, "MAX_LEAFS and FANOUT must be greater 0");
public:
    TreeCombining();
    void dec(uint64_t id);
    void init(uint64_t maxThreads);

    bool isFinished() {
        return nodes[0].value.load() == 0;
    }

private:
    void initNode(uint64_t idx);
    uint64_t leafs(uint64_t nodes) {
        if (nodes == 1) return 1;
        return nodes - parent(nodes-1) - 1;
    }

    uint64_t parent(uint64_t idx) {
        return (idx-1) / FANOUT;
    }

    bool hasChildren(uint64_t idx) {
        auto child = idx * FANOUT + 1;
        if (child < maxNodes) return true;
        return false;
    }

    /**
     * Calculates minimal amount of nodes needed for a tree with "leafs" leafs
     * and FANOUT children per inner node
     */
    static constexpr uint64_t nodesFromLeaf(uint64_t leafs) {
        return (leafs == 1)
                ? 1
                : 3 + (leafs-2) + ((leafs-2) / (FANOUT-1));
    }

private:
    std::array<combine_node, nodesFromLeaf(MAX_LEAFS)> nodes;
    uint64_t maxNodes = nodesFromLeaf(MAX_LEAFS);
    uint64_t maxLeafs  = MAX_LEAFS;
};

template<size_t MAX_LEAFS, size_t FANOUT>
void TreeCombining<MAX_LEAFS, FANOUT>::initNode(uint64_t idx) {
    ASSERT(idx < nodes.size());
    if (hasChildren(idx)) nodes[idx].value.store(0);
    else nodes[idx].value.store(1);
    for (auto i = 0; i < FANOUT; i++) {
        auto child = idx * FANOUT + i + 1;
        if (child >= maxNodes) return;
        nodes[idx].value.fetch_add(1);
        nodes[child].parent = &nodes[idx];
        initNode(child);
    }
    //MLOG_DETAIL(mlog::app, "init", idx, "children", nodes[idx].value.load());
}

template<size_t MAX_LEAFS, size_t FANOUT>
void TreeCombining<MAX_LEAFS, FANOUT>::init(uint64_t maxLeafs_) {
    ASSERT(maxLeafs_ <= MAX_LEAFS);
    auto maxNodes_ = nodesFromLeaf(maxLeafs_);
    ASSERT(maxNodes_ <= nodes.size());
    maxNodes = maxNodes_;
    maxLeafs = maxLeafs_;
    //MLOG_ERROR(mlog::app, DVAR(maxNodes), DVAR(nodes.size()));
    initNode(0);
}

template<size_t MAX_LEAFS, size_t FANOUT>
TreeCombining<MAX_LEAFS, FANOUT>::TreeCombining() {
    init(MAX_LEAFS);
}

template<size_t MAX_LEAFS, size_t FANOUT>
void TreeCombining<MAX_LEAFS, FANOUT>::dec(uint64_t id) {
    ASSERT(id < maxLeafs);
    auto lastNode = nodesFromLeaf(maxLeafs) - 1;
    auto firstLeaf = (lastNode == 0)?0:parent(lastNode) + 1;
    auto realID = firstLeaf + id;
    if (maxLeafs == 1) realID = 0;
    //MLOG_ERROR(mlog::app, "Dec", DVAR(realID), DVAR(id), DVAR(lastNode), DVAR(firstLeaf));
    nodes[realID].dec();
}

} //namespace mythos
