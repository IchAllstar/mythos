#include "app/SignalGroup.hh"

int64_t TreeStrategy::tmp[TreeStrategy::RECURSIVE_SIZE] = {0};

void SignalGroup::addMember(ISignalable *t) {
  for (int i = 0; i < MAX; i++) {
    //MLOG_ERROR(mlog::app, "Add ", i);
    if (member[i] == nullptr) {
      member[i] = t;
      size++;
      return;
    }
  }
  MLOG_ERROR(mlog::app, "Group full");
}

void SignalGroup::signalAll() {
  switch (strat) {
    case TREE:
      TreeStrategy::cast(this, 0, size);
      break;
    case SEQUENTIAL:
      SequentialStrategy::cast(this, 0, size);
      break;
    case HELPER:
      HelperStrategy::cast(this, 0, size);
      break;
  }
}
