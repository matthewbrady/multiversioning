#ifndef BATCH_ACTION_H_
#define BATCH_ACTION_H_

#include <db.h>

#include <stdint.h>
#include <unordered_set>
#include <city.h>

const uint64_t DEFAULT_TABLE_ID = 0;

class RecordKey {
  public:
    uint64_t key;
    uint64_t table_id;

    RecordKey(uint64_t key, uint64_t table_id) : key(key), table_id(table_id) {}

    // If table unspecified, set it to the default table (simplifies creating 
    // keys for tests, not sure if this is the right place for it) *
    RecordKey(uint64_t key) : key(key), table_id(DEFAULT_TABLE_ID) {}

    bool operator==(const RecordKey &other) const {
      return (key == other.key && table_id == other.table_id);
    }
};

namespace std {
  template <>
  struct hash<RecordKey> {

    size_t operator() (const RecordKey &k) const {
      return Hash128to64(std::make_pair(k.key, k.table_id)); 
    }

  };

}

//typedef uint64_t RecordKey;
typedef std::unordered_set<RecordKey> RecordSet;

class BatchAction : public translator {
  public:
    BatchAction(txn* t): translator(t) {};

    virtual void add_read_key(RecordKey rk) = 0;
    virtual void add_write_key(RecordKey rk) = 0;
    
    virtual uint64_t get_readset_size() const = 0;
    virtual uint64_t get_writeset_size() const = 0;
    virtual RecordSet* get_readset_handle() = 0;
    virtual RecordSet* get_writeset_handle() = 0;

    virtual bool operator<(const BatchAction& ba2) const = 0;
};

#endif //BATCH_ACTION_H_
