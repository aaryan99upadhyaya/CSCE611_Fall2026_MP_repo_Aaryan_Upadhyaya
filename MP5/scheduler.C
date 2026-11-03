/*
 File: scheduler.C
 
 Author:
 Date  :
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "thread.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

Scheduler::Scheduler() {

  Console::puts("Constructing the scheduler ...\n");

  // Initialize the FIFO policy pointers
  readyQueueHead = nullptr;
  readyQueueTail = nullptr;

  // space for end of quantum handler

  Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield() {
  Thread * nextThread = readyQueueHead->thread;
  ReadyQueueNode * oldHead = readyQueueHead;
  readyQueueHead = readyQueueHead->next;
  delete(oldHead);
  Thread::dispatch_to(nextThread);
}

void Scheduler::resume(Thread * _thread) {
  ReadyQueueNode * newNode = new ReadyQueueNode();
  newNode->thread = _thread;
  newNode->next = nullptr;
  if(readyQueueHead == nullptr) {
    readyQueueHead = newNode;
    readyQueueTail = newNode;
  } else {
    readyQueueTail->next = newNode;
    readyQueueTail = newNode;
  }
}

void Scheduler::add(Thread * _thread) {
  resume(_thread);
}

void Scheduler::terminate(Thread * _thread) {
  assert(false);
}
