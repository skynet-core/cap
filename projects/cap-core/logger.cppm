// Callers must redact sensitive diagnostic fields before emit; ILogger cannot
// infer secrets in arbitrary strings. CaptureLogger performs no redaction.
module;

#include <string>
#include <vector>

export module cap.core.logger;

import cap.core.diagnostic;

namespace cap::core {

export class ILogger {
public:
  virtual ~ILogger() = default;
  virtual void emit(const Diagnostic &diagnostic) = 0;
};

// Deterministic in-memory sink; also the stage-1 bootstrap's logger.
export class CaptureLogger final : public ILogger {
public:
  void emit(const Diagnostic &diagnostic) override;
  const std::vector<std::string> &lines() const;

private:
  std::vector<std::string> lines_;
};

void CaptureLogger::emit(const Diagnostic &diagnostic) {
  lines_.push_back(diagnostic.format());
}

const std::vector<std::string> &CaptureLogger::lines() const { return lines_; }

} // namespace cap::core
