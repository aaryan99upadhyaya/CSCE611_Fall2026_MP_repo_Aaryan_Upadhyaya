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

ReadyQueueNode * Scheduler::readyQueueHead = nullptr;
ReadyQueueNode * Scheduler::readyQueueTail = nullptr;

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

  // release stale threads from memory
  while(Thread::staleThreadQueueHead != nullptr) {
    StaleThreadQueueNode * oldHead = Thread::staleThreadQueueHead;
    Thread::staleThreadQueueHead = Thread::staleThreadQueueHead->next;
    terminate(oldHead->thread);
    delete(oldHead);
  }

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
  // Remove the thread from the ready queue
  if(readyQueueHead != nullptr && readyQueueHead->thread == _thread) {
    ReadyQueueNode * oldHead = readyQueueHead;
    readyQueueHead = readyQueueHead->next;
    delete(oldHead);
  } else {
    ReadyQueueNode * curr = readyQueueHead;
    while(curr != nullptr && curr->next != nullptr) {
      if(curr->next->thread == _thread) {
        ReadyQueueNode * toDelete = curr->next;
        curr->next = curr->next->next;
        delete(toDelete);
        delete(_thread);
        break;
      }
      curr = curr->next;
    }
  }

}
