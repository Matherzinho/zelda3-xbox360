// A small pool of worker threads that share out the items of a job with the thread that asks for it, so heavy
// per-frame work (such as drawing a high-resolution picture line by line) runs on the console's other cores.
// Portable pthreads; on the console each worker is placed on its own hardware thread.
#include <pthread.h>
#include <stdbool.h>
#include "s360_jobs.h"
#if defined(S360_HAVE_XSDK)
#include "xsdk/thread.h"
#endif

enum { kMaxWorkers = 5 };

static struct {
  pthread_mutex_t mutex;
  pthread_cond_t start, done;
  pthread_t threads[kMaxWorkers];
  int workers;
  unsigned generation;  // bumped for each job
  int busy;             // workers still on the current job
  S360JobFunc *func;
  void *arg;
  int count;
  int next;  // next item to take, shared by everyone on the job
} g_pool = { .mutex = PTHREAD_MUTEX_INITIALIZER, .start = PTHREAD_COND_INITIALIZER, .done = PTHREAD_COND_INITIALIZER };

static void TakeItems(S360JobFunc *func, void *arg, int count) {
  for (;;) {
    int i = __atomic_fetch_add(&g_pool.next, 1, __ATOMIC_RELAXED);
    if (i >= count) return;
    func(arg, i);
  }
}

static void *Worker(void *unused) {
  (void)unused;
  unsigned seen = 0;
  pthread_mutex_lock(&g_pool.mutex);
  for (;;) {
    while (g_pool.generation == seen) pthread_cond_wait(&g_pool.start, &g_pool.mutex);
    seen = g_pool.generation;
    S360JobFunc *func = g_pool.func;
    void *arg = g_pool.arg;
    int count = g_pool.count;
    pthread_mutex_unlock(&g_pool.mutex);
    TakeItems(func, arg, count);
    pthread_mutex_lock(&g_pool.mutex);
    if (--g_pool.busy == 0) pthread_cond_signal(&g_pool.done);
  }
  return NULL;
}

int s360_jobs_start(int workers, const int *hardware_threads) {
  if (workers > kMaxWorkers) workers = kMaxWorkers;
  for (int i = 0; i < workers; i++) {
    if (pthread_create(&g_pool.threads[g_pool.workers], NULL, Worker, NULL) != 0) break;
#if defined(S360_HAVE_XSDK)
    if (hardware_threads) xsdk_thread_place(xsdk_pthread_handle(g_pool.threads[g_pool.workers]), (uint32_t)hardware_threads[i]);
#else
    (void)hardware_threads;
#endif
    g_pool.workers++;
  }
  return g_pool.workers;
}

int s360_jobs_workers(void) { return g_pool.workers; }

void s360_jobs_run(S360JobFunc *func, void *arg, int count) {
  if (count <= 0) return;
  if (g_pool.workers == 0 || count == 1) {
    for (int i = 0; i < count; i++) func(arg, i);
    return;
  }
  pthread_mutex_lock(&g_pool.mutex);
  g_pool.func = func, g_pool.arg = arg, g_pool.count = count;
  __atomic_store_n(&g_pool.next, 0, __ATOMIC_RELAXED);
  g_pool.busy = g_pool.workers;
  g_pool.generation++;
  pthread_cond_broadcast(&g_pool.start);
  pthread_mutex_unlock(&g_pool.mutex);

  TakeItems(func, arg, count);  // this thread works too

  pthread_mutex_lock(&g_pool.mutex);
  while (g_pool.busy != 0) pthread_cond_wait(&g_pool.done, &g_pool.mutex);
  pthread_mutex_unlock(&g_pool.mutex);
}
