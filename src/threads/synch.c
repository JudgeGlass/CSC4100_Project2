/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

static void insert_lock_wait_list(struct list *list, struct thread *thrd)
{
  // This is pretty much exactly the same as the insert ready list in the threads file
  struct list_elem *e;
  for (e = list_begin (list); e != list_end (list); e = list_next (e))
  {
    struct thread *curr_thrd = list_entry(e, struct thread, elem);
    struct thread *next_thrd = list_entry(list_next(e), struct thread, elem);

    if(thrd->priority > curr_thrd->priority) // NEW PRI > CURR PRI
      break;
    if(curr_thrd->priority == thrd->priority && thrd->priority > next_thrd->priority) // FIFO
    {
      e = list_next(e);
      break;
    }
  }
  list_insert (e, &thrd->elem);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      insert_lock_wait_list(&sema->waiters, thread_current());
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters)) {
    // Ensure sema waiters priorities are in order
    list_sort(&sema->waiters, thread_cmp_priority, NULL);
    thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem));
  }
  sema->value++;
  // Yield to other threads in case we unblocked a higher priority thraed
  thread_yield();
  intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
  lock->is_donated = false;
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void 
lock_acquire(struct lock *lock)
{
    ASSERT(lock != NULL);
    ASSERT(!intr_context());
    ASSERT(!lock_held_by_current_thread(lock));

    // If the lock is already held, perform priority donation
    if (lock->holder != NULL)
    {
      struct thread *current_thread = thread_current();
      current_thread->waiting_for = lock;
      // Handle priority donation if necessary
      if (lock->holder->priority < current_thread->priority)
      {
        struct thread *donor_thread;
        // Propagate priority donation upwards the chain of locks
        for (donor_thread = current_thread; donor_thread->waiting_for != NULL; donor_thread = donor_thread->waiting_for->holder)
        {
          struct lock *current_lock = donor_thread->waiting_for;

          // Add donor's priority to the lock holder's temp priority list
          current_lock->holder->temp_priorities[current_lock->holder->temp_priorities_size] = donor_thread->priority;
          current_lock->holder->temp_priorities_size++;

          // Update the lock holder's priority to the donor's priority
          current_lock->holder->priority = donor_thread->priority;
        }
        // If the lock has not been donated before then we increment donation count
        if (!lock->is_donated)
        {
          lock->holder->num_donations++;
        }
        // Mark the lock as having been donated to
        lock->is_donated = true;
        // Refresh the ready list for thread scheduling
        thread_refresh_ready_list();
      }
    }

    // Wait for the lock to be available
    sema_down(&lock->semaphore);

    // Acquire the lock
    lock->holder = thread_current();
    lock->holder->waiting_for = NULL;
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void 
lock_release(struct lock *lock)
{
    ASSERT(lock != NULL);
    ASSERT(lock_held_by_current_thread(lock));

    // Retrieve the lock's semaphore
    struct semaphore *lock_sema = &lock->semaphore;

    // Sort the waiters list by priority before releasing the lock
    list_sort(&lock_sema->waiters, thread_cmp_priority, NULL);

    // If the lock had priority donations, handle it
    if (lock->is_donated)
    {
        struct thread *current_thread = thread_current();

        // Decrement the number of donations for the current thread
        current_thread->num_donations--;

        // Get the priority of the thread waiting at the front of the queue
        int front_priority = list_entry(list_front(&lock_sema->waiters), struct thread, elem)->priority;

        // Remove the temporary priority donation for the current thread
        thread_remove_temp_priority(current_thread, front_priority);

        // Restore the thread's priority from the temp priorities list
        current_thread->priority = current_thread->temp_priorities[current_thread->temp_priorities_size - 1];

        // Mark the lock as not donated anymore
        lock->is_donated = false;
    }

    // If there are no more donations for this thread then we restore its original priortiy
    if (thread_current()->num_donations == 0)
    {
        struct thread *current_thread = thread_current();
        current_thread->temp_priorities_size = 1;
        current_thread->priority = current_thread->temp_priorities[0];
    }

    // Release the lock and wake up any waiting threads
    lock->holder = NULL;
    sema_up(&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  struct thread *thrd = list_entry(&waiter.elem, struct thread, elem);
  insert_lock_wait_list(&cond->waiters, thrd);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

bool cmp_sema(struct list_elem *l1, struct list_elem *l2,void *aux)
{
  struct semaphore s1 = list_entry(l1,struct semaphore_elem,elem)->semaphore;
  struct semaphore s2 = list_entry(l2,struct semaphore_elem,elem)->semaphore;
  return list_entry (list_front(&s1.waiters), struct thread, elem)->priority > list_entry (list_front(&s2.waiters),struct thread, elem)->priority;
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  list_sort(&cond->waiters, cmp_sema, NULL);
  if (!list_empty (&cond->waiters)) 
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
