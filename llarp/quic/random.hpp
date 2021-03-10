#pragma once

// TODO: replace with llarp

#include <algorithm>
#include <random>
#include <cstring>
#include <cstddef>

template <typename Gen>
void
random_bytes(void* dest, size_t length, Gen&& rng)
{
  using RNG = std::remove_reference_t<Gen>;
  using UInt = typename RNG::result_type;
  static_assert(std::is_same_v<UInt, uint32_t> || std::is_same_v<UInt, uint64_t>);
  static_assert(RNG::min() == 0 && RNG::max() == std::numeric_limits<UInt>::max());
  auto* d = reinterpret_cast<std::byte*>(dest);
  for (size_t o = 0; o < length; o += sizeof(UInt))
  {
    UInt x = rng();
    std::memcpy(d + o, &x, std::min(sizeof(UInt), length - o));
  }
}

// Returns an RNG with a fully seeded state from std::random_device
template <typename RNG>
RNG
seeded()
{
  constexpr size_t rd_draws =
      ((RNG::state_size * sizeof(typename RNG::result_type) - 1) / sizeof(unsigned int) + 1);
  std::array<unsigned int, rd_draws> seed_data;
  std::generate(seed_data.begin(), seed_data.end(), std::random_device{});
  std::seed_seq seed(seed_data.begin(), seed_data.end());
  return RNG{seed};
}
