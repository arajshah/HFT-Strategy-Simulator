#include "hft/tape.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace hft {
namespace {

static std::vector<std::string> split_csv_line(const std::string& line) {
  // Minimal CSV splitter (no quoted fields needed for our v1 schema).
  std::vector<std::string> out;
  std::string cur;
  for (char c : line) {
    if (c == ',') {
      out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  out.push_back(cur);
  return out;
}

static std::string side_to_string(Side s) {
  return (s == Side::Buy) ? "Buy" : "Sell";
}

static bool side_from_string(const std::string& s, Side& out) {
  if (s == "Buy") { out = Side::Buy; return true; }
  if (s == "Sell") { out = Side::Sell; return true; }
  return false;
}

static std::string type_to_string(EventType t) {
  return (t == EventType::AddLimit) ? "AddLimit" : "Cancel";
}

static bool type_from_string(const std::string& s, EventType& out) {
  if (s == "AddLimit") { out = EventType::AddLimit; return true; }
  if (s == "Cancel") { out = EventType::Cancel; return true; }
  return false;
}

template <typename T>
static bool parse_u64(const std::string& s, T& out) {
  // Only for non-negative integers
  if (s.empty()) return false;
  std::uint64_t v = 0;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
    v = v * 10 + static_cast<std::uint64_t>(c - '0');
  }
  out = static_cast<T>(v);
  return true;
}

static bool parse_i64(const std::string& s, std::int64_t& out) {
  if (s.empty()) return false;
  std::size_t i = 0;
  bool neg = false;
  if (s[0] == '-') { neg = true; i = 1; }
  if (i >= s.size()) return false;

  std::uint64_t v = 0;
  for (; i < s.size(); ++i) {
    char c = s[i];
    if (c < '0' || c > '9') return false;
    v = v * 10 + static_cast<std::uint64_t>(c - '0');
  }
  out = neg ? -static_cast<std::int64_t>(v) : static_cast<std::int64_t>(v);
  return true;
}

}  // namespace

bool write_events_csv(const std::string& path,
                      const std::vector<Event>& events,
                      std::string* err) {
  try {
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
      std::filesystem::create_directories(p.parent_path());
    }

    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs.is_open()) {
      if (err) *err = "Failed to open for write: " + path;
      return false;
    }

    ofs << "#hft_tape_v1\n";
    ofs << "ts,seq,type,order_id,side,price_ticks,qty,cancel_id\n";

    for (const auto& e : events) {
      // For Cancel, order fields are still written (as zeros/Buy) but ignored on read.
      ofs << e.ts << ","
          << e.seq << ","
          << type_to_string(e.type) << ","
          << e.order.id << ","
          << side_to_string(e.order.side) << ","
          << e.order.price_ticks << ","
          << e.order.qty << ","
          << e.cancel_id
          << "\n";
    }

    return true;
  } catch (const std::exception& ex) {
    if (err) *err = std::string("Exception in write_events_csv: ") + ex.what();
    return false;
  }
}

std::vector<Event> read_events_csv(const std::string& path,
                                  std::string* err) {
  std::vector<Event> out;

  try {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
      if (err) *err = "Failed to open for read: " + path;
      return {};
    }

    std::string line;
    while (std::getline(ifs, line)) {
      if (line.empty()) continue;
      if (!line.empty() && line[0] == '#') continue;
      if (line.rfind("ts,seq,type", 0) == 0) continue;  // header row

      const auto cols = split_csv_line(line);
      if (cols.size() != 8) {
        if (err) *err = "Bad column count (expected 8): " + line;
        return {};
      }

      Event e{};
      if (!parse_u64(cols[0], e.ts)) { if (err) *err = "Bad ts: " + cols[0]; return {}; }
      if (!parse_u64(cols[1], e.seq)) { if (err) *err = "Bad seq: " + cols[1]; return {}; }

      if (!type_from_string(cols[2], e.type)) {
        if (err) *err = "Bad type: " + cols[2];
        return {};
      }

      // Order fields
      if (!parse_u64(cols[3], e.order.id)) { if (err) *err = "Bad order_id: " + cols[3]; return {}; }

      Side side{};
      if (!side_from_string(cols[4], side)) { if (err) *err = "Bad side: " + cols[4]; return {}; }
      e.order.side = side;

      if (!parse_i64(cols[5], e.order.price_ticks)) { if (err) *err = "Bad price_ticks: " + cols[5]; return {}; }
      if (!parse_i64(cols[6], e.order.qty)) { if (err) *err = "Bad qty: " + cols[6]; return {}; }

      if (!parse_u64(cols[7], e.cancel_id)) { if (err) *err = "Bad cancel_id: " + cols[7]; return {}; }

      // For Cancel, interpret cancel_id; ignore order payload at runtime.
      out.push_back(e);
    }

    return out;
  } catch (const std::exception& ex) {
    if (err) *err = std::string("Exception in read_events_csv: ") + ex.what();
    return {};
  }
}

}  // namespace hft

