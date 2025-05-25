#include <benchmark/benchmark.h>
#include <map>
#include <array>
#include <stack>
#include <type_traits>
#include <cstdint>
#include <memory>
#include <random>

constexpr std::uint32_t kSlote1000MsCount = 150;
constexpr std::uint32_t kSlote100MsCount = 200;
constexpr std::uint32_t kSlote50MsCount = 200;
constexpr std::size_t kArenaSize = 10;
using timeStampType = std::uint64_t;

struct BigData {
    std::array<std::uint32_t, kSlote1000MsCount> slot_1000Ms;
    std::array<std::uint32_t, kSlote100MsCount> slot_100Ms;
    std::array<std::uint32_t, kSlote50MsCount> slot_50Ms;
    BigData() {
        std::fill(slot_1000Ms.begin(), slot_1000Ms.end(), 0);
        std::fill(slot_100Ms.begin(), slot_100Ms.end(), 0);
        std::fill(slot_50Ms.begin(), slot_50Ms.end(), 0);
    }
};

template <typename T, std::size_t ArenaSize = kArenaSize>
class ArenaAllocator {
public:
    typedef T value_type;

    ArenaAllocator() throw() {}
    template <typename U> ArenaAllocator(const ArenaAllocator<U, ArenaSize>&) throw() {}

    T* allocate(std::size_t n) {
        if (n != 1) throw std::bad_alloc();
        if (!freeList_.empty()) {
            T* ptr = freeList_.top(); freeList_.pop(); return ptr;
        }
        if (arenaIndex_ < ArenaSize) {
            return reinterpret_cast<T*>(&arena_[arenaIndex_++]);
        }
        return static_cast<T*>(::operator new(sizeof(T)));
    }

    void deallocate(T* p, std::size_t) throw() {
        if (is_in_arena(p)) {
            freeList_.push(p);
        } else {
            ::operator delete(p);
        }
    }

    template <typename U> struct rebind {
        typedef ArenaAllocator<U, ArenaSize> other;
    };

private:
    typedef typename std::aligned_storage<sizeof(T), alignof(T)>::type StorageType;
    static StorageType arena_[ArenaSize];
    static std::size_t arenaIndex_;
    static std::stack<T*> freeList_;

    bool is_in_arena(void* p) const {
        uintptr_t begin = reinterpret_cast<uintptr_t>(&arena_[0]);
        uintptr_t end = reinterpret_cast<uintptr_t>(&arena_[ArenaSize]);
        uintptr_t addr = reinterpret_cast<uintptr_t>(p);
        return addr >= begin && addr < end;
    }
};

template <typename T, std::size_t ArenaSize>
typename ArenaAllocator<T, ArenaSize>::StorageType ArenaAllocator<T, ArenaSize>::arena_[ArenaSize];

template <typename T, std::size_t ArenaSize>
std::size_t ArenaAllocator<T, ArenaSize>::arenaIndex_ = 0;

template <typename T, std::size_t ArenaSize>
std::stack<T*> ArenaAllocator<T, ArenaSize>::freeList_;

// Benchmark with Arena Allocator
static void BM_CustomAllocatorMapInsert(benchmark::State& state) {
    for (auto _ : state) {
        std::map<timeStampType, BigData, std::less<timeStampType>,
            ArenaAllocator<std::pair<const timeStampType, BigData>>> buffer;

        buffer.emplace(0, BigData());
        buffer.emplace(1, BigData());
        buffer.emplace(2, BigData());
        buffer.emplace(3, BigData());
        buffer.emplace(4, BigData());
        buffer.emplace(5, BigData());
        buffer.emplace(6, BigData());
        buffer.emplace(7, BigData());
        buffer.emplace(8, BigData());
        buffer.emplace(9, BigData());
        for (int i = 10; i < state.range(0)+10; ++i) {
            buffer.emplace(i, BigData());
            if (!buffer.empty()){
            buffer.erase(buffer.begin());
            if (!buffer.empty())
            buffer.erase(buffer.begin());
        }
        }
    }
}
BENCHMARK(BM_CustomAllocatorMapInsert)->Arg(10)->Arg(50)->Arg(100);

// Benchmark with Default Allocator
static void BM_DefaultAllocatorMapInsert(benchmark::State& state) {
    for (auto _ : state) {
        std::map<timeStampType, BigData> buffer;
        buffer.emplace(0, BigData());
        buffer.emplace(1, BigData());
        buffer.emplace(2, BigData());
        buffer.emplace(3, BigData());
        buffer.emplace(4, BigData());
        buffer.emplace(5, BigData());
        buffer.emplace(6, BigData());
        buffer.emplace(7, BigData());
        buffer.emplace(8, BigData());
        buffer.emplace(9, BigData());
        for (int i = 10; i < state.range(0)+10; ++i) {
            buffer.emplace(i, BigData());
            if (!buffer.empty()){
            buffer.erase(buffer.begin());
            if (!buffer.empty())
            buffer.erase(buffer.begin());
        }
        }
    }
}
BENCHMARK(BM_DefaultAllocatorMapInsert)->Arg(10)->Arg(50)->Arg(100);

BENCHMARK_MAIN();
