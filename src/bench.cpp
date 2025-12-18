#include "hft/bench.hpp"

#include "hft/engine.hpp"
#include "hft/pipeline.hpp"

#include <chrono>

namespace hft {

BenchResult bench_run(std::vector<Event> events) {
  BenchResult out;
  out.n_events = events.size();

  for (const auto& e : events) {
    if (e.type == EventType::AddLimit) ++out.n_add;
    else if (e.type == EventType::Cancel) ++out.n_cancel;
  }

  Engine eng;

  const auto t0 = std::chrono::steady_clock::now();
  const auto rr = eng.run(std::move(events));
  const auto t1 = std::chrono::steady_clock::now();

  out.n_fills = rr.fills.size();
  for (const auto& f : rr.fills) out.traded_qty += f.qty;

  const std::chrono::duration<double> dt = t1 - t0;
  out.seconds = dt.count();

  if (out.seconds > 0.0) {
    out.events_per_sec = static_cast<double>(out.n_events) / out.seconds;
    out.fills_per_sec = static_cast<double>(out.n_fills) / out.seconds;
  }
  if (out.n_events > 0) {
    out.ns_per_event = (out.seconds * 1e9) / static_cast<double>(out.n_events);
  }

  return out;
}

BenchResult bench_run_pipeline(std::vector<Event> events) {
  BenchResult out;
  out.n_events = events.size();

  for (const auto& e : events) {
    if (e.type == EventType::AddLimit) ++out.n_add;
    else if (e.type == EventType::Cancel) ++out.n_cancel;
  }

  const auto t0 = std::chrono::steady_clock::now();
  const auto rr = pipeline_run(std::move(events));
  const auto t1 = std::chrono::steady_clock::now();

  out.n_fills = rr.fills.size();
  for (const auto& f : rr.fills) out.traded_qty += f.qty;

  const std::chrono::duration<double> dt = t1 - t0;
  out.seconds = dt.count();

  if (out.seconds > 0.0) {
    out.events_per_sec = static_cast<double>(out.n_events) / out.seconds;
    out.fills_per_sec = static_cast<double>(out.n_fills) / out.seconds;
  }
  if (out.n_events > 0) {
    out.ns_per_event = (out.seconds * 1e9) / static_cast<double>(out.n_events);
  }

  return out;
}

}  // namespace hft

