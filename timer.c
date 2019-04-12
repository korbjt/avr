#include "job.h"
#include "timer.h"

#include <stdint.h>
#include <stdlib.h>
#include <util/atomic.h>

void timer_init() {
  // configure 8bit timer
  TCCR0A = 0;
  // clk/8
  TCCR0B = _BV(CS01);
  // timer overflow interrupt enable
  TIMSK0 = _BV(TOIE0);
}

struct timer {
  uint32_t ticks;
  uint32_t target;
  void (*tick)(void *);
  void *userdata;
  uint8_t flags;
  
  struct timer *next;
};

static struct timer *timers = 0;
static struct timer *pending = 0;

void timer_sched(uint32_t ms, void (*tick)(void *), void *userdata, uint8_t flags) {
  struct timer *t = (struct timer *)malloc(sizeof(struct timer));
  t->ticks = 0;
  t->target = ms;
  t->tick = tick;
  t->userdata = userdata;
  t->flags = flags;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    t->next = pending;
    pending = t;
  }
}

void update_timers(void *userdata) {
  if(!timers && pending) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      timers = pending;
      pending = 0;
    }
  }

  struct timer **pp = &timers;
  while(*pp) {
    struct timer *t = *pp;
    if(++t->ticks > t->target) {
      t->tick(t->userdata);
      t->ticks = 0;
      if(t->flags & TIMER_PERSIST) {
	pp = &(*pp)->next;
      } else {
	*pp = (*pp)->next;
	free(t);
      }
    } else {
      pp = &(*pp)->next;
    }

    if(!*pp && pending) {
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
	*pp = pending;
	pending = 0;
      }
    }
  }
}

static volatile uint8_t ticks = 0;
ISR(TIMER0_OVF_vect) {
  if(++ticks == 8) {
    job_sched(update_timers, 0, 0);
    ticks = 0;
  }
}

