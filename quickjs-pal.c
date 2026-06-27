/*
 * QuickJS default PAL (Platform Abstraction Layer) implementation
 *
 * Copyright (c) 2026 Julien Olivier
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "quickjs-pal.h"

#if !defined(_MSC_VER)
/* uses pthread for Unix & MinGW. */
#include <sys/time.h>
#include <pthread.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#endif
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__) || defined(__GLIBC__)
#include <malloc.h>
#elif defined(__FreeBSD__)
#include <malloc_np.h>
#elif defined(_WIN32)
#include <malloc.h>
#endif

/*----------------------------------------------------------------------*/
/* debug output */

int jspal_printf(JSPal *opaque, const char *format, ...)
{
    va_list ap;
    int ret;

    (void)opaque;
    va_start(ap, format);
    ret = vprintf(format, ap);
    va_end(ap);
    return ret;
}

/*----------------------------------------------------------------------*/
/* time */

void jspal_get_time(JSPal *opaque, JSPalTime *t)
{
    (void)opaque;
#if defined(_MSC_VER)
    FILETIME ft;
    ULARGE_INTEGER uli;
    uint64_t epoch_us;

    /* FILETIME is 100ns ticks since 1601; convert to Unix time. */
    GetSystemTimePreciseAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    epoch_us = uli.QuadPart / 10 - UINT64_C(11644473600000000);
    t->tv_sec = (int64_t)(epoch_us / 1000000);
    t->tv_usec = (int64_t)(epoch_us % 1000000);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    t->tv_sec = tv.tv_sec;
    t->tv_usec = tv.tv_usec;
#endif
}

void jspal_get_time_monotonic(JSPal *opaque, JSPalTime *t)
{
    (void)opaque;
#if defined(_MSC_VER)
    LARGE_INTEGER freq, counter;
    double seconds;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    seconds = (double)counter.QuadPart / (double)freq.QuadPart;
    t->tv_sec = (int64_t)seconds;
    t->tv_usec = (int64_t)((seconds - (double)t->tv_sec) * 1e6);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t->tv_sec = ts.tv_sec;
    t->tv_usec = ts.tv_nsec / 1000;
#endif
}

int jspal_get_timezone_offset(JSPal *opaque, int64_t time)
{
    (void)opaque;
    time_t ti;
    int res;

    time /= 1000; /* convert to seconds */
#if defined(_WIN32)
    /* Windows gmtime/localtime only handle years 1970..3000; clamp to that range. */
    if (time < 0)
        time = 0;
    else if (time > 32503680000LL) /* 3000-12-31T23:59:59Z */
        time = 32503680000LL;
#endif
    if (sizeof(time_t) == 4) {
        /* on 32-bit systems, we need to clamp the time value to the
           range of `time_t`. This is better than truncating values to
           32 bits and hopefully provides the same result as 64-bit
           implementation of localtime_r.
         */
        if ((time_t)-1 < 0) {
            if (time < INT32_MIN) {
                time = INT32_MIN;
            } else if (time > INT32_MAX) {
                time = INT32_MAX;
            }
        } else {
            if (time < 0) {
                time = 0;
            } else if (time > UINT32_MAX) {
                time = UINT32_MAX;
            }
        }
    }
    ti = time;
#if defined(_WIN32)
    {
        struct tm *tm;
        time_t gm_ti, loc_ti;

        tm = gmtime(&ti);
        if (!tm)
            return 0;
        gm_ti = mktime(tm);

        tm = localtime(&ti);
        if (!tm)
            return 0;
        loc_ti = mktime(tm);

        res = (gm_ti - loc_ti) / 60;
    }
#else
    {
        struct tm tm;
        localtime_r(&ti, &tm);
        res = -tm.tm_gmtoff / 60;
    }
#endif
    return res;
}

/*----------------------------------------------------------------------*/
/* memory allocator, backing the default JSMallocFunctions impl only (see
   the comment above JSPalTime in quickjs.h) */

void *jspal_malloc(JSPal *opaque, size_t size)
{
    (void)opaque;
    return malloc(size);
}

void jspal_free(JSPal *opaque, void *ptr)
{
    (void)opaque;
    free(ptr);
}

void *jspal_realloc(JSPal *opaque, void *ptr, size_t size)
{
    (void)opaque;
    return realloc(ptr, size);
}

size_t jspal_malloc_usable_size(JSPal *opaque, const void *ptr)
{
    (void)opaque;
#if defined(__APPLE__)
    return malloc_size(ptr);
#elif defined(_WIN32)
    return _msize((void *)ptr);
#elif defined(__EMSCRIPTEN__)
    return 0;
#elif defined(__linux__) || defined(__GLIBC__)
    return malloc_usable_size((void *)ptr);
#else
    /* return 0 here if this does not compile */
    return malloc_usable_size((void *)ptr);
#endif
}

