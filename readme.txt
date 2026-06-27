The main documentation is in doc/quickjs.pdf or doc/quickjs.html.


This fork
=========

This is a fork of Fabrice Bellard's QuickJS. The goal is to let the engine
run on any platform, even one without a normal C library or operating system.
It adds the following changes.

1. Platform Abstraction Layer (PAL)

   The engine no longer calls the operating system directly. Every OS
   operation -- memory (malloc/free), time, threads, mutexes, condition
   variables, atomics, and debug printing -- now goes through a small set of
   functions called the PAL. You give your own PAL to JS_NewRuntime2(). If you
   give none, a default PAL is used (pthreads on Unix, Win32 on Windows).

   Why: the core engine (quickjs.c) becomes independent from the host. To port
   QuickJS to a new system you only write one small PAL; you do not change the
   engine code.

   New files: quickjs-pal.h/.c (the interface and the default PAL),
   quickjs-pal-overrides.h (maps the engine's pthread and atomic calls to the
   PAL), quickjs-libc-pal.h/.c (process spawn/wait/kill for the os module), and
   quickjs-libc-win32-compat.h (small Windows helpers).

2. Fatal-error API instead of abort()

   Where the engine used to call abort() (which kills the whole process), it
   now raises a special JavaScript error that a script cannot catch. The error
   carries a code and a message that the host program can read.

   Why: the host stays in control. Instead of the process dying, it gets a
   clear error that it can report or handle.

3. Visual Studio build

   A Visual Studio solution (quickjs-pal.sln) builds the engine and the tools
   with clang-cl, for both 64-bit (x64) and 32-bit (Win32).

   Why: build on Windows with Visual Studio, not only with make and MinGW.

4. Memory limit in run-test262

   The test runner now limits how much memory one test may use.

   Why: a few tests build a very large string on purpose to force an
   out-of-memory error. Without a limit this is slow on 64-bit systems.
