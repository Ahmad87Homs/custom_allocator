#include <chrono>
#include <array>
#include <stack>
#include <type_traits>
#include <cstdint>
#include <iostream>
#include <thread>
#include <map>
#include <atomic>

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

template <typename Key>
using BigDataMap = std::map<Key, BigData, std::less<Key>,
                            ArenaAllocator<std::pair<const Key, BigData>>>;

int main() {
  std::cout << "[Custom Allocator] Start Demonstration\n";

  // Create a map using our custom allocator
  BigDataMap<std::uint64_t> big_data_buffer;

  // Pre-populate some elements to demonstrate arena usage
  std::cout << "\n--- Initial Pre-population (should use Arena New) ---\n";
  big_data_buffer.emplace(0, BigData());
  big_data_buffer.emplace(1, BigData());
  big_data_buffer.emplace(2, BigData());
  big_data_buffer.emplace(3, BigData());
  big_data_buffer.emplace(4, BigData());
  std::cout << "Map size after pre-population: " << big_data_buffer.size()
            << "\n";
  std::cout << "---------------------------------------------------------------"
               "--------------------\n";

  std::atomic<bool> stop_threads{false}; // Use a more descriptive name

  // --- Inserter Thread ---
  std::thread inserter_thread([&]() {
    std::uint32_t counter =
        5; // Start counter from where pre-population left off
    while (!stop_threads) {
      auto current_key = counter++;
      // The emplace operation will trigger allocate on our custom allocator
      big_data_buffer.emplace(current_key, BigData());
      // Accessing big_data_buffer.end()->first can be risky if map is modified
      // concurrently It's generally better to log the key that was just
      // inserted if you need it.
      std::cout << "Emplaced BigData Struct [" << current_key << "] \n";
      std::this_thread::sleep_for(std::chrono::seconds(1));
      std::cout << "-----------------------------------------------------------"
                   "------------------------\n";
    }
  });

  // --- Consumer Thread ---
  std::thread consumer_thread([&]() {
    while (!stop_threads) {
      if (!big_data_buffer.empty()) {
        auto it = big_data_buffer.begin(); // Get iterator to the first element
        auto key_to_remove = it->first;

        // Erase operation will trigger deallocate on our custom allocator
        big_data_buffer.erase(it); // Erase by iterator is efficient

        std::cout << "Removed BigData Struct [" << key_to_remove << "]\n";

        // Control sleep duration based on map size to simulate load
        if (big_data_buffer.size() > 3) {
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
      } else {
        std::cout << "Map is Empty. Waiting for insertions.\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
      std::cout << "-----------------------------------------------------------"
                   "------------------------\n";
    }
  });

  // Let the threads run for a duration
  std::this_thread::sleep_for(std::chrono::seconds(30)); // Run for 30 seconds
  stop_threads = true; // Signal threads to stop

  // Join threads to ensure they complete
  inserter_thread.join();
  consumer_thread.join();

  std::cout << "\n--- Final Map State Before Clear ---" << std::endl;
  std::cout << "Current map size: " << big_data_buffer.size() << "\n";
  // This will trigger deallocations for all remaining nodes
  std::cout << "\n--- Clearing the map (deallocating remaining elements) ---\n";
  big_data_buffer.clear();
  std::cout << "Map size after final clear: " << big_data_buffer.size() << "\n";

  std::cout << "[Custom Allocator] End Demonstration\n";

  return 0;
}