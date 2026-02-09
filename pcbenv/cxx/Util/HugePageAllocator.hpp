#pragma once

#include <cstddef>
#include <cstdlib>
#include <memory>

#if defined(_WIN32)

template<typename T> using AlignedTHPAllocator = std::allocator<T>;

#else

#include <cstring>
#include <new>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

constexpr std::size_t hugepage_size = 2 * 1024 * 1024;

template<typename T> struct AlignedTHPAllocator
{
    using value_type = T;

    AlignedTHPAllocator() = default;

    template<class U> constexpr AlignedTHPAllocator(const AlignedTHPAllocator<U>&) noexcept { }

    T *allocate(std::size_t n)
    {
        std::size_t bytes = n * sizeof(T);
        void *ptr = nullptr;
        int res = posix_memalign(&ptr, hugepage_size, bytes);
        if (res != 0 || ptr == nullptr)
            throw std::bad_alloc();
        madvise(ptr, bytes, MADV_HUGEPAGE);
        return static_cast<T *>(ptr);
    }

    void deallocate(T *p, std::size_t n) noexcept
    {
        std::free(p);
    }
};

template<typename T, typename U> bool operator==(const AlignedTHPAllocator<T>&, const AlignedTHPAllocator<U>&) { return true; }
template<typename T, typename U> bool operator!=(const AlignedTHPAllocator<T>&, const AlignedTHPAllocator<U>&) { return false; }

#endif // _WIN32
