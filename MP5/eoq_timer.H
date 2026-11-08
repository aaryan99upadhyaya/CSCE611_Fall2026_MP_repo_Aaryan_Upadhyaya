#ifndef _EOQ_TIMER_H_
#define _EOQ_TIMER_H_

#include "simple_timer.H"
#include "scheduler.H"

class EOQTimer : public SimpleTimer {
private:
    Scheduler *scheduler;

public:
    EOQTimer(unsigned int _hz);
    void handle_interrupt(REGS *_r) override;
    void register_scheduler(Scheduler *_scheduler);

};

#endif