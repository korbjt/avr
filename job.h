static const int JOB_PERSIST = 0x01;

void job_sched(void (*run)(void *), void *userdata, int flags);
void job_run();
