/*
 * QuickJS libc host-binding PAL (process spawn/wait/kill)
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
#ifndef QUICKJS_LIBC_PAL_H
#define QUICKJS_LIBC_PAL_H

#include <stdint.h>

#include "quickjs-pal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* process_wait() options/status, exposed to JS as os.WNOHANG.
   POSIX uses the real <sys/wait.h> values. Windows has no equivalent, so the
   backend defines its own and always reports a normal exit (never signaled). */
#if !defined(_WIN32)
#include <sys/wait.h>
#define JS_LIBC_WNOHANG             WNOHANG
#define JS_LIBC_WIFEXITED(status)   WIFEXITED(status)
#define JS_LIBC_WEXITSTATUS(status) WEXITSTATUS(status)
#define JS_LIBC_WIFSIGNALED(status) WIFSIGNALED(status)
#define JS_LIBC_WTERMSIG(status)    WTERMSIG(status)
#else
#define JS_LIBC_WNOHANG             1
#define JS_LIBC_WIFEXITED(status)   1
#define JS_LIBC_WEXITSTATUS(status) (((status) >> 8) & 0xff)
#define JS_LIBC_WIFSIGNALED(status) 0
#define JS_LIBC_WTERMSIG(status)    0
#endif

/* libc process primitives, called by quickjs-libc.c. The first argument is
   the engine's JSPal * (from JS_GetRuntimePal()); the default impls ignore it. */

/* Spawn a child. file is the program (NULL = argv[0]); argv/envp are
   NULL-terminated (envp NULL = inherit); cwd NULL = inherit; std_fds[3] are
   the child's stdin/stdout/stderr; use_path selects PATH search; uid/gid of
   (uint32_t)-1 mean "don't change" (POSIX only). Returns 0 and sets *out_pid,
   or -errno. */
int jspal_process_spawn(JSPal *opaque, const char *file,
                              char *const argv[], char *const envp[],
                              const char *cwd, const int std_fds[3],
                              int use_path, uint32_t uid, uint32_t gid,
                              int *out_pid);

/* Wait for pid (like waitpid()). options is 0 or JS_LIBC_WNOHANG. Returns pid
   and fills *pstatus if it exited, 0 if still running with WNOHANG, or -errno. */
int jspal_process_wait(JSPal *opaque, int pid, int options, int *pstatus);

/* Send signal sig to pid (like kill()). Returns 0 or -errno. The Windows
   backend cannot send signals, so it always terminates the process. */
int jspal_process_kill(JSPal *opaque, int pid, int sig);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* QUICKJS_LIBC_PAL_H */
