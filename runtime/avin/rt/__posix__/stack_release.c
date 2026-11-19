/* Giving a lightweight thread's untouched stack pages back to the system.
 *
 * A thread's stack is reserved large and committed as it is touched, and a page once touched stays
 * committed: a thread that recursed deep once and then parked would hold every page it ever reached
 * for as long as it stays parked, and an ended thread's stack kept for the next would hold them too.
 * The runtime gives back the pages below where the thread stands, which hold nothing it can reach.
 *
 * On Linux, `madvise(MADV_DONTNEED)` drops the pages, and the next touch finds zeroed ones. On macOS
 * the same advice leaves the pages resident until the system is short of memory, so the range is
 * mapped afresh in place instead, with the flags the stack was reserved with: the old pages go at
 * once, and the new mapping is committed as it is touched, as the reservation was. Either way the
 * stack's guard, below the range, is not touched.
 */

#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

/* Gives back the whole pages of `[low, top - keep)`, where `low` is the lowest address the stack's
 * thread may use, and answers 0, or the `errno` of a release the system refused. A range with no whole
 * page in it is left alone and answers 0.
 *
 * A refusal leaves the range as it was, mapped and committed: a failed `madvise` changes nothing, and
 * a fixed mapping the kernel cannot make puts back the mapping it was to replace before it fails. A
 * remap that answered some other address than the one asked for would be a mapping the stack does not
 * own, so it is unmapped again and counted as refused, the range still holding its old pages. The
 * trim is only a saving, so its caller counts a refusal and goes on. */
int avin_rt_stack_release(char *low, char *top, size_t keep) {
    uintptr_t page = (uintptr_t)getpagesize();
    uintptr_t lo = ((uintptr_t)low + page - 1) & ~(page - 1);
    uintptr_t hi;

    if ((uintptr_t)top < lo + keep + page) return 0;
    hi = ((uintptr_t)top - keep) & ~(page - 1);
    if (hi <= lo) return 0;
#if defined(__APPLE__)
    {
        int flags = MAP_PRIVATE | MAP_ANON | MAP_FIXED;
        void *got;
#if defined(MAP_NORESERVE)
        flags |= MAP_NORESERVE;
#endif
        got = mmap((void *)lo, hi - lo, PROT_READ | PROT_WRITE, flags, -1, 0);
        if (got == MAP_FAILED) return errno != 0 ? errno : EINVAL;
        if (got != (void *)lo) {
            if (munmap(got, hi - lo) != 0) return errno != 0 ? errno : EINVAL;
            return EINVAL;
        }
        return 0;
    }
#else
    if (madvise((void *)lo, hi - lo, MADV_DONTNEED) != 0) return errno != 0 ? errno : EINVAL;
    return 0;
#endif
}

/* An address on the calling thread's stack, just below its caller's frame: where the stack stands. */
__attribute__((noinline)) char *avin_rt_stack_here(void) {
    return (char *)__builtin_frame_address(0);
}
