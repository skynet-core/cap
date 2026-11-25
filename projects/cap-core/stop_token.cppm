module;
#include <atomic>

export module cap.core.stop_token;

export namespace cap::core {

class StopToken {
public:
  bool is_stop_requested() const noexcept {
    return stopped_.load(std::memory_order_acquire);
  }
  void request_stop() noexcept {
    stopped_.store(true, std::memory_order_release);
  }

private:
  std::atomic<bool> stopped_{false};
};

} // namespace cap::core
