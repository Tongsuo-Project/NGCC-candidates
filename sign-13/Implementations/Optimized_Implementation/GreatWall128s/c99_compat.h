#ifndef GREATWALL_C99_COMPAT_H
#define GREATWALL_C99_COMPAT_H

#include <stddef.h>

/* False conditions remain compilation errors, including with NDEBUG. */
#define GW_STATIC_ASSERT(condition, message) \
    extern void greatwall_static_assertion(char (*)[(condition) ? 1 : -1])

#define GW_ALIGNOF(type) offsetof(struct { char prefix; type value; }, value)

/* Linux/POSIX allocation with SIMD alignment; release with free(). */
void* greatwall_aligned_alloc(size_t alignment, size_t size);

#endif
