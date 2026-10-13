#pragma once

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

/**
 * Test-binary-only allocation guard for real-time path checks.
 * Counts allocations only while a ScopedRtGuard is active on the current thread.
 */
namespace fmlib::test
{

inline thread_local bool rtGuardActive = false;
inline std::atomic<uint64_t> rtAllocCount { 0 };
inline std::atomic<uint64_t> rtAllocBytes { 0 };

struct ScopedRtGuard
{
    uint64_t allocsAtStart = 0;
    uint64_t bytesAtStart = 0;
    bool prev = false;

    ScopedRtGuard()
    {
        prev = rtGuardActive;
        allocsAtStart = rtAllocCount.load (std::memory_order_relaxed);
        bytesAtStart = rtAllocBytes.load (std::memory_order_relaxed);
        rtGuardActive = true;
    }

    ~ScopedRtGuard() { rtGuardActive = prev; }

    uint64_t allocations() const
    {
        return rtAllocCount.load (std::memory_order_relaxed) - allocsAtStart;
    }

    uint64_t bytes() const
    {
        return rtAllocBytes.load (std::memory_order_relaxed) - bytesAtStart;
    }
};

} // namespace fmlib::test

#if defined(FMLIBPLUG_RT_ALLOC_GUARD)

void* operator new (std::size_t size)
{
    if (fmlib::test::rtGuardActive)
    {
        fmlib::test::rtAllocCount.fetch_add (1, std::memory_order_relaxed);
        fmlib::test::rtAllocBytes.fetch_add ((uint64_t) size, std::memory_order_relaxed);
    }
    if (void* p = std::malloc (size))
        return p;
    throw std::bad_alloc();
}

void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{
    if (fmlib::test::rtGuardActive)
    {
        fmlib::test::rtAllocCount.fetch_add (1, std::memory_order_relaxed);
        fmlib::test::rtAllocBytes.fetch_add ((uint64_t) size, std::memory_order_relaxed);
    }
    return std::malloc (size);
}

void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete (void* p, const std::nothrow_t&) noexcept { std::free (p); }

void* operator new[] (std::size_t size)
{
    return ::operator new (size);
}

void* operator new[] (std::size_t size, const std::nothrow_t& nt) noexcept
{
    return ::operator new (size, nt);
}

void operator delete[] (void* p) noexcept { ::operator delete (p); }
void operator delete[] (void* p, std::size_t s) noexcept { ::operator delete (p, s); }

#endif