/*----------------------------------------------------------------------*/
/* mutex / condition variables */

#if defined(_MSC_VER)
/* Windows condition variables can wake without a signal; wakeNbr counts real ones. */
typedef struct {
    CONDITION_VARIABLE cv;
    volatile LONG wakeNbr;
} JSPAL_CONDITION;

static_assert(sizeof(SRWLOCK) <= sizeof(JSPalMutex), "SRWLOCK too big for JSPalMutex");
static_assert(sizeof(JSPAL_CONDITION) <= sizeof(JSPalCond), "JSPAL_CONDITION too big for JSPalCond");
static_assert(sizeof(HANDLE) <= sizeof(JSPalThread), "HANDLE too big for JSPalThread");
#else
static_assert(sizeof(pthread_mutex_t) <= sizeof(JSPalMutex), "pthread_mutex_t too big for JSPalMutex");
static_assert(sizeof(pthread_cond_t) <= sizeof(JSPalCond), "pthread_cond_t too big for JSPalCond");
static_assert(sizeof(pthread_t) <= sizeof(JSPalThread), "pthread_t too big for JSPalThread");
#endif

void jspal_mutex_init(JSPal *opaque, JSPalMutex *mutex)
{
    (void)opaque;
#if defined(_MSC_VER)
    InitializeSRWLock((PSRWLOCK)mutex);
#else
    pthread_mutex_init((pthread_mutex_t *)mutex, NULL);
#endif
}

void jspal_mutex_destroy(JSPal *opaque, JSPalMutex *mutex)
{
    (void)opaque;
#if defined(_MSC_VER)
    /* SRWLOCK requires no destruction. */
    (void)mutex;
#else
    pthread_mutex_destroy((pthread_mutex_t *)mutex);
#endif
}

void jspal_mutex_lock(JSPal *opaque, JSPalMutex *mutex)
{
    (void)opaque;
#if defined(_MSC_VER)
    AcquireSRWLockExclusive((PSRWLOCK)mutex);
#else
    pthread_mutex_lock((pthread_mutex_t *)mutex);
#endif
}

void jspal_mutex_unlock(JSPal *opaque, JSPalMutex *mutex)
{
    (void)opaque;
#if defined(_MSC_VER)
    ReleaseSRWLockExclusive((PSRWLOCK)mutex);
#else
    pthread_mutex_unlock((pthread_mutex_t *)mutex);
#endif
}

void jspal_cond_init(JSPal *opaque, JSPalCond *cond)
{
    (void)opaque;
#if defined(_MSC_VER)
    JSPAL_CONDITION *c = (JSPAL_CONDITION *)cond;
    InitializeConditionVariable(&c->cv);
    c->wakeNbr = 0;
#else
    pthread_cond_init((pthread_cond_t *)cond, NULL);
#endif
}

void jspal_cond_destroy(JSPal *opaque, JSPalCond *cond)
{
    (void)opaque;
#if defined(_MSC_VER)
    /* CONDITION_VARIABLE requires no destruction. */
    (void)cond;
#else
    pthread_cond_destroy((pthread_cond_t *)cond);
#endif
}

void jspal_cond_wait(JSPal *opaque, JSPalCond *cond, JSPalMutex *mutex)
{
    (void)opaque;
#if defined(_MSC_VER)
    JSPAL_CONDITION *c = (JSPAL_CONDITION *)cond;
    const LONG wakeNbr = c->wakeNbr;

    do {
        SleepConditionVariableSRW(&c->cv, (PSRWLOCK)mutex, INFINITE, 0);
        /* re-check: the wake may not come from a signal */
    } while (c->wakeNbr == wakeNbr);
#else
    pthread_cond_wait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex);
#endif
}

int jspal_cond_timedwait(JSPal *opaque, JSPalCond *cond, JSPalMutex *mutex, const JSPalTime *abstime)
{
#if defined(_MSC_VER)
    JSPAL_CONDITION *c = (JSPAL_CONDITION *)cond;
    const LONG wakeNbr = c->wakeNbr;
    JSPalTime now;
    DWORD timeout_ms;
    BOOL woken;

    for (;;) {
        int64_t delta_us;
        jspal_get_time(opaque, &now);
        delta_us = (abstime->tv_sec - now.tv_sec) * 1000000 + (abstime->tv_usec - now.tv_usec);
        if (delta_us <= 0)
            return ETIMEDOUT; /* absolute deadline reached */
        /* round the wait up to whole milliseconds */
        timeout_ms = (DWORD)((delta_us + 999) / 1000);
        woken = SleepConditionVariableSRW(&c->cv, (PSRWLOCK)mutex, timeout_ms, 0);
        if (c->wakeNbr != wakeNbr)
            return 0; /* a real signal/broadcast happened, even if it raced with a timeout */
        if (!woken && GetLastError() != ERROR_TIMEOUT)
            return -1; /* genuine wait failure */
        /* woke early or without a signal: loop until the deadline really passes */
    }
#else
    struct timespec ts;
    (void)opaque;
    ts.tv_sec = abstime->tv_sec;
    ts.tv_nsec = abstime->tv_usec * 1000;
    return pthread_cond_timedwait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex, &ts);
