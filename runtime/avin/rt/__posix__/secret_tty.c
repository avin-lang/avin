/* Reading a line from the controlling terminal with what is typed not shown.
 *
 * `avin.io.readSecret` opens `/dev/tty`, turns the terminal's echo off, writes a prompt, reads a
 * line and puts the terminal back as it was found. This file is the part that needs `struct termios`
 * and signal handlers, which Sysl does not spell: what crosses to Sysl is a descriptor, a count and
 * an answer that is zero or more on success and `-errno` on failure.
 *
 * Only one read runs at a time (the runtime holds the lock over its input meanwhile), so the saved
 * terminal and the signal bookkeeping are single statics.
 *
 * The terminal is put back on every way out. The ordinary ones call `avin_rt_secret_restore`. A
 * signal that ends the program while echo is off would leave the user's shell showing nothing as it
 * is typed, so the signals that would end it (interrupt, hangup, terminate, quit) are caught for the
 * length of the read: the handler puts the terminal back, gives the signal its previous action again
 * and sends it to the process once more, so the program dies or handles it as it would have, with
 * echo on. A stop from the terminal (Ctrl-Z) puts the terminal back, stops the program, and when it
 * is continued hides the terminal again and reads on. A program that survives the signal sees its read fail as interrupted. A normal exit while
 * echo is off, which only a program that exits from another thread can reach, puts it back as well.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define SECRET_SIGNALS 4

static const int guarded[SECRET_SIGNALS] = { SIGINT, SIGHUP, SIGTERM, SIGQUIT };
static struct sigaction before[SECRET_SIGNALS];
static struct termios saved;

/* The descriptor whose mode is changed and not yet put back, or -1. */
static volatile sig_atomic_t hidden_fd = -1;
/* The signal that arrived during the read, or 0. */
static volatile sig_atomic_t arrived = 0;
static int exit_hook_set = 0;

/* What the terminal is set to while the secret is read, and what the terminal-stop signal did before. */
static struct termios hidden_mode;
static struct sigaction before_stop;
/* Set when the program was stopped and continued during the read. */
static volatile sig_atomic_t resumed = 0;

static void on_stop(int sig);

static void give_back_signals(void) {
    for (int i = 0; i < SECRET_SIGNALS; i++) sigaction(guarded[i], &before[i], NULL);
    sigaction(SIGTSTP, &before_stop, NULL);
}

/* A stop from the terminal (Ctrl-Z): puts the terminal back, stops the program as the signal would
 * have, and once it is continued hides the terminal again and tells the read to write its prompt
 * again. The read's own descriptor stays open throughout. */
static void on_stop(int sig) {
    int fd = hidden_fd;
    if (fd >= 0) tcsetattr(fd, TCSANOW, &saved);
    struct sigaction dfl, mine;
    memset(&dfl, 0, sizeof dfl);
    dfl.sa_handler = SIG_DFL;
    sigemptyset(&dfl.sa_mask);
    sigaction(sig, &dfl, &mine);
    sigset_t s;
    sigemptyset(&s);
    sigaddset(&s, sig);
    sigprocmask(SIG_UNBLOCK, &s, NULL);
    kill(getpid(), sig);
    /* Continued. */
    sigprocmask(SIG_BLOCK, &s, NULL);
    sigaction(sig, &mine, NULL);
    if (hidden_fd >= 0) tcsetattr(hidden_fd, TCSADRAIN, &hidden_mode);
    resumed = 1;
}

static void on_signal(int sig) {
    int fd = hidden_fd;
    if (fd >= 0) {
        tcsetattr(fd, TCSANOW, &saved);
        hidden_fd = -1;
    }
    arrived = sig;
    give_back_signals();
    kill(getpid(), sig);
}

static void put_back_at_exit(void) {
    int fd = hidden_fd;
    if (fd >= 0) {
        tcsetattr(fd, TCSANOW, &saved);
        hidden_fd = -1;
    }
}

/* The controlling terminal, opened for reading and writing; -errno where the process has none. */
int avin_rt_secret_open(void) {
    int fd = open("/dev/tty", O_RDWR | O_NOCTTY | O_CLOEXEC);
    return fd < 0 ? -errno : fd;
}

void avin_rt_secret_close(int fd) {
    close(fd);
}

/* Turns echo off on the terminal at `fd`, keeping line editing, and starts catching the signals that
 * would end the program with echo off. Typed-ahead input is discarded, so nothing typed before the
 * prompt is taken for the secret. Zero, or -errno. */
int avin_rt_secret_hide(int fd) {
    struct termios t;
    if (tcgetattr(fd, &saved) != 0) return -errno;
    t = saved;
    t.c_lflag &= ~(tcflag_t) (ECHO | ECHONL);
    t.c_lflag |= ICANON;
    if (!exit_hook_set) {
        exit_hook_set = 1;
        atexit(put_back_at_exit);
    }
    arrived = 0;
    struct sigaction act;
    memset(&act, 0, sizeof act);
    act.sa_handler = on_signal;
    sigemptyset(&act.sa_mask);
    for (int i = 0; i < SECRET_SIGNALS; i++) sigaction(guarded[i], &act, &before[i]);
    struct sigaction stop;
    memset(&stop, 0, sizeof stop);
    stop.sa_handler = on_stop;
    sigemptyset(&stop.sa_mask);
    sigaction(SIGTSTP, &stop, &before_stop);
    hidden_mode = t;
    resumed = 0;
    hidden_fd = fd;
    if (tcsetattr(fd, TCSAFLUSH, &t) != 0) {
        int e = errno;
        hidden_fd = -1;
        give_back_signals();
        return -e;
    }
    return 0;
}

/* Puts the terminal back exactly as `avin_rt_secret_hide` found it, and the signals' actions. Doing it
 * again, or after a signal already did, changes nothing. */
void avin_rt_secret_restore(int fd) {
    if (hidden_fd >= 0) {
        tcsetattr(fd, TCSANOW, &saved);
        hidden_fd = -1;
    }
    give_back_signals();
}

/* Writes all of `n` bytes at `p` to `fd`; zero, or -errno. */
int avin_rt_secret_write(int fd, const unsigned char *p, long n) {
    while (n > 0) {
        ssize_t w = write(fd, p, (size_t) n);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -errno;
        }
        p += w;
        n -= w;
    }
    return 0;
}

/* Reads up to `n` bytes into `p`: how many, zero at the end of input, or -errno. A signal that
 * arrived during the read, and that the program survived, answers -EINTR. A stop and continue answers
 * -4097: the terminal is hidden again and the caller writes its prompt again and reads on. */
long avin_rt_secret_read(int fd, unsigned char *p, long n) {
    for (;;) {
        ssize_t r = read(fd, p, (size_t) n);
        if (r >= 0) return (long) r;
        if (errno == EINTR) {
            if (arrived != 0) return -EINTR;
            if (resumed) {
                resumed = 0;
                return -4097;
            }
            continue;
        }
        return -errno;
    }
}

/* Whether a signal arrived since the terminal was hidden. */
int avin_rt_secret_interrupted(void) {
    return arrived != 0;
}
