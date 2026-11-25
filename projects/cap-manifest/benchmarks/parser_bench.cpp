#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

import cap.manifest;

int main(int argc, char **argv) {
  if (argc != 2)
    return 1;
  std::ifstream in(argv[1]);
  if (!in)
    return 1;
  const std::string bytes{std::istreambuf_iterator<char>(in), {}};
  std::vector<double> samples;
  std::size_t count = 0;
  for (int run = 0; run != 6; ++run) {
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i != 10000; ++i) {
      auto result = cap::manifest::parse_manifest(bytes, argv[1]);
      if (!result)
        return 2;
      count += result->bins.size();
    }
    auto end = std::chrono::steady_clock::now();
    if (run)
      samples.push_back(
          std::chrono::duration<double, std::micro>(end - start).count() /
          10000);
  }
  std::sort(samples.begin(), samples.end());
  std::cout << "us/parse median=" << samples[2] << " range=" << samples.front()
            << ".." << samples.back() << " bins=" << count << '\n';
}
