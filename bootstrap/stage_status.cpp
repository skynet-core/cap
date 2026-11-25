module;

#include <string>

#include <boost/json.hpp>
#include <boost/json/basic_parser_impl.hpp>
#include <boost/version.hpp>

module cap.bootstrap.stage_status;

namespace cap::bootstrap {

namespace {
struct member_counter {
  static constexpr std::size_t max_array_size = -1;
  static constexpr std::size_t max_object_size = -1;
  static constexpr std::size_t max_string_size = -1;
  static constexpr std::size_t max_key_size = -1;
  std::size_t count = 0;

  bool on_object_end(std::size_t n, boost::system::error_code &) {
    count = n;
    return true;
  }
  template <typename... T> bool on_document_begin(T &&...) { return true; }
  template <typename... T> bool on_document_end(T &&...) { return true; }
  template <typename... T> bool on_object_begin(T &&...) { return true; }
  template <typename... T> bool on_array_begin(T &&...) { return true; }
  template <typename... T> bool on_array_end(T &&...) { return true; }
  bool on_key_part(boost::json::string_view, std::size_t,
                   boost::system::error_code &) {
    return true;
  }
  bool on_key(boost::json::string_view, std::size_t,
              boost::system::error_code &) {
    return true;
  }
  bool on_string_part(boost::json::string_view, std::size_t,
                      boost::system::error_code &) {
    return true;
  }
  bool on_string(boost::json::string_view, std::size_t,
                 boost::system::error_code &) {
    return true;
  }
  bool on_number_part(boost::json::string_view, boost::system::error_code &) {
    return true;
  }
  bool on_int64(std::int64_t, boost::json::string_view,
                boost::system::error_code &) {
    return true;
  }
  bool on_uint64(std::uint64_t, boost::json::string_view,
                 boost::system::error_code &) {
    return true;
  }
  bool on_double(double, boost::json::string_view,
                 boost::system::error_code &) {
    return true;
  }
  template <typename... T> bool on_bool(T &&...) { return true; }
  template <typename... T> bool on_null(T &&...) { return true; }
  bool on_comment_part(boost::json::string_view, boost::system::error_code &) {
    return true;
  }
  bool on_comment(boost::json::string_view, boost::system::error_code &) {
    return true;
  }
};
} // namespace

bool valid_stage_status(const std::string &output) {
  boost::system::error_code ec;
  const auto value = boost::json::parse(output, ec);
  if (ec || !value.is_object())
    return false;
  const auto &object = value.as_object();
  if (object.size() != 4)
    return false;
  // The root object's end event is last and counts members before DOM collapse.
  boost::json::basic_parser<member_counter> parser({});
  parser.write_some(false, output.data(), output.size(), ec);
  if (ec || parser.handler().count != object.size())
    return false;
  const auto *stage = object.if_contains("stage");
  const auto *core = object.if_contains("cap_core");
  const auto *boost = object.if_contains("boost");
  const auto *check = object.if_contains("self_check");
  return stage && stage->is_int64() && stage->as_int64() == 1 && core &&
         core->is_string() && core->as_string() == "ok" && boost &&
         boost->is_string() && boost->as_string() == BOOST_LIB_VERSION &&
         check && check->is_string() && check->as_string() == "passed";
}

} // namespace cap::bootstrap
