#include <occ.h>
#include <action.h>

OCCWorker::OCCWorker(OCCWorkerConfig conf, struct RecordBuffersConfig rb_conf)
        : Runnable(conf.cpu)
{
        this->config = conf;
        this->bufs = new(conf.cpu) RecordBuffers(rb_conf);
}

void OCCWorker::Init()
{
}

/*
 * Process batches of transactions.
 */
void OCCWorker::StartWorking()
{
        OCCActionBatch input, output;
        while (true) {
                input = config.inputQueue->DequeueBlocking();
                for (uint32_t i = 0; i < input.batchSize; ++i) {
                        RunSingle(input.batch[i]);
                        if (config.is_leader) 
                                UpdateEpoch();
                }
                config.outputQueue->EnqueueBlocking(input);
        }
}

void OCCWorker::UpdateEpoch()
{
        uint64_t now = rdtsc();
        if (now - incr_timestamp > config.epoch_threshold)
                fetch_and_increment_32(config.epoch_ptr);
}

/*
 * Run the action to completion. If the transaction aborts due to a conflict, 
 * retry.
 */
void OCCWorker::RunSingle(OCCAction *action)
{
        uint64_t cur_tid;
        bool commit, no_conflicts;
        volatile uint32_t epoch;
        PrepareWrites(action);
        PrepareReads(action);
        while (true) {
                ObtainTIDs(action);
                commit = action->Run();
                AcquireWriteLocks(action);
                barrier();
                epoch = *config.epoch_ptr;
                barrier();
                if (Validate(action)) {
                        if (commit) {
                                cur_tid = ComputeTID(action, epoch);
                                InstallWrites(action, cur_tid);
                        }
                        ReleaseWriteLocks(action);
                        RecycleBufs(action);
                        break;
                } else {
                        ReleaseWriteLocks(action);
                }
        }
}

/*
 * The TID exceeds the existing TIDs of every record in the readset, writeset, 
 * and epoch.
 */
uint64_t OCCWorker::ComputeTID(OCCAction *action, uint32_t epoch)
{
        uint64_t max_tid, cur_tid;
        uint32_t num_reads, num_writes, i;
        if (last_epoch < epoch) {
                txn_counter = 1;
                last_epoch = epoch;
        } else {
                txn_counter += 1;
        }
        max_tid = CREATE_TID(epoch, txn_counter); 
        num_reads = action->readset.size();
        num_writes = action->writeset.size();
        for (i = 0; i < num_reads; ++i) {
                cur_tid = GET_TIMESTAMP(action->readset[i].old_tid);
                if (cur_tid > max_tid)
                        max_tid = cur_tid;
        }
        for (i = 0; i < num_writes; ++i) {
                assert(IS_LOCKED(*(uint64_t*)action->write_records[i]));
                cur_tid = GET_TIMESTAMP(*(uint64_t*)action->write_records[i]);
                assert(!IS_LOCKED(cur_tid));
                if (cur_tid > max_tid)
                        max_tid = cur_tid;
        }
        max_tid += 0x10;
        return max_tid;
}

/*
 * Remember the TID of every record in the readset. Used for validation.
 */
void OCCWorker::ObtainTIDs(OCCAction *action)
{
        uint32_t num_reads, i;
        volatile uint64_t *tid_ptr;
        num_reads = action->readset.size();
        for (i = 0; i < num_reads; ++i) {
                tid_ptr = (volatile uint64_t*)action->readset[i].value;
                action->readset[i].old_tid = *tid_ptr;
        }
}

/*
 * Obtain a reference to each record in the readset. Remember the TID and keep a
 * reference for each record.
 */
void OCCWorker::PrepareReads(OCCAction *action)
{
        uint32_t num_reads, table_id;
        uint64_t key;
        volatile uint64_t old_tid;
        void *value;
        num_reads = action->readset.size();
        for (uint32_t i = 0; i < num_reads; ++i) {
                table_id = action->readset[i].tableId;
                key = action->readset[i].key;
                value = config.tables[table_id]->Get(key);
                action->readset[i].value = value;
        }
}

/*
 * 
 */
void OCCWorker::InstallWrites(OCCAction *action, uint64_t tid)
{
        assert(action->writeset.size() == action->write_records.size());
        assert(!IS_LOCKED(tid));
        uint32_t record_size, table_id;
        uint64_t key;
        void *value;
        for (uint32_t i = 0; i < action->writeset.size(); ++i) {
                value = action->write_records[i];
                memcpy(RECORD_VALUE_PTR(value), action->writeset[i].GetValue(),
                       record_size);
                xchgq(RECORD_TID_PTR(value), tid);
        }
}

/*
 *
 */
void OCCWorker::PrepareWrites(OCCAction *action)
{
        uint32_t num_writes, table_id;
        uint64_t key;
        void *rec;
        void *value;
        num_writes = action->writeset.size();
        for (uint32_t i = 0; i < num_writes; ++i) {
                table_id = action->writeset[i].tableId;
                key = action->writeset[i].key;
                rec = bufs->GetRecord(table_id);
                action->writeset[i].value = rec;
                value = config.tables[table_id]->Get(key);
                action->write_records[i] = value;
        }
}

/*
 *
 */
