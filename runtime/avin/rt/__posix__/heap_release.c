/* Giving the heap's free pages back to the system.
 *
 * After a collection the collector hands the runtime the whole pages of every large free run, which
 * hold nothing a program can reach; the runtime gives them back here, so a burst of allocation that
 * has been dropped does not keep the process at its peak footprint for ever. The collector may hand
 * the same pages out again at any later allocation, with no call first, and its allocations work
 * whether a page then reads as zero or as what it held before.
 *
 * On Linux, `madvise(MADV_DONTNEED)` drops the pages at once, and the next touch finds zeroed ones.
 * On macOS neither `MADV_FREE` nor `MADV_FREE_REUSABLE` lowers the resident size the system reports
 * for the process (measured: a dropped 930 MB burst stayed at 1047 MB under both), and the second
 * also expects `MADV_FREE_REUSE` before the pages are used again, which the collector never calls.
 * So the range is mapped afresh in place, as the stacks' unused pages are: the old pages go at once,
 * and the new mapping is committed as it is touched.
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

/* The system's page size, in bytes. */
size_t avin_rt_page_size(void) {
    return (size_t)getpagesize();
}

/* Gives back the `len` bytes at `at`, whole pages on a page boundary, and answers 0, or the `errno`
 * of a release the system refused. A refusal leaves the range mapped and holding what it held: a
 * failed `madvise` changes nothing, and a fixed mapping the kernel cannot make puts back the mapping
 * it was to replace before it fails. A remap that answered some other address is unmapped again and
 * counted as refused. A release is only a saving, so its caller counts a refusal and goes on. */
int avin_rt_heap_release(char *at, size_t len) {
    if (len == 0) return 0;
#if defined(__APPLE__)
    {
        int flags = MAP_PRIVATE | MAP_ANON | MAP_FIXED;
        void *got;
#if defined(MAP_NORESERVE)
        flags |= MAP_NORESERVE;
#endif
        got = mmap((void *)at, len, PROT_READ | PROT_WRITE, flags, -1, 0);
        if (got == MAP_FAILED) return errno != 0 ? errno : EINVAL;
        if (got != (void *)at) {
            if (munmap(got, len) != 0) return errno != 0 ? errno : EINVAL;
            return EINVAL;
        }
        return 0;
    }
#else
    if (madvise((void *)at, len, MADV_DONTNEED) != 0) return errno != 0 ? errno : EINVAL;
    return 0;
#endif
}
