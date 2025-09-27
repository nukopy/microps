#ifndef PLATFORM_H
#define PLATFORM_H

#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>

/*
 * Memory
 */

static inline void *memory_alloc(size_t size) { return calloc(1, size); }

static inline void memory_free(void *ptr) { free(ptr); }

/*
 * Mutex
 */

typedef pthread_mutex_t mutex_t;

#define MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

static inline int mutex_init(mutex_t *mutex) {
  return pthread_mutex_init(mutex, NULL);
}

static inline int mutex_lock(mutex_t *mutex) {
  return pthread_mutex_lock(mutex);
}

static inline int mutex_unlock(mutex_t *mutex) {
  return pthread_mutex_unlock(mutex);
}

/*
 * Interrupt
 */

// IRQ 番号の開始番号
//
/*

microps では IRQ 番号としてシグナル番号を使用する。

Linux では、SIGRTMIN ~ SIGRTMAX (34 ~ 64)までのシグナルを
アプリケーションが任意の目的で利用できる。

ただし、SIGRTMIN (34) に関しては、glibc が内部的に利用しているため、
+1 した番号から利用するようにしている。

そのため、microps の IRQ 番号は 35 始まり。

*/
#define INTR_IRQ_BASE (SIGRTMIN + 1)

#define INTR_IRQ_SHARED 0x0001

extern int intr_request_irq(unsigned int irq,
                            int (*handler)(unsigned int irq, void *id),
                            int flags, const char *name, void *dev);
extern int intr_raise_irq(unsigned int irq);

extern int intr_run(void);
extern void intr_shutdown(void);
extern int intr_init(void);

#endif
