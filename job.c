#include "job.h"

#include <util/atomic.h>
#include <stdlib.h>

struct job {
  void (*run) (void *);
  void *userdata;
  uint8_t flags;

  struct job *next;
};

static struct job *jobs = 0;
static struct job *pending = 0;

void job_sched(void (*run)(void *), void *userdata, int flags) {
  struct job *j = (struct job *)malloc(sizeof(struct job));
  j->run = run;
  j->userdata = userdata;
  j->flags = flags;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    j->next = pending;
    pending = j;
  }
}

void job_run() {
  while(1) {
    if(!jobs && pending) {
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
	jobs = pending;
	pending = 0;
      }
    }
    
    struct job **pp = &jobs;
    while(*pp){
      struct job *j = *pp;
      j->run(j->userdata);
      if(j->flags & JOB_PERSIST) {
	pp = &(*pp)->next;
      } else {
	*pp = (*pp)->next;
	free(j);
      }

      if(!*pp && pending) {
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
	  *pp = pending;
	  pending = 0;
	}
      }
    }
  }
}
