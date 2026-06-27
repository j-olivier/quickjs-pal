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
#ifndef QUICKJS_PAL_H
#define QUICKJS_PAL_H

#include <stdio.h>
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#define __js_printf_like(f, a)   __attribute__((format(printf, f, a)))
#else
#define __js_printf_like(a, b)
#endif

#define JS_BOOL int

#ifdef __cplusplus
extern "C" {
#endif

/* JSPal: opaque handle for Platform Abstraction Layer OS functions. */
typedef struct JSPal JSPal;

typedef struct JSPalTime {
    int64_t tv_sec;
    int64_t tv_usec;
} JSPalTime;

typedef struct JSPalMutex  { union { void *_align; unsigned char opaque[64]; }; } JSPalMutex;
typedef struct JSPalCond   { union { void *_align; unsigned char opaque[64]; }; } JSPalCond;
typedef struct JSPalThread { union { void *_align; unsigned char opaque[64]; }; } JSPalThread;

/* Every jspal_* function takes a JSPal * as its first argument. */

/* Allocator for the default JSMallocFunctions only. */
extern void *jspal_malloc(JSPal *opaque, size_t size);
extern void jspal_free(JSPal *opaque, void *ptr);
extern void *jspal_realloc(JSPal *opaque, void *ptr, size_t size);
extern size_t jspal_malloc_usable_size(JSPal *opaque, const void *ptr);

/* wall clock time (gettimeofday-equivalent) */
extern void jspal_get_time(JSPal *opaque, JSPalTime *t);
/* monotonic clock, unaffected by wall-clock adjustments */
extern void jspal_get_time_monotonic(JSPal *opaque, JSPalTime *t);
/* timezone offset in minutes for the given time (ms since epoch) */
extern int jspal_get_timezone_offset(JSPal *opaque, int64_t time_ms);
/* printf-style debug output; used only by the engine's dump routines. */
extern int jspal_printf(JSPal *opaque, const char *format, ...) __js_printf_like(2, 3);

/* thread */
/* stack_size == 0 means "use the platform default" */
extern int jspal_thread_create(JSPal *opaque, JSPalThread *thread, void *(*start)(void *arg), void *arg, size_t stack_size);
extern int jspal_thread_join(JSPal *opaque, JSPalThread *thread);
extern int jspal_thread_detach(JSPal *opaque, JSPalThread *thread);

/* mutex */
extern void jspal_mutex_init(JSPal *opaque, JSPalMutex *mutex);
extern void jspal_mutex_destroy(JSPal *opaque, JSPalMutex *mutex);
extern void jspal_mutex_lock(JSPal *opaque, JSPalMutex *mutex);
extern void jspal_mutex_unlock(JSPal *opaque, JSPalMutex *mutex);

/* condition variable */
extern void jspal_cond_init(JSPal *opaque, JSPalCond *cond);
extern void jspal_cond_destroy(JSPal *opaque, JSPalCond *cond);
extern void jspal_cond_wait(JSPal *opaque, JSPalCond *cond, JSPalMutex *mutex);
/* returns 0 on success, nonzero on timeout */
extern int jspal_cond_timedwait(JSPal *opaque, JSPalCond *cond, JSPalMutex *mutex, const JSPalTime *abstime);
extern void jspal_cond_signal(JSPal *opaque, JSPalCond *cond);
extern void jspal_cond_broadcast(JSPal *opaque, JSPalCond *cond);

/* atomic ops backing the JS Atomics object / SharedArrayBuffer. */
extern uint8_t  jspal_atomic_load_8(uint8_t *ptr);
extern uint16_t jspal_atomic_load_16(uint16_t *ptr);
extern uint32_t jspal_atomic_load_32(uint32_t *ptr);
extern uint64_t jspal_atomic_load_64(uint64_t *ptr);
extern void jspal_atomic_store_8(uint8_t *ptr, uint8_t v);
extern void jspal_atomic_store_16(uint16_t *ptr, uint16_t v);
extern void jspal_atomic_store_32(uint32_t *ptr, uint32_t v);
extern void jspal_atomic_store_64(uint64_t *ptr, uint64_t v);
extern uint8_t  jspal_atomic_exchange_8(uint8_t *ptr, uint8_t v);
extern uint16_t jspal_atomic_exchange_16(uint16_t *ptr, uint16_t v);
extern uint32_t jspal_atomic_exchange_32(uint32_t *ptr, uint32_t v);
extern uint64_t jspal_atomic_exchange_64(uint64_t *ptr, uint64_t v);
extern JS_BOOL jspal_atomic_compare_exchange_8(uint8_t *ptr, uint8_t *expected, uint8_t desired);
extern JS_BOOL jspal_atomic_compare_exchange_16(uint16_t *ptr, uint16_t *expected, uint16_t desired);
extern JS_BOOL jspal_atomic_compare_exchange_32(uint32_t *ptr, uint32_t *expected, uint32_t desired);
extern JS_BOOL jspal_atomic_compare_exchange_64(uint64_t *ptr, uint64_t *expected, uint64_t desired);
extern uint8_t  jspal_atomic_fetch_add_8(uint8_t *ptr, uint8_t v);
extern uint16_t jspal_atomic_fetch_add_16(uint16_t *ptr, uint16_t v);
extern uint32_t jspal_atomic_fetch_add_32(uint32_t *ptr, uint32_t v);
extern uint64_t jspal_atomic_fetch_add_64(uint64_t *ptr, uint64_t v);
extern uint8_t  jspal_atomic_fetch_sub_8(uint8_t *ptr, uint8_t v);
extern uint16_t jspal_atomic_fetch_sub_16(uint16_t *ptr, uint16_t v);
extern uint32_t jspal_atomic_fetch_sub_32(uint32_t *ptr, uint32_t v);
extern uint64_t jspal_atomic_fetch_sub_64(uint64_t *ptr, uint64_t v);
extern uint8_t  jspal_atomic_fetch_and_8(uint8_t *ptr, uint8_t v);
extern uint16_t jspal_atomic_fetch_and_16(uint16_t *ptr, uint16_t v);
extern uint32_t jspal_atomic_fetch_and_32(uint32_t *ptr, uint32_t v);
extern uint64_t jspal_atomic_fetch_and_64(uint64_t *ptr, uint64_t v);
extern uint8_t  jspal_atomic_fetch_or_8(uint8_t *ptr, uint8_t v);
extern uint16_t jspal_atomic_fetch_or_16(uint16_t *ptr, uint16_t v);
extern uint32_t jspal_atomic_fetch_or_32(uint32_t *ptr, uint32_t v);
extern uint64_t jspal_atomic_fetch_or_64(uint64_t *ptr, uint64_t v);
extern uint8_t  jspal_atomic_fetch_xor_8(uint8_t *ptr, uint8_t v);
extern uint16_t jspal_atomic_fetch_xor_16(uint16_t *ptr, uint16_t v);
extern uint32_t jspal_atomic_fetch_xor_32(uint32_t *ptr, uint32_t v);
extern uint64_t jspal_atomic_fetch_xor_64(uint64_t *ptr, uint64_t v);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QUICKJS_PAL_H */
