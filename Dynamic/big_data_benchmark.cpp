#include "ArenaAllocator.hpp"
#include "BigData.hpp"

#include <benchmark/benchmark.h>
#include <map>
#include <random> // For generating random keys

// Type alias for map using ArenaAllocator
template <typename Key>
using ArenaBigDataMap = std::map<Key, BigData, std::less<Key>,
                                 ArenaAllocator<std::pair<const Key, BigData>>>;

// Type alias for map using DefaultAllocator (std::allocator wrapper)
template <typename Key>
using DefaultBigDataMap =
    std::map<Key, BigData, std::less<Key>,
             DefaultAllocator<std::pair<const Key, BigData>>>;

// Random number generator for keys
std::mt19937_64 rng(std::random_device{}());

// Helper function to generate a unique key
uint64_t generate_unique_key() {
  static uint64_t key_counter = 0;
  return key_counter++;
}

// --- Benchmark for Insert Operations ---

// Benchmark: Insert with ArenaAllocator
void BM_Insert_Arena(benchmark::State &state) {
  ArenaBigDataMap<uint64_t> m;
  uint64_t key = 0;
  for (auto _ : state) {
    state.PauseTiming(); // Pause timing for key generation
    key = generate_unique_key();
    state.ResumeTiming(); // Resume timing for map operation
    m.emplace(key, BigData());
  }
  // Ensure map is not optimized away and cleans up
  benchmark::DoNotOptimize(m);
}
BENCHMARK(BM_Insert_Arena);

// Benchmark: Insert with DefaultAllocator
void BM_Insert_Default(benchmark::State &state) {
  DefaultBigDataMap<uint64_t> m;
  uint64_t key = 0;
  for (auto _ : state) {
    state.PauseTiming();
    key = generate_unique_key();
    state.ResumeTiming();
    m.emplace(key, BigData());
  }
  benchmark::DoNotOptimize(m);
}
BENCHMARK(BM_Insert_Default);

// --- Benchmark for Erase Operations ---

// Helper function to fill a map for erase benchmarks
template <typename MapType> void fill_map(MapType &m, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    m.emplace(generate_unique_key(), BigData());
  }
}

// Benchmark: Erase with ArenaAllocator
void BM_Erase_Arena(benchmark::State &state) {
  ArenaBigDataMap<uint64_t> m;
  // Fill the map to start with some elements
  fill_map(m, state.range(0)); // Use range as initial map size

  // Keys to erase (randomly selected from current map)
  std::vector<uint64_t> keys_to_erase;
  for (const auto &pair : m) {
    keys_to_erase.push_back(pair.first);
  }
  std::shuffle(keys_to_erase.begin(), keys_to_erase.end(), rng);
  size_t erase_idx = 0;

  for (auto _ : state) {
    state.PauseTiming();
    // If we run out of keys, refill the map
    if (erase_idx >= keys_to_erase.size()) {
      state.PauseTiming(); // Pause to avoid timing refill
      m.clear();           // Clear before refilling
      fill_map(m, state.range(0));
      keys_to_erase.clear();
      for (const auto &pair : m) {
        keys_to_erase.push_back(pair.first);
      }
      std::shuffle(keys_to_erase.begin(), keys_to_erase.end(), rng);
      erase_idx = 0;
      state.ResumeTiming();
    }
    uint64_t key = keys_to_erase[erase_idx++];
    state.ResumeTiming();
    m.erase(key);
  }
}
// Run erase benchmarks with different initial map sizes
BENCHMARK(BM_Erase_Arena)
    ->Range(10, 1000); // Test with maps of 10 to 1000 elements

