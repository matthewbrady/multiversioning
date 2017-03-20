#ifndef PACKING_H_
#define PACKING_H_

#include <batch/container.h>

#include <vector>
#include <unordered_set>

/**
 * Pseudo - static class containing logic for creation of packings. 
 *
 * This is inherently single-threaded and no synchronization if provided.
 **/
class Packer {
private:
  typedef BatchAction::RecKey RecordKey;
  typedef BatchAction::RecSet RecordSet;
  typedef Container::BatchActions BatchActions;

  Packer();

  static bool txn_conflicts(
      BatchAction* t, 
      RecordSet* ex_locks_in_packing, 
      RecordSet* sh_locks_in_packing); 

public:
  static BatchActions get_packing(Container* c);
};

#endif // PACKING_H_
