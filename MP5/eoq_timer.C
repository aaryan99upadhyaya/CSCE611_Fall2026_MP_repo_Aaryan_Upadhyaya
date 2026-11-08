#include "eoq_timer.H"
#include "machine.H"
#include "thread.H"
#include "console.H"

EOQTimer::EOQTimer(unsigned int _hz) 
    : SimpleTimer(_hz) {}

void EOQTimer::register_scheduler(Scheduler *_scheduler) {
    scheduler = _scheduler;
}

void EOQTimer::handle_interrupt(REGS *_r) {

    // Tell PIC that we're done with the interrupt
    Machine::outportb(0x20, 0x20);

    Console::puts("Interrupt invoked! Transferring control to next thread ..\n");

    // Preempt current thread if scheduler is active
    if (scheduler != nullptr && Thread::CurrentThread() != nullptr) {
        scheduler->resume(Thread::CurrentThread());
        scheduler->yield();
    }
}