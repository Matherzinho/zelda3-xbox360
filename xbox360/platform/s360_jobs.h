// Worker threads for splitting per-frame work across the console's cores (s360_jobs.c). Portable C with pthreads.
#ifndef S360_JOBS_H
#define S360_JOBS_H

typedef void S360JobFunc(void *arg, int index);

// Starts up to that many workers, the i-th on hardware_threads[i] on the console (NULL: where the kernel puts them).
// Returns how many started; with none, jobs run on the calling thread.
int s360_jobs_start(int workers, const int *hardware_threads);
int s360_jobs_workers(void);
// Calls func(arg, i) for every i in [0, count), shared between the workers and the calling thread, in no set order,
// and returns when all are done. Work done before the call is visible to func, and func's work after it returns.
void s360_jobs_run(S360JobFunc *func, void *arg, int count);

#endif
