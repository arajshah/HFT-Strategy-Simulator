#pragma once

#include "hft/engine.hpp"  // RunResult
#include "hft/event.hpp"

#include <vector>

namespace hft {

// Deterministic pipeline run:
// 1) sort events by (ts, seq)
// 2) producer enqueues in sorted order
// 3) consumer dequeues and applies to a single OrderBook (single writer)
// 4) returns fills + final top snapshot
RunResult pipeline_run(std::vector<Event> events);

}  // namespace hft

