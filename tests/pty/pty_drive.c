/* Runs a program with a pseudo-terminal as its controlling terminal (or with no terminal at all) and
 * plays a script against it, so a test can type at a program and look at what the terminal shows.
 *
 *     pty_drive [--notty | --job] ACTION... -- PROGRAM [ARGUMENT...]
 *
 * With --job the program runs as a shell's job would, in its own foreground process group, so that
 * a typed Ctrl-Z (`send:\x1a`) stops it; the driver then prints `[stopped]`.
 *
 * Everything the program writes to the terminal is copied to standard output as it arrives, followed
 * by one marker line for each `report` and a last one for how the program ended. The actions run in
 * order:
 *
 *     expect:TEXT   wait until TEXT has come from the program since the last `expect`
 *     send:TEXT     type TEXT (\n \r \t \\ and \xHH are understood)
 *     report        write `[echo=E icanon=C isig=S]`, the terminal's state at that moment
 *     sleep:MS      wait that many milliseconds, copying what arrives
 *     kill:N        send signal N to the program
 *     cont          with --job, continue the program after it was stopped
 *     eof           type the terminal's end-of-file character, or with --notty close the program's input
 *
 * With --notty the program starts in a session of its own with no controlling terminal, its input
 * and output pipes. Where a wait runs 30 seconds without the program doing what it waits for, the
 * program is killed and the last line says `[timeout]`.
 *
 * The last line is `[exit N]`, or `[signal N]` for a program a signal ended.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

#define SEEN_MAX (1 << 20)
#define LIMIT_MS 30000

static char seen[SEEN_MAX];
static size_t seen_len = 0;
static size_t mark = 0;
static int read_fd = -1;
static int write_fd = -1;
static int master = -1;
static int closed_output = 0;
static pid_t child = -1;

static long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

static void emit(const char *p, size_t n) {
    fwrite(p, 1, n, stdout);
    fflush(stdout);
}

static void note(const char *text) {
    emit(text, strlen(text));
}

/* Copies what the program has written, waiting up to `ms` for some. */
static void pump(int ms) {
    if (closed_output) return;
    struct pollfd p = { read_fd, POLLIN, 0 };
    if (poll(&p, 1, ms) <= 0) return;
    char buf[4096];
    ssize_t n = read(read_fd, buf, sizeof buf);
    if (n <= 0) {
        closed_output = 1;
        return;
    }
    emit(buf, (size_t) n);
    if (seen_len + (size_t) n < SEEN_MAX) {
        memcpy(seen + seen_len, buf, (size_t) n);
        seen_len += (size_t) n;
    }
}

static void give_up(void) {
    note("\n[timeout]\n");
    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    exit(3);
}

static void expect(const char *text) {
    long end = now_ms() + LIMIT_MS;
    size_t m = strlen(text);
    for (;;) {
        if (seen_len >= mark + m) {
            for (size_t i = mark; i + m <= seen_len; i++) {
                if (memcmp(seen + i, text, m) == 0) {
                    mark = i + m;
                    return;
                }
            }
        }
        if (closed_output || now_ms() > end) give_up();
        pump(200);
    }
}

static int hex(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    return c - 'A' + 10;
}

static void send_text(const char *text) {
    char out[4096];
    size_t n = 0;
    for (const char *p = text; *p && n < sizeof out; p++) {
        if (*p != '\\') {
            out[n++] = *p;
            continue;
        }
        p++;
        if (*p == 'n') out[n++] = '\n';
        else if (*p == 'r') out[n++] = '\r';
        else if (*p == 't') out[n++] = '\t';
        else if (*p == 'x') {
            out[n++] = (char) (hex(p[1]) * 16 + hex(p[2]));
            p += 2;
        } else out[n++] = *p;
    }
    size_t at = 0;
    while (at < n) {
        ssize_t w = write(write_fd, out + at, n - at);
        if (w < 0) {
            if (errno == EINTR) continue;
            return;
        }
        at += (size_t) w;
    }
}

static pid_t member = -1;

static void continue_member(int sig) {
    (void) sig;
    kill(-member, SIGCONT);
}

/* Runs in the child of the pseudo-terminal, which leads its session: starts the program in a process
 * group of its own that is the terminal's foreground group, as a shell does a job, and stays as that
 * group's parent. A group whose parent is outside it is not orphaned, so Ctrl-Z stops it. Says
 * `[stopped]` each time the program is stopped, continues it on SIGUSR1, and ends as it did. */
