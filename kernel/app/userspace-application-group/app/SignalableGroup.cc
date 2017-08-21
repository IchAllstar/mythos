#include "app/SignalableGroup.hh"

int64_t TreeStrategy::tmp[20] = {0};

void SignalableGroup::addMember(ISignalable *t) {
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

void SignalableGroup::signalAll() {
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
