module;
#include <chrono>

export module amp.core.clock;

export namespace amp::core {

class IClock {
public:
  virtual ~IClock() = default;
  virtual std::chrono::steady_clock::time_point monotonic_now() const = 0;
};

class SystemClock final : public IClock {
public:
  std::chrono::steady_clock::time_point monotonic_now() const override {
    return std::chrono::steady_clock::now();
  }
};

} // namespace amp::core
