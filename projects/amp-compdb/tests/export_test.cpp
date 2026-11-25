#include <cassert>
#include <string>
#include <vector>

import amp.build.actions;
import amp.compdb;

/// Exercises JSON projection, filtering, validation, UTF-8 handling, and
/// escaping using planner-produced compile and link actions.
/// @note Takes no parameters; the test uses fixed in-process inputs.
/// @return int — Returns zero when all assertions pass; an assertion failure
/// terminates the test unsuccessfully.
/// @throws std::bad_alloc — If test setup or serialization exhausts memory.
/// @throws std::length_error — If test setup or serialization exceeds a
/// standard string or vector maximum size.
int main() {
  auto planned = amp::build::plan_compile({"/usr/bin/c++",
                                           "/tmp/build",
                                           "/tmp/src space/quo\"te.cpp",
                                           "/tmp/build/out.o",
                                           "c++26",
                                           {},
                                           false});
  assert(planned);
  auto link = amp::build::plan_link({"/usr/bin/c++",
                                     "/tmp/build",
                                     "/tmp/build/app",
                                     {"/tmp/build/out.o"},
                                     {},
                                     {}});
  assert(link);
  auto second = amp::build::plan_compile({"/usr/bin/c++",
                                          "/tmp/build",
                                          "/tmp/second.cpp",
                                          "/tmp/build/second.o",
                                          "c++26",
                                          {},
                                          false});
  assert(second);
  second->arguments.push_back("");
  auto serialized = amp::compdb::to_json({*planned, *link, *second});
  assert(serialized &&
         *serialized ==
             "[{\"directory\":\"/tmp/build\",\"file\":\"/tmp/src "
             "space/quo\\\"te.cpp\",\"arguments\":[\"/usr/bin/"
             "c++\",\"-std=c++26\",\"-c\",\"/tmp/src "
             "space/quo\\\"te.cpp\",\"-o\",\"/tmp/build/out.o\"],\"output\":\"/"
             "tmp/build/out.o\"},{\"directory\":\"/tmp/build\",\"file\":\"/tmp/"
             "second.cpp\",\"arguments\":[\"/usr/bin/"
             "c++\",\"-std=c++26\",\"-c\",\"/tmp/second.cpp\",\"-o\",\"/tmp/"
             "build/second.o\",\"\"],\"output\":\"/tmp/build/second.o\"}]");
  assert(*amp::compdb::to_json({*link}) == "[]");
  assert(*amp::compdb::to_json({}) == "[]");
  auto malformed = *planned;
  malformed.inputs.clear();
  assert(!amp::compdb::to_json({malformed}));
  malformed = *planned;
  malformed.arguments.clear();
  assert(!amp::compdb::to_json({malformed}));
  malformed = *planned;
  malformed.directory = "relative";
  assert(!amp::compdb::to_json({malformed}));
  malformed = *planned;
  malformed.arguments.front() = "compiler";
  assert(!amp::compdb::to_json({malformed}));
  for (const std::string &bad :
       {std::string(), std::string("relative"), std::string("/bad\0x", 6),
        std::string("/bad/\xff", 6)}) {
    malformed = *planned;
    malformed.directory = bad;
    assert(!amp::compdb::to_json({malformed}));
    malformed = *planned;
    malformed.inputs.front() = bad;
    assert(!amp::compdb::to_json({malformed}));
    malformed = *planned;
    malformed.outputs.front() = bad;
    assert(!amp::compdb::to_json({malformed}));
    malformed = *planned;
    malformed.arguments.front() = bad;
    assert(!amp::compdb::to_json({malformed}));
  }
  malformed = *planned;
  malformed.outputs.clear();
  assert(!amp::compdb::to_json({malformed}));
  malformed = *planned;
  malformed.arguments[1] = std::string("a\0b", 3);
  assert(!amp::compdb::to_json({malformed}));

  for (const std::string &invalid_utf8 :
       {std::string("\x80", 1), std::string("\xc0\x80", 2),
        std::string("\xe0\x80\x80", 3), std::string("\xed\xa0\x80", 3),
        std::string("\xf4\x90\x80\x80", 4), std::string("\xf0\x9f", 2),
        std::string("\xe2\x82\x41", 3), std::string("\xf0\x9f\x92\xff", 4)}) {
    malformed = *planned;
    malformed.arguments[1] = invalid_utf8;
    assert(!amp::compdb::to_json({malformed}));
  }
  malformed = *planned;
  malformed.arguments[1] = "caf\xc3\xa9";
  auto utf8_argument = amp::compdb::to_json({malformed});
  assert(utf8_argument &&
         utf8_argument->find("caf\xc3\xa9") != std::string::npos);
  malformed.arguments[1] = "\xe2\x82\xac\xf0\x9f\x98\x80";
  auto multibyte = amp::compdb::to_json({malformed});
  assert(multibyte &&
         multibyte->find(malformed.arguments[1]) != std::string::npos);
  malformed.arguments[1] = "\"\\\b\f\n\r\t\x01\x1f";
  auto escaped = amp::compdb::to_json({malformed});
  assert(escaped &&
         escaped->find("\"\\\"\\\\\\b\\f\\n\\r\\t\\u0001\\u001f\"") !=
             std::string::npos);
  malformed = *planned;
  malformed.inputs.front() = std::string("/bad/\xff", 6);
  assert(!amp::compdb::to_json({malformed}));
  malformed = *planned;
  malformed.arguments[1] = "";
  auto empty_argument = amp::compdb::to_json({malformed});
  assert(empty_argument &&
         empty_argument->find("\"/usr/bin/c++\",\"\"") != std::string::npos);
}