static void run_as_job(char **program) {
    member = fork();
    if (member == 0) {
        setpgid(0, 0);
        signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(0, getpid());
        signal(SIGTTOU, SIG_DFL);
        execv(program[0], program);
        _exit(127);
    }
    setpgid(member, member);
    signal(SIGTTOU, SIG_IGN);
    tcsetpgrp(0, member);
    signal(SIGUSR1, continue_member);
    for (;;) {
        int st = 0;
        pid_t r = waitpid(member, &st, WUNTRACED);
        if (r < 0) {
            if (errno == EINTR) continue;
            _exit(126);
        }
        if (WIFSTOPPED(st)) {
            (void) !write(1, "\n[stopped]\n", 11);
            continue;
        }
        if (WIFEXITED(st)) _exit(WEXITSTATUS(st));
        signal(WTERMSIG(st), SIG_DFL);
        kill(getpid(), WTERMSIG(st));
        _exit(128 + WTERMSIG(st));
    }
}

static void report(void) {
    char line[100];
    struct termios t;
    if (master < 0) {
        note("\n[report: no terminal]\n");
        return;
    }
    if (tcgetattr(master, &t) != 0) {
        snprintf(line, sizeof line, "\n[report failed: errno %d]\n", errno);
        note(line);
        return;
    }
    snprintf(line, sizeof line, "\n[echo=%d icanon=%d isig=%d]\n", (t.c_lflag & ECHO) != 0,
             (t.c_lflag & ICANON) != 0, (t.c_lflag & ISIG) != 0);
    note(line);
}

int main(int argc, char **argv) {
    int notty = 0;
    int job = 0;
    int i = 1;
    /* A script may type at a program that has already finished: the write then fails with EPIPE,
     * which is ignored, rather than killing the driver with SIGPIPE. */
    signal(SIGPIPE, SIG_IGN);
    if (i < argc && strcmp(argv[i], "--notty") == 0) {
        notty = 1;
        i++;
    } else if (i < argc && strcmp(argv[i], "--job") == 0) {
        job = 1;
        i++;
    }
    int first = i;
    while (i < argc && strcmp(argv[i], "--") != 0) i++;
    if (i >= argc - 1) {
        fprintf(stderr, "usage: pty_drive [--notty] ACTION... -- PROGRAM [ARGUMENT...]\n");
        return 2;
    }
    int actions_end = i;
    char **program = argv + i + 1;

    if (notty) {
        int in[2], out[2];
        if (pipe(in) != 0 || pipe(out) != 0) return 2;
        child = fork();
        if (child == 0) {
            signal(SIGPIPE, SIG_DFL);
            setsid();
            dup2(in[0], 0);
            dup2(out[1], 1);
            dup2(out[1], 2);
            close(in[0]);
            close(in[1]);
            close(out[0]);
            close(out[1]);
            execv(program[0], program);
            _exit(127);
        }
        close(in[0]);
        close(out[1]);
        write_fd = in[1];
        read_fd = out[0];
    } else {
        child = forkpty(&master, NULL, NULL, NULL);
        if (child == 0) {
            if (job) run_as_job(program);
            execv(program[0], program);
            _exit(127);
        }
        read_fd = write_fd = master;
    }
    if (child < 0) return 2;

    for (i = first; i < actions_end; i++) {
        const char *a = argv[i];
        if (strncmp(a, "expect:", 7) == 0) expect(a + 7);
        else if (strncmp(a, "send:", 5) == 0) send_text(a + 5);
        else if (strcmp(a, "report") == 0) report();
        else if (strncmp(a, "kill:", 5) == 0) kill(child, atoi(a + 5));
        else if (strcmp(a, "cont") == 0) kill(child, SIGUSR1);
        else if (strncmp(a, "sleep:", 6) == 0) {
            long end = now_ms() + atol(a + 6);
            while (now_ms() < end) pump(20);
        } else if (strcmp(a, "eof") == 0) {
            if (notty) {
                close(write_fd);
                write_fd = -1;
            } else send_text("\\x04");
        } else {
            fprintf(stderr, "unknown action %s\n", a);
            kill(child, SIGKILL);
            return 2;
        }
    }

    long end = now_ms() + LIMIT_MS;
    int state = 0;
    for (;;) {
        pump(50);
        pid_t r = waitpid(child, &state, WNOHANG);
        if (r == child) break;
        if (now_ms() > end) give_up();
    }
    for (int k = 0; k < 20 && !closed_output; k++) pump(20);
    char line[64];
    if (WIFEXITED(state)) snprintf(line, sizeof line, "\n[exit %d]\n", WEXITSTATUS(state));
    else snprintf(line, sizeof line, "\n[signal %d]\n", WTERMSIG(state));
    note(line);
    if (master >= 0) {
        report();
    }
    return 0;
}
