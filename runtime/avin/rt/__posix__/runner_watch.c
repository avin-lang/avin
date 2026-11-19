/* A program `avin run` or `avin test` started ends when its runner does.
 *
 * The runner names itself in `AVIN_RUNNER_PID` for the programs it starts. A program started with the
 * variable by the very process it names watches that process from a thread of its own, and once the
 * process is gone -- whatever ended it, `SIGKILL` included, which the runner cannot answer -- the
 * program ends: with everything in its process group when it leads one, as a program a runner starts
 * away from a terminal does, and alone otherwise. What is in the group is asked with `SIGTERM` first,
 * and whatever is left after a short grace is killed.
 *
 * The variable is taken out of the environment as the program starts, so a program it starts in turn
 * does not watch a process that is not its parent. A program started any other way -- by a shell, by
 * any other program, or with the variable naming some other process -- watches nothing.
 *
 * On macOS the thread waits on a kqueue for the runner's exit; elsewhere it asks every tenth of a
 * second whether the program's parent is still the runner. The thread runs no Avin code and has every
 * signal blocked, so signals reach the program's own threads as they did before it.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/event.h>
#endif

/* How long what is left of the group has to end once asked, in milliseconds. */
#define AVIN_RUNNER_GRACE_MS 200

static pid_t avin_runner = 0;

static void avin_runner_nap(long ms) {
    struct timespec t;

    t.tv_sec = ms / 1000;
    t.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&t, &t) != 0 && errno == EINTR) {
    }
}

/* The runner is gone: this program ends, and the group it leads with it. The program's own answer to
 * `SIGTERM` is set aside first, so that the whole group is asked and then killed by this thread
 * rather than the program ending halfway through and leaving the rest. */
static void avin_runner_gone(void) {
    pid_t me = getpid();
    pid_t target = getpgrp() == me ? 0 : me;

    signal(SIGTERM, SIG_IGN);
    kill(target, SIGTERM);
    avin_runner_nap(AVIN_RUNNER_GRACE_MS);
    kill(target, SIGKILL);
    _exit(128 + SIGKILL);
}

static void *avin_runner_loop(void *unused) {
    (void)unused;

#if defined(__APPLE__)
    {
        int q = kqueue();

        if (q >= 0) {
            struct kevent change;
            struct kevent got;

            EV_SET(&change, avin_runner, EVFILT_PROC, EV_ADD | EV_ONESHOT, NOTE_EXIT, 0, 0);
            if (kevent(q, &change, 1, 0, 0, 0) != 0) {
                /* Refused because the runner has already gone. */
                if (errno == ESRCH) avin_runner_gone();
            } else {
                /* The runner may have ended between the check at start and the registration. */
                if (getppid() != avin_runner) avin_runner_gone();
                for (;;) {
                    int n = kevent(q, 0, 0, &got, 1, 0);

                    if (n > 0) avin_runner_gone();
                    if (n < 0 && errno != EINTR) break;
                }
            }
            close(q);
        }
    }
#endif

    for (;;) {
        if (getppid() != avin_runner) avin_runner_gone();
        avin_runner_nap(100);
    }
    return 0;
}

/* Starts the watch when the program was started by the runner the variable names, and takes the
 * variable out of the environment either way. Called once, as the program starts, before any other
 * thread is. */
void avin_rt_runner_watch_install(void) {
    const char *named = getenv("AVIN_RUNNER_PID");
    char *end = 0;
    long pid;
    sigset_t all;
    sigset_t before;
    pthread_t thread;
    int started;

    int valid;

    if (named == 0) return;
    pid = strtol(named, &end, 10);
    valid = end != named && *end == 0 && pid > 1;
    unsetenv("AVIN_RUNNER_PID");
    if (!valid) return;
    if ((pid_t)pid != getppid()) {
        /* A runner that ended before this program got this far has left it to the init process. */
        if (getppid() == 1 && kill((pid_t)pid, 0) != 0 && errno == ESRCH) avin_runner_gone();
        return;
    }
    avin_runner = (pid_t)pid;

    sigfillset(&all);
    pthread_sigmask(SIG_SETMASK, &all, &before);
    started = pthread_create(&thread, 0, avin_runner_loop, 0);
    pthread_sigmask(SIG_SETMASK, &before, 0);
    if (started == 0) pthread_detach(thread);
}
