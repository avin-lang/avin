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

#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

/* Gives back the whole pages of `[low, top - keep)`, where `low` is the lowest address the stack's
 * thread may use. A range with no whole page in it is left alone. */
void avin_rt_stack_release(char *low, char *top, size_t keep) {
    uintptr_t page = (uintptr_t)getpagesize();
    uintptr_t lo = ((uintptr_t)low + page - 1) & ~(page - 1);
    uintptr_t hi;

    if ((uintptr_t)top < lo + keep + page) return;
    hi = ((uintptr_t)top - keep) & ~(page - 1);
    if (hi <= lo) return;
#if defined(__APPLE__)
    {
        int flags = MAP_PRIVATE | MAP_ANON | MAP_FIXED;
#if defined(MAP_NORESERVE)
        flags |= MAP_NORESERVE;
#endif
        (void)mmap((void *)lo, hi - lo, PROT_READ | PROT_WRITE, flags, -1, 0);
    }
#else
    (void)madvise((void *)lo, hi - lo, MADV_DONTNEED);
#endif
}

/* An address on the calling thread's stack, just below its caller's frame: where the stack stands. */
__attribute__((noinline)) char *avin_rt_stack_here(void) {
    return (char *)__builtin_frame_address(0);
}
