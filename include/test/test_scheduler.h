#ifndef TEST_SCHEDULER_H_
#define TEST_SCHEDULER_H_

#include "batch/scheduler.h"

class TestScheduler : public Scheduler {
public:
  TestScheduler(SchedulerConfig cf): Scheduler(cf) {};
  std::vector<std::unique_ptr<BatchActionInterface>> put_actions;
  
  void put_action(std::unique_ptr<BatchActionInterface> act) override {
    put_actions.push_back(std::move(act));
  };
};

#endif // TEST_SCHEDULER_H_