// Benchmark: Erase with DefaultAllocator
void BM_Erase_Default(benchmark::State &state) {
  DefaultBigDataMap<uint64_t> m;
  fill_map(m, state.range(0));

  std::vector<uint64_t> keys_to_erase;
  for (const auto &pair : m) {
    keys_to_erase.push_back(pair.first);
  }
  std::shuffle(keys_to_erase.begin(), keys_to_erase.end(), rng);
  size_t erase_idx = 0;

  for (auto _ : state) {
    state.PauseTiming();
    if (erase_idx >= keys_to_erase.size()) {
      state.PauseTiming();
      m.clear();
      fill_map(m, state.range(0));
      keys_to_erase.clear();
      for (const auto &pair : m) {
        keys_to_erase.push_back(pair.first);
      }
      std::shuffle(keys_to_erase.begin(), keys_to_erase.end(), rng);
      erase_idx = 0;
      state.ResumeTiming();
    }
    uint64_t key = keys_to_erase[erase_idx++];
    state.ResumeTiming();
    m.erase(key);
  }
}
BENCHMARK(BM_Erase_Default)->Range(10, 1000);

// --- Benchmark for Mixed Insert/Erase Operations ---

// Benchmark: Mixed (50% insert, 50% erase) with ArenaAllocator
void BM_Mixed_Arena(benchmark::State &state) {
  ArenaBigDataMap<uint64_t> m;
  // Pre-fill map to avoid always inserting into empty map
  fill_map(m, state.range(0)); // Initial size of map

  std::vector<uint64_t> existing_keys;
  for (const auto &pair : m) {
    existing_keys.push_back(pair.first);
  }
  size_t key_idx = 0;

  // Use a random device for operation choice (insert vs erase)
  std::uniform_int_distribution<> distrib(0, 1); // 0 for erase, 1 for insert

  for (auto _ : state) {
    state.PauseTiming();
    uint64_t current_key;
    int op_choice = distrib(rng);
    state.ResumeTiming();

    if (op_choice == 0 && !m.empty()) { // Erase
      state.PauseTiming();
      // Erase an existing key
      if (key_idx >= existing_keys.size()) { // Refill keys if exhausted
        existing_keys.clear();
        for (const auto &pair : m) {
          existing_keys.push_back(pair.first);
        }
        std::shuffle(existing_keys.begin(), existing_keys.end(), rng);
        key_idx = 0;
      }
      if (!existing_keys.empty()) {
        current_key = existing_keys[key_idx++];
        state.ResumeTiming();
        m.erase(current_key);
      } else { // If map becomes empty, force insert
        state.ResumeTiming();
        op_choice = 1;
      }
    }
    if (op_choice == 1 ||
        m.empty()) { // Insert (or forced insert if erase failed/map empty)
      state.PauseTiming();
      current_key = generate_unique_key();
      state.ResumeTiming();
      m.emplace(current_key, BigData());
      existing_keys.push_back(current_key); // Add to potential erase list
    }
  }
}
BENCHMARK(BM_Mixed_Arena)->Range(10, 1000);

// Benchmark: Mixed (50% insert, 50% erase) with DefaultAllocator
void BM_Mixed_Default(benchmark::State &state) {
  DefaultBigDataMap<uint64_t> m;
  fill_map(m, state.range(0));

  std::vector<uint64_t> existing_keys;
  for (const auto &pair : m) {
    existing_keys.push_back(pair.first);
  }
  size_t key_idx = 0;

  std::uniform_int_distribution<> distrib(0, 1);

  for (auto _ : state) {
    state.PauseTiming();
    uint64_t current_key;
    int op_choice = distrib(rng);
    state.ResumeTiming();

    if (op_choice == 0 && !m.empty()) { // Erase
      state.PauseTiming();
      if (key_idx >= existing_keys.size()) {
        existing_keys.clear();
        for (const auto &pair : m) {
          existing_keys.push_back(pair.first);
        }
        std::shuffle(existing_keys.begin(), existing_keys.end(), rng);
        key_idx = 0;
      }
      if (!existing_keys.empty()) {
        current_key = existing_keys[key_idx++];
        state.ResumeTiming();
        m.erase(current_key);
      } else {
        state.ResumeTiming();
        op_choice = 1;
      }
    }
    if (op_choice == 1 || m.empty()) { // Insert
      state.PauseTiming();
      current_key = generate_unique_key();
      state.ResumeTiming();
      m.emplace(current_key, BigData());
      existing_keys.push_back(current_key);
    }
  }
}
BENCHMARK(BM_Mixed_Default)->Range(10, 1000);

// Main for Google Benchmark
BENCHMARK_MAIN();