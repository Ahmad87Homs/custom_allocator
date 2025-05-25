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

// --- Custom Allocator: ArenaAllocator ---
template <typename T, std::size_t ArenaSize = kAreanaSize>
class ArenaAllocator {
public:
  // Standard allocator type definitions
  using value_type = T;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  // Constructors are noexcept as they don't allocate or throw
  ArenaAllocator() noexcept = default;

  template <typename U>
  ArenaAllocator(const ArenaAllocator<U, ArenaSize> &) noexcept {}

  // allocate: Allocates memory for n objects of type T
  // Can throw std::bad_alloc if n != 1 or dynamic allocation fails.
  T *allocate(std::size_t n) {
    // We only support single-element allocations for our arena pool.
    // std::map's nodes will request n=1.
    if (n != 1) {
      std::cout << "[ERROR] ArenaAllocator: Requested " << n
                << " elements. Only n=1 is supported directly by arena.\n";
      throw std::bad_alloc(); // Or fall back to dynamic allocation if preferred
    }

    // Try to get a pointer from the freelist (reused arena slot)
    if (!freeList_.empty()) {
      std::lock_guard<std::mutex> lock(s_mutex_); // Protect freelist access
      if (!freeList_.empty()) { // Double-check after acquiring lock
        T *ptr = freeList_.top();
        freeList_.pop();
        std::cout << "Allocate [Arena Reuse]: " << std::showbase << std::hex
                  << ptr << std::dec << "\n";
        return ptr;
      }
    }

    // If freelist is empty, try to allocate a new slot from the arena's
    // contiguous block
    if (arenaIndex_ < ArenaSize) {
      std::lock_guard<std::mutex> lock(
          s_mutex_);                 // Protect arenaIndex_ and arena_ access
      if (arenaIndex_ < ArenaSize) { // Double-check after acquiring lock
        T *ptr = reinterpret_cast<T *>(&arena_[arenaIndex_++]);
        std::cout << "Allocate [Arena New]: " << std::showbase << std::hex
                  << ptr << std::dec << " (Index: " << arenaIndex_ - 1 << ")\n";
        return ptr;
      }
    }

    // If both arena reuse and new arena slots are exhausted, fall back to
    // dynamic allocation
    T *ptr = static_cast<T *>(::operator new(sizeof(T)));
    if (!ptr) {
      throw std::bad_alloc(); // operator new can throw itself, but explicit
                              // check is fine.
    }
    std::cout << "Allocate [Dynamic]: " << std::showbase << std::hex << ptr
              << std::dec << "\n";
    return ptr;
  }

  // deallocate: Deallocates memory. This method should not throw.
  void deallocate(T *p, std::size_t n) noexcept {
    if (p == nullptr || n == 0)
      return; // Nothing to deallocate

    // Protect static members during deallocation
    std::lock_guard<std::mutex> lock(s_mutex_);

    // Check if the pointer 'p' belongs to our arena
    if (is_in_arena(p)) {
      freeList_.push(p); // Add to freelist for reuse
      std::cout << "Deallocate [Arena]: " << std::showbase << std::hex << p
                << std::dec << "\n";
    } else {
      // It was dynamically allocated, free it using global delete
      std::cout << "Deallocate [Dynamic]: " << std::showbase << std::hex << p
                << std::dec << "\n";
      ::operator delete(p);
    }
  }

  // Required for containers to rebind the allocator for different types (e.g.,
  // node types)
  template <typename U> struct rebind {
    typedef ArenaAllocator<U, ArenaSize> other;
  };

  // Equality operators: All instances of this allocator are equal as they share
  // static resources.
  template <typename U, std::size_t N>
  friend bool operator==(const ArenaAllocator &, const ArenaAllocator<U, N> &) {
    return true;
  }

  template <typename U, std::size_t N>
  friend bool operator!=(const ArenaAllocator &, const ArenaAllocator<U, N> &) {
    return false;
  }

private:
  // --- Static Members (shared across all instances of ArenaAllocator<T,
  // ArenaSize>) --- Use std::aligned_storage for proper alignment of raw memory
  typedef
      typename std::aligned_storage<sizeof(T), alignof(T)>::type StorageType;

  // The raw memory arena.
  static StorageType arena_[ArenaSize];
  // Index for next available slot in the arena (for initial allocations).
  static std::atomic<std::size_t>
      arenaIndex_; // Using atomic for thread-safe increment
  // Stack of pointers to freed arena slots for reuse.
  static std::stack<T *> freeList_;
  // Mutex for protecting access to arenaIndex_ and freeList_.
  static std::mutex s_mutex_;

  // Helper to check if a pointer is within the arena's memory range
  bool is_in_arena(void *p) const noexcept {
    // Cast to uintptr_t for safe pointer arithmetic and comparison
    uintptr_t begin = reinterpret_cast<uintptr_t>(&arena_[0]);
    uintptr_t end =
        reinterpret_cast<uintptr_t>(&arena_[ArenaSize]); // One past the end
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    return addr >= begin && addr < end;
  }
};

// --- Definition of Static Members (must be defined outside the class template)
// ---
template <typename T, std::size_t ArenaSize>
typename ArenaAllocator<T, ArenaSize>::StorageType
    ArenaAllocator<T, ArenaSize>::arena_[ArenaSize];

template <typename T, std::size_t ArenaSize>
std::atomic<std::size_t> ArenaAllocator<T, ArenaSize>::arenaIndex_ =
    0; // Initialize atomic

template <typename T, std::size_t ArenaSize>
std::stack<T *> ArenaAllocator<T, ArenaSize>::freeList_;

template <typename T, std::size_t ArenaSize>
std::mutex ArenaAllocator<T, ArenaSize>::s_mutex_;

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