void OCCWorker::RecycleBufs(OCCAction *action)
{
        uint32_t num_writes, table_id;
        void *rec;
        num_writes = action->writeset.size();
        for (uint32_t i = 0; i < num_writes; ++i) {
                table_id = action->writeset[i].tableId;
                rec = action->writeset[i].value;
                bufs->ReturnRecord(table_id, rec);
        }
}

/*
 * Silo's validation protocol.
 */
bool OCCWorker::Validate(OCCAction *action)
{
        uint32_t num_reads, i;
        num_reads = action->readset.size();
        for (i = 0; i < num_reads; ++i) 
                if (!action->readset[i].ValidateRead())
                        return false;
        return true;
}

/*
 * Try to acquire a record's write latch.
 */
inline bool OCCWorker::TryAcquireLock(volatile uint64_t *version_ptr)
{
        volatile uint64_t cmp_tid, locked_tid;
        cmp_tid = *version_ptr;
        locked_tid = (cmp_tid | 1);
        if (!IS_LOCKED(cmp_tid) &&
            cmp_and_swap(version_ptr, cmp_tid, locked_tid))
                return true;
        return false;
}

/*
 * Acquire write lock for a single record. Exponential back-off under 
 * contention.
 */
void OCCWorker::AcquireSingleLock(volatile uint64_t *version_ptr)
{
        uint32_t backoff, temp;
        if (USE_BACKOFF) 
                backoff = 1;
        while (true) {
                if (TryAcquireLock(version_ptr)) {
                        assert(IS_LOCKED(*version_ptr));
                        break;
                }
                if (USE_BACKOFF) {
                        temp = backoff;
                        while (temp-- > 0)
                                single_work();
                        backoff = backoff*2;
                }
        }
}

/*
 * Acquire a lock for every record in the transaction's writeset.
 */
void OCCWorker::AcquireWriteLocks(OCCAction *action)
{
        uint32_t num_writes, i;
        num_writes = action->writeset.size();
        for (i = 0; i < num_writes; ++i) 
                AcquireSingleLock((volatile uint64_t*)action->write_records[i]);
}

/*
 * Release write locks by zero-ing the least significant 4 bits.
 */
void OCCWorker::ReleaseWriteLocks(OCCAction *action)
{
        uint32_t num_writes, i;
        volatile uint64_t *tid_ptr;
        uint64_t old_tid;
        num_writes = action->writeset.size();
        for (i = 0; i < num_writes; ++i) {
                tid_ptr = (volatile uint64_t*)action->write_records[i];
                assert(IS_LOCKED(*tid_ptr));
                old_tid = *tid_ptr & ~TIMESTAMP_MASK;
                assert(!IS_LOCKED(old_tid));
                assert(GET_TIMESTAMP(old_tid) == GET_TIMESTAMP(*tid_ptr));
                xchgq(tid_ptr, old_tid);
        }
}

/*
 * Create a linked list of thread local buffers for a particular type of record.
 * Given the size of each buffer and the number of buffers.
 */
void RecordBuffers::LinkBufs(struct RecordBuffy *start, uint32_t buf_size,
                             uint32_t num_bufs)
{
        uint32_t offset, i;
        char *cur;
        struct RecordBuffy *temp;
        offset = sizeof(struct RecordBuffy*) + buf_size;
        cur = (char*)start;
        for (i = 0; i < num_bufs; ++i) {
                temp  = (struct RecordBuffy*)cur;
                temp->next = (struct RecordBuffy*)(cur + offset);
                cur = (char*)(temp->next);
        }
        temp->next = NULL;
}

void* RecordBuffers::AllocBufs(struct RecordBuffersConfig conf)
{
        uint32_t i;
        uint64_t total_size, single_buf_sz;
        for (i = 0; i < conf.num_tables; ++i) {
                single_buf_sz =
                        sizeof(struct RecordBuffy*)+conf.record_sizes[i];
                total_size += conf.num_buffers * single_buf_sz;
        }
        return alloc_mem(total_size, conf.cpu);
}

/*
 * 
 */
RecordBuffers::RecordBuffers(struct RecordBuffersConfig conf)
{
        uint32_t i;
        uint64_t total_size;
        char *temp;
        temp = (char *)alloc_mem(conf.num_tables*sizeof(struct RecordBuffy*),
                                 conf.cpu);
        record_lists = (struct RecordBuffy**)temp;
        temp = (char*)AllocBufs(conf);
        for (i = 0; i < conf.num_tables; ++i) {
                LinkBufs((struct RecordBuffy*)temp, conf.record_sizes[i],
                         conf.num_buffers);
                record_lists[i] = (struct RecordBuffy*)temp;
                temp += conf.record_sizes[i]*conf.num_buffers;
        }
}

void* RecordBuffers::GetRecord(uint32_t tableId)
{
        RecordBuffy *ret;
        assert(record_lists[tableId] != NULL);
        ret = record_lists[tableId];
        record_lists[tableId] = ret->next;
        ret->next = NULL;
        return ret;
}

void RecordBuffers::ReturnRecord(uint32_t tableId, void *record)
{
        RecordBuffy *ret;
        ret = (RecordBuffy*)record;
        ret->next = record_lists[tableId];
        record_lists[tableId] = ret;
}
