#ifndef _LOCK_STAGE_H_
#define _LOCK_STAGE_H_

#include "batch/lock_type.h"

#include <memory>
#include <cstding>

// TODO: substitute Txn for the new action type
// TODO: include the new action type
// TODO: Override the new/delete operators
// TODO: implement a public inheritance test class which provides a == operator.

class LockStage {
protected:
  typedef std::shared_ptr<Txn> Txn_spt;
  typedef std::unordered_set<Txn_spt> RequestingTxns;
  typedef std::shared_ptr<LockStage> Stage_spt;

  // The number of transactions holding on to the lock.
  uint64_t holders;
  Stage_spt next_stage;
  LockType l_type;
  ResquestingTxns requesters;

public:
  LockStage();
  LockStage(
      RequestingTxns requesters,
      LockType lt,
      Stage_spt ns = nullptr);

  // Returns true if successful and false otherwise
  bool addToStage(Txn_spt txn, LockType lt);
  // Returns the new value of holders
  uint64_t decrementHolders(); 
  // Returns true if successful and false otherwise.
  //
  // Failure reported when next_stage was non-null
  bool setNextStage(LockStage_spt ns);

  LockStage_spt getNextStage();
  const RequestingTxns& getRequesters() const; 

  friend bool operator==(const LockStage& ls1, const LockStage& ls2);
};

#endif // _LOCK_STAGE_H_
