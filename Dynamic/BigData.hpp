#ifndef BIGDATA_HPP
#define BIGDATA_HPP

#include <array>
#include <cstdint>
#include <algorithm> 

constexpr std::uint32_t kSlote1000MsCount = 150;
constexpr std::uint32_t kSlote100MsCount = 200;
constexpr std::uint32_t kSlote50MsCount = 200;

struct BigData {
    std::array<std::uint32_t, kSlote1000MsCount> slot_1000Ms;
    std::array<std::uint32_t, kSlote100MsCount> slot_100Ms;
    std::array<std::uint32_t, kSlote50MsCount> slot_50Ms;

    BigData() noexcept {
        std::fill(slot_1000Ms.begin(), slot_1000Ms.end(), 0);
        std::fill(slot_100Ms.begin(), slot_100Ms.end(), 0);
        std::fill(slot_50Ms.begin(), slot_50Ms.end(), 0);
    }
    ~BigData() = default;

    BigData(const BigData&) = default;
    BigData(BigData&&) noexcept = default;
    BigData& operator=(const BigData&) = default;
    BigData& operator=(BigData&&) noexcept = default;
};

#endif // BIGDATA_HPP