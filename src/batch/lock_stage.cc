#include "batch/lock_stage.h"
#include "utils.h"

LockStage::LockStage(): 
    holders(0),
    next_stage(nullptr),
    type(LockType::shared),
    requesters({})
  {};

LockStage::LockStage(
      RequestingTxns requesters,
      LockType lt,
      Stage_spt ls = nullptr,
      Stage_spt ns = nullptr):
    holders(requesters.size()),
    next_stage(ns),
    type(lt),
    requesters(requesters)
  {
    assert(!(t == LockType::exclusive && requesters.size() > 1));
  };

bool LockStage::addToStage(Txn_spt txn, LockType lt) {
  // can only add to a stage when both the request and the stage are shared
  // or when the stage is empty.
  if ((lt == LockType::exclusive && requesters.size() > 0) ||
      l_type == LockType::exclusive) {
    return false;
  }

  requesters.insert(txn);
  l_type = req_type;
  fetch_and_increment(&holders);
  return true;
}

uint64_t LockStage::decrementHolders() {
  fetch_and_decrement(&holders);
}

bool LockStage::setNextStage(LockStage_spt ns) {
  return cmp_and_swap((uint64_t*) next_stage, nullptr, ns); 
}

LockStage_spt getNextStage() {
  return next_stage; 
}

const RequestingTxns& LockStage::getRequesters() const {
  return requesters;
}
