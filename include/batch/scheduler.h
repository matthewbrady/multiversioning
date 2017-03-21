#ifndef BATCH_SCHEDULER_H_
#define BATCH_SCHEDULER_H_

#include "batch/batch_action.h"
#include "batch/lock_table.h"
#include "batch/MS_queue.h"
#include "batch/container.h"
#include "batch/input_queue.h"
#include "runnable.hh"

class InputQueue;

// TODO:
//    - Add handle to the global schedule here so that we may merge 
//      the local schedule in every time we need to. 
struct SchedulerConfig {
  uint32_t batch_size_act;
  uint32_t batch_length_sec;
  int m_cpu_number;
  InputQueue* input;
  // TODO:
  //   GlobalSchedule* gc;
};

enum class SchedulerState { 
  waiting_for_input = 0,
  input,
  batch_creation,
  waiting_to_merge,
  batch_merging,
  waiting_to_signal_execution,
  signaling_execution,
  state_count
};

//  Scheduler
//
//    Implementation of the scheduling thread. Scheduling threads may be 
//    thought of as state machines. The state transitions are described below.
//
//    State transition of scheduler:
//      - waiting_for_input 
//          -- Awaits its turn to dequeue actions from the input queue. Input
//            Queue is a single-producer single-consumer MS-queue that contains 
//            all of the transactions in the system.
//      - input 
//          -- Dequeues transactions from the input queue.
//      - batch_creation 
//          -- Creates the batch schedule. This is done offline and does not
//          conflict with anything else in the system.
//      - waiting to merge 
//          -- Awaits for the logical lock on merging its schedule into the 
//          global schedule.
//      - batch merging
//          -- Merges into the global schedule.
//      - waiting to signal execution
//          -- Awaits for the logical lock on signaling to execution threads.
//          Signaling execution threads means passing them pointers to
//          actions that they own. For more information see include/batch/executor.h
//      - signaling execution
//          -- Signals execution threads
//      - waiting for input
//      - ...
//
//
//      TODO:
//          Tests for:
//            - StartWorking 
//            - Init
//            - make_batch_schedule
class Scheduler : public Runnable {
public:
  typedef Container::BatchActions BatchActions;
protected:
  std::unique_ptr<BatchActions> batch_actions;
  BatchLockTable lt;
  SchedulerConfig conf;
  SchedulerState state;

  // state change is done using CAS and is thread safe.
  bool change_state(SchedulerState nextState, SchedulerState expectedCurrState);
  void make_batch_schedule();
public:
  Scheduler(SchedulerConfig sc);

  // override Runnable interface
  void StartWorking() override;
  void Init() override;

  // own functions
  SchedulerState get_state();
  unsigned int get_max_actions();

  virtual void put_action(std::unique_ptr<BatchAction> act);

  // All of the below are thread safe.
  bool signal_waiting_for_input();
  bool signal_input();
  bool signal_batch_creation();
  bool signal_waiting_for_merge();
  bool signal_merging();
  bool signal_waiting_for_exec_signal();
  bool signal_exec_signal();
};

#endif // BATCH_SCHEDULER_H_
