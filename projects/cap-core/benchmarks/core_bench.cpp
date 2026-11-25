#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

import cap.core.digest;
import cap.core.process;

namespace {
using Clock = std::chrono::steady_clock;
constexpr int warmups = 2;
constexpr int samples = 9;
constexpr std::size_t payload_size = 1024 * 1024;

template <class F> std::vector<double> measure(F &&operation) {
  for (int i = 0; i < warmups; ++i)
    operation();
  std::vector<double> times;
  for (int i = 0; i < samples; ++i) {
    const auto start = Clock::now();
    operation();
    times.push_back(
        std::chrono::duration<double, std::milli>(Clock::now() - start)
            .count());
  }
  return times;
}

std::pair<double, double> median_mad(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  const double median = values[values.size() / 2];
  for (double &value : values)
    value = std::abs(value - median);
  std::sort(values.begin(), values.end());
  return {median, values[values.size() / 2]};
}
} // namespace

int main() {
  std::vector<std::byte> payload(payload_size);
  constexpr std::string_view pattern = "cap-core benchmark payload\n";
  for (std::size_t i = 0; i < payload.size(); ++i)
    payload[i] = static_cast<std::byte>(pattern[i % pattern.size()]);

  const auto expected = cap::core::Digest::parse(
      "34b23c9dfd077c6885be2daeaa600398ec5b18810f6cfe243da20c1f352d551a");
  if (!expected || cap::core::Digest::from_bytes(payload) != *expected) {
    std::cerr << "digest known-answer check failed\n";
    return 1;
  }
  const auto digest_times = measure([&] {
    if (cap::core::Digest::from_bytes(payload) != *expected)
      std::abort();
  });
  std::vector<double> digest_throughputs;
  digest_throughputs.reserve(digest_times.size());
  for (const double milliseconds : digest_times)
    digest_throughputs.push_back(1000.0 / milliseconds);
  const auto [throughput_median, throughput_mad] =
      median_mad(std::move(digest_throughputs));
  std::cout << "digest: payload_bytes=" << payload_size
            << " warmups=" << warmups << " samples=" << samples
            << " median_MiB_s=" << throughput_median
            << " MAD_MiB_s=" << throughput_mad << '\n';

  cap::core::LocalProcessRunner runner;
  cap::core::ProcessSpec spec;
  spec.argv = {"/usr/bin/true"};
  const auto run_child = [&] {
    const auto result = runner.run(spec);
    if (!result || result->exit_code != 0 || result->timed_out)
      std::abort();
  };
  run_child();
  const auto process_times = measure(run_child);
  const auto [process_median, process_mad] = median_mad(process_times);
  std::cout << "process: executable=/usr/bin/true argv_count=1 warmups="
            << warmups << " samples=" << samples
            << " median_ms=" << process_median << " MAD_ms=" << process_mad
            << '\n';
}
