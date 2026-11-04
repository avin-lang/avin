/* What a program says when a thread runs out of stack.
 *
 * Recursion without a base case runs a thread's stack into its guard page, and the fault, with no
 * handler, ends the process with a segfault and no message, and with whatever the program had
 * printed still in standard output's buffer. Every other runtime failure is a trap that says why,
 * so this one is made into a trap as well: `avin: trap: stack overflow`.
 *
 * It is C because a handler runs between two instructions of whatever the thread was doing, on a
 * stack of its own, and may call only what is safe there. Each thread that runs Avin code calls
 * `avin_rt_stack_guard_install` before it starts and `avin_rt_stack_guard_remove` when it ends: the
 * first records where the thread's stack is and gives it an alternate signal stack, which is what
 * lets the handler run when the ordinary stack is the thing that is full.
 *
 * The handler reports only a fault inside the guard region of the faulting thread's own stack. Any
 * other fault is a real crash and is left to crash: the handler puts the default action back and
 * returns, so the instruction that faulted runs again and ends the process as it would have.
 *
 * Standard output is flushed from the handler with `fflush`, which is not async-signal-safe: a
 * fault inside the C library's own output routines would find the stream half updated. The report
 * has been written by then, an alarm ends the process if the flush never returns, and the worst
 * case is that output buffered at that moment is lost.
 */

#define _GNU_SOURCE

#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The alternate stack each thread's handler runs on. The handler writes one line and flushes. */
#define AVIN_ALT_STACK_SIZE (64 * 1024)

/* How far below the lowest usable stack address, and how far above it, a fault still counts as the
 * guard's. A frame larger than a page can step over the guard page and fault further down, so the
 * window is wider than the guard itself; it stays small enough that a wild pointer is unlikely to
 * land in it. */
#define AVIN_GUARD_BELOW (256 * 1024)
#define AVIN_GUARD_ABOVE (64 * 1024)

/* How long a stuck flush is given before the alarm ends the process. */
#define AVIN_FLUSH_SECONDS 2

static _Thread_local char *avin_stack_low = 0;
static _Thread_local void *avin_alt_stack = 0;
static atomic_flag avin_handler_set = ATOMIC_FLAG_INIT;

static void avin_say(const char *s) {
    ssize_t ignored = write(2, s, strlen(s));

    (void)ignored;
}

static void avin_fault(int sig, siginfo_t *info, void *context) {
    char *at = info ? (char *)info->si_addr : 0;
    int overflowed = avin_stack_low != 0 && at >= avin_stack_low - AVIN_GUARD_BELOW &&
                     at < avin_stack_low + AVIN_GUARD_ABOVE;

    (void)context;

    if (!overflowed) {
        signal(sig, SIG_DFL);
        return;
    }

    avin_say("avin: trap: stack overflow\n");
    alarm(AVIN_FLUSH_SECONDS);
    fflush(NULL);
    __builtin_trap();
}

/* The lowest address of the calling thread's stack. macOS answers with the high address and the
 * size; Linux fills an attribute object with the low address and the size. */
static char *avin_stack_bottom(void) {
#if defined(__APPLE__)
    pthread_t self = pthread_self();

    return (char *)pthread_get_stackaddr_np(self) - pthread_get_stacksize_np(self);
#else
    pthread_attr_t attr;
    void *base = 0;
    size_t size = 0;
    char *low = 0;

    if (pthread_getattr_np(pthread_self(), &attr) == 0) {
        if (pthread_attr_getstack(&attr, &base, &size) == 0) low = (char *)base;
        pthread_attr_destroy(&attr);
    }

    return low;
#endif
}

/* Makes a stack overflow in the calling thread a trap. Called once by each thread before it runs
 * Avin code; a thread whose stack cannot be found, or that cannot have an alternate stack, keeps
 * the default behaviour. */
void avin_rt_stack_guard_install(void) {
    char *low = avin_stack_bottom();
    void *memory = malloc(AVIN_ALT_STACK_SIZE);
    stack_t alt;

    if (low == 0 || memory == 0) {
        free(memory);
        return;
    }

    alt.ss_sp = memory;
    alt.ss_size = AVIN_ALT_STACK_SIZE;
    alt.ss_flags = 0;

    if (sigaltstack(&alt, 0) != 0) {
        free(memory);
        return;
    }

    avin_alt_stack = memory;
    avin_stack_low = low;

    if (!atomic_flag_test_and_set(&avin_handler_set)) {
        struct sigaction sa;

        memset(&sa, 0, sizeof sa);
        sa.sa_sigaction = avin_fault;
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigemptyset(&sa.sa_mask);

        /* Which of the two a stack overflow raises is the operating system's choice: Linux raises
         * SIGSEGV and macOS raises SIGBUS for some of them. */
        sigaction(SIGSEGV, &sa, 0);
        sigaction(SIGBUS, &sa, 0);
    }
}

/* Gives back what `avin_rt_stack_guard_install` made for the calling thread, which calls it last. */
void avin_rt_stack_guard_remove(void) {
    stack_t off;

    if (avin_alt_stack == 0) return;

    memset(&off, 0, sizeof off);
    off.ss_flags = SS_DISABLE;
    off.ss_size = AVIN_ALT_STACK_SIZE;
    sigaltstack(&off, 0);

    avin_stack_low = 0;
    free(avin_alt_stack);
    avin_alt_stack = 0;
}
