#include <algorithm> // For std::fill
#include <array>     // For std::array
#include <atomic>    // For std::atomic
#include <chrono>    // For std::chrono::seconds, milliseconds
#include <cstddef>   // For std::size_t, std::ptrdiff_t
#include <cstdint>   // For std::uint32_t, std::uint64_t
#include <exception> // For std::bad_alloc
#include <iomanip>   // For std::hex, std::showbase, std::dec
#include <iostream>  // For std::cout
#include <map>       // For std::map
#include <memory>    // For std::aligned_storage
#include <mutex>
#include <new>         // For ::operator new, ::operator delete
#include <stack>       // For std::stack (freeList_)
#include <thread>      // For std::thread
#include <type_traits> // For std::aligned_storage, alignof

// --- Constants ---
constexpr std::uint32_t kSlote1000MsCount = 150;
constexpr std::uint32_t kSlote100MsCount = 200;
constexpr std::uint32_t kSlote50MsCount = 200;
constexpr std::uint32_t kAreanaSize =
    10; // Number of nodes to pre-allocate in the arena
using timeStampType = std::uint64_t;

// --- BigData Structure ---
struct BigData {
  std::array<std::uint32_t, kSlote1000MsCount> slot_1000Ms;
  std::array<std::uint32_t, kSlote100MsCount> slot_100Ms;
  std::array<std::uint32_t, kSlote50MsCount> slot_50Ms;

  // Default constructor: Initializes arrays. Implicitly noexcept.
  BigData() noexcept {
    std::fill(slot_1000Ms.begin(), slot_1000Ms.end(), 0);
    std::fill(slot_100Ms.begin(), slot_100Ms.end(), 0);
    std::fill(slot_50Ms.begin(), slot_50Ms.end(), 0);
  }
  // Destructor: Implicitly noexcept.
  ~BigData() = default;

  // Default copy/move constructors/assignments (implicitly noexcept for array
  // members)
  BigData(const BigData &) = default;
  BigData(BigData &&) noexcept = default;
  BigData &operator=(const BigData &) = default;
  BigData &operator=(BigData &&) noexcept = default;
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
      std::cout << "Allocate [Arena Reuse]: " << static_cast<void*>(ptr) << "\n";
      return ptr;
    }

    if (arenaIndex_ < ArenaSize) {
      T* ptr = reinterpret_cast<T*>(&arena_[arenaIndex_++]);
      std::cout << "Allocate [Arena New]: " << static_cast<void*>(ptr) << "\n";
      return ptr;
    }

    T* ptr = static_cast<T*>(::operator new(sizeof(T)));
    std::cout << "Allocate [Dynamic]: " << static_cast<void*>(ptr) << "\n";
    return ptr;
  }

  void deallocate(T* p, std::size_t n) noexcept {
    if (!p || n == 0) return;

    if (is_in_arena(p)) {
      if (freeListTop_ < ArenaSize) {
        freeList_[freeListTop_++] = p;
        std::cout << "Deallocate [Arena]: " << static_cast<void*>(p) << "\n";
      } else {
        std::cout << "[Warning] Arena free list overflow!\n";
      }
    } else {
      std::cout << "Deallocate [Dynamic]: " << static_cast<void*>(p) << "\n";
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
std::size_t ArenaAllocator<T, ArenaSize>::freeListTop_ = 0;
// --- Convenience Type Alias for the Map ---
// Makes it easier to declare your map with the custom allocator.
// std::map's node type is std::pair<const Key, Value>.
template <typename Key>
using BigDataMap = std::map<Key, BigData, std::less<Key>,
                            ArenaAllocator<std::pair<const Key, BigData>>>;

// --- Main Function for Demonstration ---
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