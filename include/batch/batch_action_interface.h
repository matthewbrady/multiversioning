#ifndef BATCH_ACTION_INTERFACE_H_
#define BATCH_ACTION_INTERFACE_H_

#include "batch/record_key.h"
#include "db.h"

#include <stdint.h>
#include <unordered_set>

// BatchActionInterface
//
//    The general interface of an action used within our
//    system. 
class BatchActionInterface : public translator {
  public:
    // typedefs
    typedef std::unordered_set<RecordKey> RecordKeySet;

    BatchActionInterface(txn* t): translator(t) {};

    virtual void add_read_key(RecordKey rk) = 0;
    virtual void add_write_key(RecordKey rk) = 0;
    
    virtual uint64_t get_readset_size() const = 0;
    virtual uint64_t get_writeset_size() const = 0;
    virtual RecordKeySet* get_readset_handle() = 0;
    virtual RecordKeySet* get_writeset_handle() = 0;

    // TODO: 
    //    Run must get a handle to the database storage
    virtual void run() = 0;

    virtual bool operator<(const BatchActionInterface& ba2) const = 0;
};

#endif // BATCH_ACTION_INTERFACE_H_
