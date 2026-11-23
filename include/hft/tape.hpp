// CSV tape reader and writer for the versioned event format.
#pragma once

#include "hft/event.hpp"

#include <string>
#include <vector>

namespace hft {

// CSV tape format (v1):
//   #hft_tape_v1
//   ts,seq,type,order_id,side,price_ticks,qty,cancel_id
// type: AddLimit | Cancel
// side: Buy | Sell
//
// Returns true on success; on failure returns false and (if provided) sets *err.
bool write_events_csv(const std::string& path,
                      const std::vector<Event>& events,
                      std::string* err = nullptr);

// Returns parsed events; on failure returns empty vector and (if provided) sets *err.
// Lines starting with '#' are ignored.
std::vector<Event> read_events_csv(const std::string& path,
                                  std::string* err = nullptr);

}  // namespace hft

