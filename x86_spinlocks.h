#ifndef ZK_SPINLOCK_H_
#define ZK_SPINLOCK_H_ 
#include <sched.h>

typedef struct{
   volatile unsigned int lock;
   volatile int locked;
}spinlock_t;

#define spinlock_init(rw)  do { (rw)->lock = 0x00000001; (rw)->pid=-1; (rw)->locked=0;} while(0)
#define SPINLOCK_INITIALIZER  {0x00000001, 0}
#define spinlock_try_lock(rw)  asm volatile("lock ; decl %0" :"=m" ((rw)->lock) : : "memory")
#define _spinlock_unlock(rw)   asm volatile("lock ; incl %0" :"=m" ((rw)->lock) : : "memory")

static inline void yield()
{
    sched_yield();
}

static inline void spinlock_unlock(spinlock_t* rw) {
  if (rw->locked) {
    rw->locked = 0;
    _spinlock_unlock(rw);
  }
}

static inline void spinlock_lock(spinlock_t* rw)
{
  while (1) {
    spinlock_try_lock(rw);
    if (rw->lock == 0) {
      rw->locked = 1;
      return;
    }
    _spinlock_unlock(rw);
    yield();
  }
}

#endif