#endif
}

void jspal_cond_signal(JSPal *opaque, JSPalCond *cond)
{
    (void)opaque;
#if defined(_MSC_VER)
    JSPAL_CONDITION *c = (JSPAL_CONDITION *)cond;
    InterlockedIncrement(&c->wakeNbr);
    WakeConditionVariable(&c->cv);
#else
    pthread_cond_signal((pthread_cond_t *)cond);
#endif
}

void jspal_cond_broadcast(JSPal *opaque, JSPalCond *cond)
{
    (void)opaque;
#if defined(_MSC_VER)
    JSPAL_CONDITION *c = (JSPAL_CONDITION *)cond;
    InterlockedIncrement(&c->wakeNbr);
    WakeAllConditionVariable(&c->cv);
#else
    pthread_cond_broadcast((pthread_cond_t *)cond);
#endif
}

/*----------------------------------------------------------------------*/
/* threads */

int jspal_thread_create(JSPal *opaque, JSPalThread *thread, void *(*start)(void *arg), void *arg,
                              size_t stack_size)
{
    (void)opaque;
#if defined(_MSC_VER)
    HANDLE h;

    h = CreateThread(NULL, stack_size, (LPTHREAD_START_ROUTINE)start, arg, 0, NULL);
    if (!h) return -1;

    *(HANDLE *)thread = h;
    return 0;
#else
    pthread_attr_t attr;
    int ret;

    pthread_attr_init(&attr);
    if (stack_size != 0)
        pthread_attr_setstacksize(&attr, stack_size);
    ret = pthread_create((pthread_t *)thread, &attr, start, arg);
    pthread_attr_destroy(&attr);
    return ret;
#endif
}

int jspal_thread_join(JSPal *opaque, JSPalThread *thread)
{
    (void)opaque;
#if defined(_MSC_VER)
    HANDLE h = *(HANDLE *)thread;
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
    return 0;
#else
    return pthread_join(*(pthread_t *)thread, NULL);
#endif
}

int jspal_thread_detach(JSPal *opaque, JSPalThread *thread)
{
    (void)opaque;
#if defined(_MSC_VER)
    CloseHandle(*(HANDLE *)thread);
    return 0;
#else
    return pthread_detach(*(pthread_t *)thread);
#endif
}

/* atomics: plain C11 stdatomic.h wrappers. */

