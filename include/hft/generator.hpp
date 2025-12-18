#pragma once

#include "hft/event.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace hft {

// Deterministic (platform-stable) event generator.
// - seed: controls RNG
// - n_events: number of generated events AFTER the initial book seeding
// - mid_px: center price in ticks
// - spread_ticks: max distance from mid for passive orders
std::vector<Event> generate_events_gen_v1(std::uint64_t seed,
                                         std::size_t n_events,
                                         std::int64_t mid_px = 101,
                                         std::int64_t spread_ticks = 5);

}  // namespace hft

