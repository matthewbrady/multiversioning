#include <batch/batch_action.h>
#include <cassert>

uint64_t BatchAction::get_readset_size() const {
  return readset.size();
}

uint64_t BatchAction::get_writeset_size() const {
  return writeset.size();
}

void BatchAction::add_write_key(RecordKey rk) {
  writeset.insert(rk);
}

void BatchAction::add_read_key(RecordKey rk) {
  readset.insert(rk);
}

void* BatchAction::write_ref(uint64_t key, uint32_t table) {
  RecordKey rk(key, table);
  auto it = writeset.find(rk);
  assert(it != writeset.end());//record must be part of this action
  // TODO:
  //    Do we need this anywhere?
  //return it->record;
  return nullptr;
}

void* BatchAction::read(uint64_t key, uint32_t table) {
  RecordKey rk(key, table);
  auto it = readset.find(rk);
  assert(it != readset.end());
  // TODO:
  //    Do we need this anywhere?
  // return it->record;
  return nullptr;
};

BatchActionInterface::RecordKeySet* BatchAction::get_readset_handle() {
  return &readset;
}

BatchActionInterface::RecordKeySet* BatchAction::get_writeset_handle() {
  return &writeset;
}

//currently sort by total number of records in action
bool BatchAction::operator<(const BatchActionInterface& ba2) const {
  return (get_readset_size() + get_writeset_size() < 
          ba2.get_readset_size() + ba2.get_writeset_size());
}

int BatchAction::rand() {
  return 0;
}

void BatchAction::run() {
  // TODO.
}
