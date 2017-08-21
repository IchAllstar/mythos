#include "app/SignalableGroup.hh"

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
  SequentialStrategy::cast(this, 0, size);
}
