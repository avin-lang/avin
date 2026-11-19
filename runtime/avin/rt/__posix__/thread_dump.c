/* The thread dump the quit signal asks for.
 *
 * Ctrl-\ at a terminal, or any `kill -QUIT`, would end the program with a core file. An Avin program
 * instead writes a list of its threads to standard error, what each is doing and how long it has
 * waited, and goes on (`diagnostics.sysl`). A handler may call only what is safe inside one, so the
 * handler written here only writes a byte to a pipe; a thread of its own, started with the program,
 * reads the pipe and writes the dump. That thread runs no Avin code and has every signal blocked, so
 * signals reach the program's own threads as they did before it.
 *
 * A program that watches the quit signal itself (`avin.signal.watch`) hears of it instead, and gets
 * the dump back when its last watch is gone. A program started with the signal ignored keeps it
 * ignored.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

/* Written by the runtime: the dump itself. */
extern void avin_rt_thread_dump(void);

static int avin_dump_pipe[2] = {-1, -1};

static void avin_quit(int sig) {
    int saved = errno;
    char byte = 1;
    ssize_t ignored = write(avin_dump_pipe[1], &byte, 1);

    (void)sig;
    (void)ignored;
    errno = saved;
}

static void *avin_dump_loop(void *unused) {
    char bytes[16];

    (void)unused;

    for (;;) {
        ssize_t n = read(avin_dump_pipe[0], bytes, sizeof bytes);

        if (n > 0) {
            avin_rt_thread_dump();
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            return 0;
        }
    }
}

static void avin_close_on_exec(int fd) {
    int flags = fcntl(fd, F_GETFD);

    if (flags >= 0) fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

/* Starts the dump's thread and makes the quit signal ask it for a dump. Called once, as the program
 * starts; where anything it needs is refused, the signal keeps its default action. */
void avin_rt_thread_dump_install(void) {
    struct sigaction was;
    struct sigaction sa;
    sigset_t all;
    sigset_t before;
    pthread_t thread;
    int started;

    if (sigaction(SIGQUIT, 0, &was) != 0 || was.sa_handler == SIG_IGN) return;
    if (pipe(avin_dump_pipe) != 0) return;

    avin_close_on_exec(avin_dump_pipe[0]);
    avin_close_on_exec(avin_dump_pipe[1]);
    fcntl(avin_dump_pipe[1], F_SETFL, fcntl(avin_dump_pipe[1], F_GETFL) | O_NONBLOCK);

    /* The thread starts with every signal blocked, which it inherits from this one for a moment. */
    sigfillset(&all);
    pthread_sigmask(SIG_SETMASK, &all, &before);
    started = pthread_create(&thread, 0, avin_dump_loop, 0);
    pthread_sigmask(SIG_SETMASK, &before, 0);

    if (started != 0) {
        close(avin_dump_pipe[0]);
        close(avin_dump_pipe[1]);
        avin_dump_pipe[0] = -1;
        avin_dump_pipe[1] = -1;
        return;
    }

    pthread_detach(thread);

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = avin_quit;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGQUIT, &sa, 0);
}
