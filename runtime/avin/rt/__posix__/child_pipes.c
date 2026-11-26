/* The program's ends of the pipes to a child program's standard streams, for `avin.process`.
 *
 * What crosses to Sysl is a count, or `-errno` on failure. Each end is non-blocking, so a read or a
 * write here never waits: one that would answers `-EAGAIN`, and the runtime waits for the descriptor
 * (`pipes.sysl`), parking a lightweight thread on its poller.
 *
 * A write to a pipe whose reader has gone answers `EPIPE` rather than ending the program. On macOS
 * the system raises the SIGPIPE such a write would raise on the whole process, where any thread not
 * blocking it would take it, so each end is marked never to raise it (`F_SETNOSIGPIPE`). Elsewhere
 * the signal goes to the writing thread: SIGPIPE is blocked on it for the length of the write, and
 * the SIGPIPE the write raised, if it raised one that was not already pending, is taken back before
 * it is unblocked.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <unistd.h>

/* Makes `fd` non-blocking, and on macOS never to raise SIGPIPE: 0, or -errno. */
int32_t avin_rt_pipe_non_blocking(int32_t fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) return -(int32_t)errno;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return -(int32_t)errno;
#ifdef F_SETNOSIGPIPE
    if (fcntl(fd, F_SETNOSIGPIPE, 1) == -1) return -(int32_t)errno;
#endif
    return 0;
}

/* Up to `n` bytes from `fd` into `p`: how many, 0 at its end, or -errno, `-EAGAIN` where nothing has
 * arrived yet. A signal that interrupts the read is not a failure: the read is made again. */
int64_t avin_rt_pipe_read(int32_t fd, unsigned char *p, int64_t n) {
    for (;;) {
        ssize_t k = read(fd, p, (size_t)n);
        if (k >= 0) return (int64_t)k;
        if (errno != EINTR) return -(int64_t)errno;
    }
}

/* Up to `n` bytes of `p` into `fd`, as many as there is room for: how many, or -errno, `-EAGAIN` where
 * there is no room for one, and `-EPIPE` where nothing reads the pipe any more. */
int64_t avin_rt_pipe_write(int32_t fd, const unsigned char *p, int64_t n) {
    sigset_t pipe_only, before, pending;
    sigemptyset(&pipe_only);
    sigaddset(&pipe_only, SIGPIPE);
    sigpending(&pending);
    int was_pending = sigismember(&pending, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &pipe_only, &before);
    int64_t answer;
    for (;;) {
        ssize_t k = write(fd, p, (size_t)n);
        if (k >= 0) {
            answer = (int64_t)k;
            break;
        }
        if (errno != EINTR) {
            answer = -(int64_t)errno;
            break;
        }
    }
    if (!was_pending) {
        sigpending(&pending);
        if (sigismember(&pending, SIGPIPE)) {
            int taken;
            sigwait(&pipe_only, &taken);
        }
    }
    pthread_sigmask(SIG_SETMASK, &before, NULL);
    return answer;
}
