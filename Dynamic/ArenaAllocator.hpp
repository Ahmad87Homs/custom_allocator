#ifndef ARENA_ALLOCATOR_HPP
#define ARENA_ALLOCATOR_HPP

#include <cstddef>     // For std::size_t, std::ptrdiff_t
#include <exception>   // For std::bad_alloc
#include <stack>       // For std::stack
#include <memory>      // For std::aligned_storage
#include <new>         // For ::operator new, ::operator delete
// #include <mutex>    // Removed: Not needed for single-threaded benchmarks
// #include <atomic>   // Removed: Not needed for single-threaded benchmarks
// #include <iostream> // Removed: Not needed for production benchmarks
// #include <iomanip>  // Removed: Not needed for production benchmarks

constexpr std::size_t kAreanaSize = 10; // Number of nodes to pre-allocate in the arena

template <typename T, std::size_t ArenaSize = kAreanaSize>
class ArenaAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    ArenaAllocator() noexcept = default;

    template <typename U>
    ArenaAllocator(const ArenaAllocator<U, ArenaSize>&) noexcept {}

    T* allocate(std::size_t n) {
        if (n != 1) {
            // For benchmarking, we don't want to throw for n > 1.
            // Fallback to dynamic for n > 1.
            return static_cast<pointer>(::operator new(n * sizeof(T)));
        }

        // Try to get a pointer from the freelist (reused arena slot)
        if (!freeList_.empty()) {
            T* ptr = freeList_.top();
            freeList_.pop();
            return ptr;
        }

        // If freelist is empty, try to allocate a new slot from the arena's contiguous block
        if (arenaIndex_ < ArenaSize) {
            return reinterpret_cast<T*>(&arena_[arenaIndex_++]);
        }

        // Fall back to dynamic allocation
        T* ptr = static_cast<T*>(::operator new(sizeof(T)));
        if (!ptr) {
            throw std::bad_alloc();
        }
        return ptr;
    }

    void deallocate(T* p, std::size_t n) noexcept {
        if (p == nullptr || n == 0) return;

        if (is_in_arena(p)) {
            freeList_.push(p);
        } else {
            ::operator delete(p);
        }
    }

    template <typename U>
    struct rebind {
        typedef ArenaAllocator<U, ArenaSize> other;
    };

    template <typename U, std::size_t N>
    friend bool operator==(const ArenaAllocator&, const ArenaAllocator<U, N>&) { return true; }

    template <typename U, std::size_t N>
    friend bool operator!=(const ArenaAllocator&, const ArenaAllocator<U, N>&) { return false; }

private:
    typedef typename std::aligned_storage<sizeof(T), alignof(T)>::type StorageType;

    // Static member declarations (no alignas here, it's in the definition within the class)
    static StorageType arena_[ArenaSize];
    static std::size_t arenaIndex_; // Changed from std::atomic to std::size_t
    static std::stack<T*> freeList_;
    // static std::mutex s_mutex_; // Removed: No longer needed for single-threaded

    bool is_in_arena(void* p) const noexcept {
        uintptr_t begin = reinterpret_cast<uintptr_t>(&arena_[0]);
        uintptr_t end = reinterpret_cast<uintptr_t>(&arena_[ArenaSize]);
        uintptr_t addr = reinterpret_cast<uintptr_t>(p);
        return addr >= begin && addr < end;
    }
};

// Definition of Static Members
template <typename T, std::size_t ArenaSize>
typename ArenaAllocator<T, ArenaSize>::StorageType ArenaAllocator<T, ArenaSize>::arena_[ArenaSize];

template <typename T, std::size_t ArenaSize>
std::size_t ArenaAllocator<T, ArenaSize>::arenaIndex_ = 0; // Initialize as plain size_t

template <typename T, std::size_t ArenaSize>
std::stack<T*> ArenaAllocator<T, ArenaSize>::freeList_;

// Removed: static std::mutex DataSlotAllocator<T>::s_mutex_;

// --- Default Allocator Wrapper (for comparison) ---
// This just wraps std::allocator to allow consistent template usage.
template <typename T>
class DefaultAllocator {
public:
    using value_type = T;

    DefaultAllocator() noexcept = default;
    template <typename U>
    DefaultAllocator(const DefaultAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        return std::allocator<T>{}.allocate(n);
    }

    void deallocate(T* p, std::size_t n) noexcept {
        std::allocator<T>{}.deallocate(p, n);
    }

    template <typename U>
    struct rebind {
        typedef DefaultAllocator<U> other;
    };

    template <typename U>
    friend bool operator==(const DefaultAllocator&, const DefaultAllocator<U>&) { return true; }

    template <typename U>
    friend bool operator!=(const DefaultAllocator&, const DefaultAllocator<U>&) { return false; }
};

#endif // ARENA_ALLOCATOR_HPP