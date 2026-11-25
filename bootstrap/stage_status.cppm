module;

#include <string>

export module cap.bootstrap.stage_status;

export namespace cap::bootstrap {

bool valid_stage_status(const std::string &output);

} // namespace cap::bootstrap
