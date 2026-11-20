/* Standard input, output and error as bytes, for `avin.io.stdin()`, `stdout()` and `stderr()`.
 *
 * What crosses to Sysl is a count, or `-errno` on failure.
 *
 * A write to standard output goes behind everything `print` has put in stdio's buffer and is never
 * split by a `print` from another thread: the stream's own lock (`flockfile`) is held while the
 * buffer is flushed and the bytes are written to the descriptor, and a `print` writes through that
 * same stream. A write to standard error flushes standard output first, as `eprint` does, and writes
 * to the descriptor at once, as `eprint` does.
 *
 * A write to a pipe whose reader has gone answers `EPIPE` rather than ending the program: SIGPIPE is
 * blocked on the writing thread for the length of the write, and the SIGPIPE the write raised, if it
 * raised one that was not already pending, is taken back before it is unblocked. `print` itself is
 * left as it was: a program that only prints still ends quietly when its reader goes.
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

/* Opens `/dev/null` on each of descriptors 0, 1 and 2 the program was started without, before the
 * runtime opens anything of its own. Otherwise the runtime's first descriptor, a pipe or the poller's,
 * would be numbered 0, and standard input would read the runtime's own pipe; or 1, and a `print`
 * would write into it. A closed standard input reads as empty, and a closed standard output or error
 * takes what is written and keeps none of it. Each is opened in order, so `open` answers the lowest
 * free descriptor, which is the one missing. */
void avin_rt_open_standard_descriptors(void) {
    for (int fd = 0; fd <= 2; fd++) {
        if (fcntl(fd, F_GETFD) == -1 && errno == EBADF) {
            int got = open("/dev/null", fd == 0 ? O_RDONLY : O_WRONLY);
            if (got >= 0 && got != fd) {
                (void)dup2(got, fd);
                (void)close(got);
            }
        }
    }
}

/* Up to `n` bytes of standard input into `p`: how many, 0 at its end, or -errno. A signal that
 * interrupts the read is not a failure: the read is made again. */
int64_t avin_rt_stdin_read(unsigned char *p, int64_t n) {
    for (;;) {
        ssize_t k = read(0, p, (size_t)n);
        if (k >= 0) return (int64_t)k;
        if (errno != EINTR) return -(int64_t)errno;
    }
}

/* Writes every byte to `fd`, waiting for room on a descriptor something else made non-blocking. */
static int write_all(int fd, const unsigned char *p, int64_t n) {
    int64_t at = 0;
    while (at < n) {
        ssize_t k = write(fd, p + at, (size_t)(n - at));
        if (k > 0) {
            at += k;
        } else if (k < 0 && errno == EINTR) {
            continue;
        } else if (k < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct pollfd w = { fd, POLLOUT, 0 };
            (void)poll(&w, 1, -1);
        } else {
            return k < 0 ? errno : EIO;
        }
    }
    return 0;
}

/* Runs the write with SIGPIPE held off this thread, and takes back a SIGPIPE it raised. */
static int write_guarded(int fd, const unsigned char *p, int64_t n) {
    sigset_t pipe_only, before, pending;
    sigemptyset(&pipe_only);
    sigaddset(&pipe_only, SIGPIPE);
    sigpending(&pending);
    int was_pending = sigismember(&pending, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &pipe_only, &before);
    int failure = 0;
    errno = 0;
    /* Standard output's own failure fails a write to it; before a write to standard error it is
     * standard output's to report, at its next write. */
    if (fflush(stdout) != 0 && fd == 1) failure = errno ? errno : EIO;
    if (failure == 0) failure = write_all(fd, p, n);
    if (!was_pending) {
        sigpending(&pending);
        if (sigismember(&pending, SIGPIPE)) {
            int taken;
            sigwait(&pipe_only, &taken);
        }
    }
    pthread_sigmask(SIG_SETMASK, &before, NULL);
    return failure;
}

/* Writes `n` bytes to standard output (`fd` 1) or standard error (`fd` 2): 0, or -errno. */
int64_t avin_rt_stdio_write(int32_t fd, const unsigned char *p, int64_t n) {
    int failure;
    if (fd == 1) {
        flockfile(stdout);
        failure = write_guarded(1, p, n);
        funlockfile(stdout);
    } else {
        failure = write_guarded(fd, p, n);
    }
    return failure == 0 ? 0 : -(int64_t)failure;
}