uint8_t  jspal_atomic_load_8(uint8_t *ptr) { return atomic_load((_Atomic(uint8_t) *)ptr); }
uint16_t jspal_atomic_load_16(uint16_t *ptr) { return atomic_load((_Atomic(uint16_t) *)ptr); }
uint32_t jspal_atomic_load_32(uint32_t *ptr) { return atomic_load((_Atomic(uint32_t) *)ptr); }
uint64_t jspal_atomic_load_64(uint64_t *ptr)  { return atomic_load((_Atomic(uint64_t) *)ptr); }
void jspal_atomic_store_8(uint8_t *ptr, uint8_t v) { atomic_store((_Atomic(uint8_t) *)ptr, v); }
void jspal_atomic_store_16(uint16_t *ptr, uint16_t v) { atomic_store((_Atomic(uint16_t) *)ptr, v); }
void jspal_atomic_store_32(uint32_t *ptr, uint32_t v) { atomic_store((_Atomic(uint32_t) *)ptr, v); }
void jspal_atomic_store_64(uint64_t *ptr, uint64_t v)  { atomic_store((_Atomic(uint64_t) *)ptr, v); }
uint8_t  jspal_atomic_exchange_8(uint8_t *ptr, uint8_t v) { return atomic_exchange((_Atomic(uint8_t) *)ptr, v); }
uint16_t jspal_atomic_exchange_16(uint16_t *ptr, uint16_t v) { return atomic_exchange((_Atomic(uint16_t) *)ptr, v); }
uint32_t jspal_atomic_exchange_32(uint32_t *ptr, uint32_t v) { return atomic_exchange((_Atomic(uint32_t) *)ptr, v); }
uint64_t jspal_atomic_exchange_64(uint64_t *ptr, uint64_t v)  { return atomic_exchange((_Atomic(uint64_t) *)ptr, v); }
JS_BOOL jspal_atomic_compare_exchange_8(uint8_t *ptr, uint8_t *expected, uint8_t desired) { return atomic_compare_exchange_strong((_Atomic(uint8_t) *)ptr, expected, desired); }
JS_BOOL jspal_atomic_compare_exchange_16(uint16_t *ptr, uint16_t *expected, uint16_t desired) { return atomic_compare_exchange_strong((_Atomic(uint16_t) *)ptr, expected, desired); }
JS_BOOL jspal_atomic_compare_exchange_32(uint32_t *ptr, uint32_t *expected, uint32_t desired) { return atomic_compare_exchange_strong((_Atomic(uint32_t) *)ptr, expected, desired); }
JS_BOOL jspal_atomic_compare_exchange_64(uint64_t *ptr, uint64_t *expected, uint64_t desired)  { return atomic_compare_exchange_strong((_Atomic(uint64_t) *)ptr, expected, desired); }
uint8_t  jspal_atomic_fetch_add_8(uint8_t *ptr, uint8_t v) { return atomic_fetch_add((_Atomic(uint8_t) *)ptr, v); }
uint16_t jspal_atomic_fetch_add_16(uint16_t *ptr, uint16_t v) { return atomic_fetch_add((_Atomic(uint16_t) *)ptr, v); }
uint32_t jspal_atomic_fetch_add_32(uint32_t *ptr, uint32_t v) { return atomic_fetch_add((_Atomic(uint32_t) *)ptr, v); }
uint64_t jspal_atomic_fetch_add_64(uint64_t *ptr, uint64_t v)  { return atomic_fetch_add((_Atomic(uint64_t) *)ptr, v); }
uint8_t  jspal_atomic_fetch_sub_8(uint8_t *ptr, uint8_t v) { return atomic_fetch_sub((_Atomic(uint8_t) *)ptr, v); }
uint16_t jspal_atomic_fetch_sub_16(uint16_t *ptr, uint16_t v) { return atomic_fetch_sub((_Atomic(uint16_t) *)ptr, v); }
uint32_t jspal_atomic_fetch_sub_32(uint32_t *ptr, uint32_t v) { return atomic_fetch_sub((_Atomic(uint32_t) *)ptr, v); }
uint64_t jspal_atomic_fetch_sub_64(uint64_t *ptr, uint64_t v)  { return atomic_fetch_sub((_Atomic(uint64_t) *)ptr, v); }
uint8_t  jspal_atomic_fetch_and_8(uint8_t *ptr, uint8_t v) { return atomic_fetch_and((_Atomic(uint8_t) *)ptr, v); }
uint16_t jspal_atomic_fetch_and_16(uint16_t *ptr, uint16_t v) { return atomic_fetch_and((_Atomic(uint16_t) *)ptr, v); }
uint32_t jspal_atomic_fetch_and_32(uint32_t *ptr, uint32_t v) { return atomic_fetch_and((_Atomic(uint32_t) *)ptr, v); }
uint64_t jspal_atomic_fetch_and_64(uint64_t *ptr, uint64_t v)  { return atomic_fetch_and((_Atomic(uint64_t) *)ptr, v); }
uint8_t  jspal_atomic_fetch_or_8(uint8_t *ptr, uint8_t v) { return atomic_fetch_or((_Atomic(uint8_t) *)ptr, v); }
uint16_t jspal_atomic_fetch_or_16(uint16_t *ptr, uint16_t v) { return atomic_fetch_or((_Atomic(uint16_t) *)ptr, v); }
uint32_t jspal_atomic_fetch_or_32(uint32_t *ptr, uint32_t v) { return atomic_fetch_or((_Atomic(uint32_t) *)ptr, v); }
uint64_t jspal_atomic_fetch_or_64(uint64_t *ptr, uint64_t v)  { return atomic_fetch_or((_Atomic(uint64_t) *)ptr, v); }
uint8_t  jspal_atomic_fetch_xor_8(uint8_t *ptr, uint8_t v) { return atomic_fetch_xor((_Atomic(uint8_t) *)ptr, v); }
uint16_t jspal_atomic_fetch_xor_16(uint16_t *ptr, uint16_t v) { return atomic_fetch_xor((_Atomic(uint16_t) *)ptr, v); }
uint32_t jspal_atomic_fetch_xor_32(uint32_t *ptr, uint32_t v) { return atomic_fetch_xor((_Atomic(uint32_t) *)ptr, v); }
uint64_t jspal_atomic_fetch_xor_64(uint64_t *ptr, uint64_t v) { return atomic_fetch_xor((_Atomic(uint64_t) *)ptr, v); }
