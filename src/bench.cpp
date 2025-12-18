#include "hft/bench.hpp"

#include "hft/engine.hpp"
#include "hft/pipeline.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>

namespace hft {

BenchResult bench_run(std::vector<Event> events) {
  BenchResult out;
  out.n_events = events.size();

  for (const auto& e : events) {
    if (e.type == EventType::AddLimit) ++out.n_add;
    else if (e.type == EventType::Cancel) ++out.n_cancel;
  }

  const auto s0 = std::chrono::steady_clock::now();
  std::sort(events.begin(), events.end(),
            [](const Event& a, const Event& b) {
              if (a.ts != b.ts) return a.ts < b.ts;
              return a.seq < b.seq;
            });
  const auto s1 = std::chrono::steady_clock::now();

  Engine eng;

  const auto t0 = std::chrono::steady_clock::now();
  const auto rr = eng.run(std::move(events));
  const auto t1 = std::chrono::steady_clock::now();

  out.n_fills = rr.fills.size();
  for (const auto& f : rr.fills) out.traded_qty += f.qty;

  out.sort_seconds = std::chrono::duration<double>(s1 - s0).count();
  out.run_seconds = std::chrono::duration<double>(t1 - t0).count();
  out.seconds = out.sort_seconds + out.run_seconds;

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

  const auto s0 = std::chrono::steady_clock::now();
  std::sort(events.begin(), events.end(),
            [](const Event& a, const Event& b) {
              if (a.ts != b.ts) return a.ts < b.ts;
              return a.seq < b.seq;
            });
  const auto s1 = std::chrono::steady_clock::now();

  const auto t0 = std::chrono::steady_clock::now();
  const auto rr = pipeline_run(std::move(events));
  const auto t1 = std::chrono::steady_clock::now();

  out.n_fills = rr.fills.size();
  for (const auto& f : rr.fills) out.traded_qty += f.qty;

  out.sort_seconds = std::chrono::duration<double>(s1 - s0).count();
  out.run_seconds = std::chrono::duration<double>(t1 - t0).count();
  out.seconds = out.sort_seconds + out.run_seconds;

  if (out.seconds > 0.0) {
    out.events_per_sec = static_cast<double>(out.n_events) / out.seconds;
    out.fills_per_sec = static_cast<double>(out.n_fills) / out.seconds;
  }
  if (out.n_events > 0) {
    out.ns_per_event = (out.seconds * 1e9) / static_cast<double>(out.n_events);
  }

  return out;
}

BenchSummary bench_repeat(std::vector<Event> events,
                          bool pipeline,
                          std::size_t iters,
                          bool breakdown) {
  BenchSummary s;
  s.iters = iters;
  s.n_events = events.size();

  std::vector<double> eps;
  std::vector<double> ns;
  eps.reserve(iters);
  ns.reserve(iters);

  for (std::size_t i = 0; i < iters; ++i) {
    // Copy events each iteration to keep identical input.
    auto ev = events;

    const auto br = pipeline ? bench_run_pipeline(std::move(ev))
                             : bench_run(std::move(ev));

    eps.push_back(br.events_per_sec);
    ns.push_back(br.ns_per_event);

    if (breakdown) {
      std::cout << "BENCH_ITER " << i
                << " SORT_S " << br.sort_seconds
                << " RUN_S " << br.run_seconds
                << " TOTAL_S " << br.seconds
                << " EPS " << br.events_per_sec
                << " NS_PER_EVENT " << br.ns_per_event
                << "\n";
    }
  }

  auto median_of = [](std::vector<double>& v) -> double {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t mid = v.size() / 2;
    if (v.size() % 2 == 1) return v[mid];
    return 0.5 * (v[mid - 1] + v[mid]);
  };

  auto minmax_of = [](const std::vector<double>& v) -> std::pair<double, double> {
    if (v.empty()) return {0.0, 0.0};
    auto mm = std::minmax_element(v.begin(), v.end());
    return {*mm.first, *mm.second};
  };

  auto eps_copy = eps;
  auto ns_copy = ns;

  const auto [eps_min, eps_max] = minmax_of(eps);
  const auto [ns_min, ns_max] = minmax_of(ns);

  s.eps_min = eps_min;
  s.eps_max = eps_max;
  s.eps_median = median_of(eps_copy);

  s.ns_min = ns_min;
  s.ns_max = ns_max;
  s.ns_median = median_of(ns_copy);

  return s;
}

}  // namespace hft

