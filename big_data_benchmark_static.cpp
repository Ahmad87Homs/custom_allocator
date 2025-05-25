#include <benchmark/benchmark.h>
#include <map>
#include <array>
#include <type_traits>
#include <cstdint>
#include <iostream>
#include <cassert>

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
        slot_1000Ms.fill(0);
        slot_100Ms.fill(0);
        slot_50Ms.fill(0);
    }
};


// ArenaAllocator definition
template <typename T, std::size_t ArenaSize = 10>
class ArenaAllocator {
public:
  using value_type = T;
  using pointer = T*;

  template <typename U>
  struct rebind {
    using other = ArenaAllocator<U, ArenaSize>;
  };

  ArenaAllocator() noexcept {}
  template <typename U>
  ArenaAllocator(const ArenaAllocator<U, ArenaSize>&) noexcept {}

  T* allocate(std::size_t n) {
    if (n != 1) throw std::bad_alloc();

    if (freeListTop_ > 0) {
      T* ptr = freeList_[--freeListTop_];
    //   std::cout << "Allocate [Arena Reuse]: " << static_cast<void*>(ptr) << "\n";
      return ptr;
    }

    if (arenaIndex_ < ArenaSize) {
      T* ptr = reinterpret_cast<T*>(&arena_[arenaIndex_++]);
    //   std::cout << "Allocate [Arena New]: " << static_cast<void*>(ptr) << "\n";
      return ptr;
    }

    T* ptr = static_cast<T*>(::operator new(sizeof(T)));
    // std::cout << "Allocate [Dynamic]: " << static_cast<void*>(ptr) << "\n";
    return ptr;
  }

  void deallocate(T* p, std::size_t n) noexcept {
    if (!p || n == 0) return;

    if (is_in_arena(p)) {
      if (freeListTop_ < ArenaSize) {
        freeList_[freeListTop_++] = p;
        // std::cout << "Deallocate [Arena]: " << static_cast<void*>(p) << "\n";
      } else {
        // std::cout << "[Warning] Arena free list overflow!\n";
      }
    } else {
    //   std::cout << "Deallocate [Dynamic]: " << static_cast<void*>(p) << "\n";
      ::operator delete(p);
    }
  }

  template <typename U, typename... Args>
  void construct(U* p, Args&&... args) {
    ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
  }

  template <typename U>
  void destroy(U* p) {
    p->~U();
  }

  bool operator==(const ArenaAllocator&) const noexcept { return true; }
  bool operator!=(const ArenaAllocator&) const noexcept { return false; }

private:
  using StorageType = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

  static bool is_in_arena(T* p) {
    auto begin = reinterpret_cast<std::uintptr_t>(&arena_[0]);
    auto end = reinterpret_cast<std::uintptr_t>(&arena_[ArenaSize]);
    auto addr = reinterpret_cast<std::uintptr_t>(p);
    return addr >= begin && addr < end;
  }

  static StorageType arena_[ArenaSize];
  static std::size_t arenaIndex_;
  static T* freeList_[ArenaSize];
  static std::size_t freeListTop_;
};

// Static members initialization
template <typename T, std::size_t ArenaSize>
typename ArenaAllocator<T, ArenaSize>::StorageType ArenaAllocator<T, ArenaSize>::arena_[ArenaSize];

template <typename T, std::size_t ArenaSize>
std::size_t ArenaAllocator<T, ArenaSize>::arenaIndex_ = 0;

template <typename T, std::size_t ArenaSize>
T* ArenaAllocator<T, ArenaSize>::freeList_[ArenaSize] = {nullptr};

template <typename T, std::size_t ArenaSize>
std::size_t ArenaAllocator<T, ArenaSize>::freeListTop_ = 0 ;// Benchmark with Arena Allocator
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
         if(i%10 ==0){
            for(int j= 0 ; j < 5; j++){
                buffer.emplace(i*j, BigData());
            }
         }
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
        if(i%10 ==0){
            for(int j= 0 ; j < 5; j++){
                buffer.emplace(i*j, BigData());
            }}
        }
        }
    }
}
BENCHMARK(BM_DefaultAllocatorMapInsert)->Arg(10)->Arg(50)->Arg(100);

BENCHMARK_MAIN();
