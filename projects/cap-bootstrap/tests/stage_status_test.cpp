#include <cassert>
#include <string>

#include <boost/version.hpp>

import cap.bootstrap.stage_status;

int main() {
  using cap::bootstrap::valid_stage_status;
  const std::string valid =
      R"({"stage":1,"cap_core":"ok","boost":")" BOOST_LIB_VERSION
      R"(","self_check":"passed"})";
  assert(valid_stage_status(valid));
  assert(valid_stage_status(valid + "\n"));
  assert(
      valid_stage_status(" \t\n"
                         R"({"self_check":"passed","boost":")" BOOST_LIB_VERSION
                         R"(","cap_core":"ok","stage":1})"
                         " \r\n"));
  assert(!valid_stage_status(
      R"({"stage":2,"stage":1,"cap_core":"ok","boost":")" BOOST_LIB_VERSION
      R"(","self_check":"passed"})"));
  assert(!valid_stage_status(
      R"({"stage":1,"stage":2,"cap_core":"ok","boost":")" BOOST_LIB_VERSION
      R"(","self_check":"passed"})"));
  assert(!valid_stage_status(
      R"({"stage":1,"stage":1,"cap_core":"ok","boost":")" BOOST_LIB_VERSION
      R"(","self_check":"passed"})"));
  assert(!valid_stage_status(
      R"({"stage":1,"\u0073tage":1,"cap_core":"ok","boost":")" BOOST_LIB_VERSION
      R"(","self_check":"passed"})"));
  assert(!valid_stage_status(
      R"({"stage":1,"cap_core":"ok","boost":")" BOOST_LIB_VERSION
      R"(","self_check":"passed","self_check":"passed"})"));
  assert(!valid_stage_status(R"({"stage":1 broken)"));
  assert(!valid_stage_status(valid + " trailing"));
  assert(!valid_stage_status(valid + "{}"));
  assert(!valid_stage_status("[]"));
  assert(!valid_stage_status(
      R"({"stage":"1","cap_core":"ok","boost":")" BOOST_LIB_VERSION
      R"(","self_check":"passed"})"));
  assert(!valid_stage_status(
      R"({"stage":1.0,"cap_core":"ok","boost":")" BOOST_LIB_VERSION
      R"(","self_check":"passed"})"));
  assert(!valid_stage_status(
      R"({"stage":2,"cap_core":"ok","boost":")" BOOST_LIB_VERSION
      R"(","self_check":"passed"})"));
  assert(!valid_stage_status(
      R"({"stage":1,"cap_core":true,"boost":")" BOOST_LIB_VERSION
      R"(","self_check":"passed"})"));
  assert(!valid_stage_status(
      R"({"stage":1,"cap_core":"bad","boost":")" BOOST_LIB_VERSION
      R"(","self_check":"passed"})"));
  assert(!valid_stage_status(
      R"({"stage":1,"cap_core":"ok","boost":"wrong","self_check":"passed"})"));
  assert(!valid_stage_status(
      R"({"stage":1,"cap_core":"ok","boost":")" BOOST_LIB_VERSION
      R"(","self_check":"failed"})"));
  assert(!valid_stage_status(
      R"({"stage":1,"cap_core":"ok","boost":")" BOOST_LIB_VERSION
      R"(","self_check":"passed","extra":1})"));
  assert(!valid_stage_status(
      R"({"stage":1,"cap_core":"ok","boost":")" BOOST_LIB_VERSION
      R"(","other":"passed"})"));
}
