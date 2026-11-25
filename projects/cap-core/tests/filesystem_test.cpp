// Contract test for cap-core filesystem (M0/T02).
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "test_util.hpp"

import cap.core.diagnostic;
import cap.core.filesystem;

using cap::core::LocalFileSystem;

/// Checks filesystem read, write, enumeration, canonicalization, and failure
/// contracts, including bounded rejection of a FIFO and symlink behavior.
/// @note Takes no arguments.
/// @return int — 0 when all expected behaviors pass; 1 when scratch setup or
///         any contract check fails.
/// @throws std::filesystem::filesystem_error if a throwing filesystem setup,
///         inspection, permission, or cleanup operation fails.
/// @throws std::bad_alloc if test path, payload, or result storage allocation
///         fails.
/// @throws std::length_error if test storage exceeds its maximum size.
int main() {
  LocalFileSystem fsx;
  std::filesystem::path scratch =
      std::filesystem::temp_directory_path() /
      ("cap-core-fs-test-" + std::to_string(::getpid()));
  std::error_code rm_ec;
  std::filesystem::remove_all(scratch, rm_ec);
  if (!std::filesystem::create_directories(scratch)) {
    std::cerr << "FAIL: cannot create scratch directory " << scratch << "\n";
    return 1;
  }

  const std::vector<std::byte> data = {std::byte{1}, std::byte{2}, std::byte{3},
                                       std::byte{4}, std::byte{5}};

  // The first generated temp name is predictable; a collision must be
  // untouched.
  const auto collision_dir = scratch / "collision";
  std::filesystem::create_directory(collision_dir);
  const auto victim = collision_dir / "victim";
  const auto target = collision_dir / "output";
  const auto temp =
      collision_dir / ("output.cap-tmp-" + std::to_string(::getpid()) + "-0");
  std::ofstream(victim) << "inert scratch content";
  std::filesystem::create_symlink(victim.filename(), temp);
  auto collision = fsx.write_atomic(target.string(), data);
  EXPECT_TRUE(!collision);
  if (!collision) {
    EXPECT_EQ(cap::core::error_code_name(collision.error().code),
              "transient-io");
    EXPECT_TRUE(collision.error().message.find("temp open failed") !=
                std::string::npos);
  }
  EXPECT_TRUE(std::filesystem::is_symlink(temp));
  if (std::filesystem::is_symlink(temp))
    EXPECT_TRUE(std::filesystem::read_symlink(temp) == victim.filename());
  std::ifstream victim_in(victim);
  std::string victim_content((std::istreambuf_iterator<char>(victim_in)), {});
  EXPECT_EQ(victim_content, "inert scratch content");
  EXPECT_TRUE(!std::filesystem::exists(target));
  std::filesystem::remove_all(collision_dir);

  // write_atomic + read round-trip
  const std::string a = (scratch / "a.bin").string();
  EXPECT_TRUE(static_cast<bool>(fsx.write_atomic(a, data)));
  auto rd = fsx.read(a);
  EXPECT_TRUE(static_cast<bool>(rd));
  if (rd) {
    EXPECT_EQ(rd->size(), data.size());
    EXPECT_TRUE(*rd == data);
  }
  const auto regular_link = scratch / "regular-link";
  std::filesystem::create_symlink("a.bin", regular_link);
  auto linked_read = fsx.read(regular_link.string());
  EXPECT_TRUE(static_cast<bool>(linked_read));
  if (linked_read)
    EXPECT_TRUE(*linked_read == data);
  std::filesystem::remove(regular_link);

  const auto fifo = scratch / "manifest-fifo";
  EXPECT_EQ(::mkfifo(fifo.c_str(), 0600), 0);
  const pid_t fifo_reader = ::fork();
  if (fifo_reader == 0) {
    ::alarm(2);
    const auto result = fsx.read(fifo.string());
    _exit(!result && result.error().code ==
                         cap::core::ErrorCode::unsupported_capability
              ? 0
              : 1);
  }
  int fifo_status = 0;
  if (fifo_reader > 0) {
    EXPECT_EQ(::waitpid(fifo_reader, &fifo_status, 0), fifo_reader);
    EXPECT_TRUE(WIFEXITED(fifo_status));
    if (WIFEXITED(fifo_status))
      EXPECT_EQ(WEXITSTATUS(fifo_status), 0);
  } else {
    EXPECT_TRUE(false);
  }
  std::filesystem::remove(fifo);

  // Replacing a longer file truncates the old payload and leaves no temp entry.
  const std::vector<std::byte> shorter = {std::byte{9}, std::byte{8}};
  EXPECT_TRUE(static_cast<bool>(fsx.write_atomic(a, shorter)));
  auto replaced = fsx.read(a);
  EXPECT_TRUE(static_cast<bool>(replaced));
  if (replaced)
    EXPECT_TRUE(*replaced == shorter);
  auto after_replace = fsx.enumerate(scratch.string());
  EXPECT_TRUE(static_cast<bool>(after_replace));
  if (after_replace) {
    EXPECT_EQ(after_replace->size(), 1u);
    if (after_replace->size() == 1)
      EXPECT_EQ((*after_replace)[0], "a.bin");
  }

  // missing file / directory read / missing parent
  auto missing = fsx.read((scratch / "nope").string());
  EXPECT_TRUE(!missing);
  EXPECT_EQ(cap::core::error_code_name(missing.error().code),
            "missing-resource");
  auto child_of_file = fsx.read((scratch / "a.bin" / "child").string());
  EXPECT_TRUE(!child_of_file);
  EXPECT_EQ(cap::core::error_code_name(child_of_file.error().code),
            "missing-resource");
  const auto dangling_link = scratch / "dangling-regular-link";
  std::filesystem::create_symlink("missing-regular-target", dangling_link);
  auto dangling_read = fsx.read(dangling_link.string());
  EXPECT_TRUE(!dangling_read);
  EXPECT_EQ(cap::core::error_code_name(dangling_read.error().code),
            "missing-resource");
  std::filesystem::remove(dangling_link);
  auto dir_read = fsx.read(scratch.string());
  EXPECT_TRUE(!dir_read);
  EXPECT_EQ(cap::core::error_code_name(dir_read.error().code),
            "unsupported-capability");
  auto bad_parent =
      fsx.write_atomic((scratch / "sub" / "nope.bin").string(), data);
  EXPECT_TRUE(!bad_parent);
  EXPECT_EQ(cap::core::error_code_name(bad_parent.error().code),
            "missing-resource");

  // relative path (empty parent directory branch)
  std::error_code cwd_ec;
  const std::filesystem::path old_cwd = std::filesystem::current_path(cwd_ec);
  std::filesystem::current_path(scratch, cwd_ec);
  auto rel_write = fsx.write_atomic("rel.bin", data);
  EXPECT_TRUE(static_cast<bool>(rel_write));
  auto rel_read = fsx.read("rel.bin");
  EXPECT_TRUE(static_cast<bool>(rel_read));
  if (rel_read)
    EXPECT_EQ(rel_read->size(), data.size());
  std::filesystem::current_path(old_cwd, cwd_ec);

  // enumerate: deterministic lexicographic order
  EXPECT_TRUE(
      static_cast<bool>(fsx.write_atomic((scratch / "b.txt").string(), data)));
  EXPECT_TRUE(
      static_cast<bool>(fsx.write_atomic((scratch / "c.txt").string(), data)));
  EXPECT_TRUE(
      static_cast<bool>(fsx.write_atomic((scratch / "A.txt").string(), data)));
  auto list = fsx.enumerate(scratch.string());
  EXPECT_TRUE(static_cast<bool>(list));
  if (list) {
    const std::vector<std::string> expected = {"A.txt", "a.bin", "b.txt",
                                               "c.txt", "rel.bin"};
    if (*list != expected) {
      ++cap_test::failures();
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__
                << ": enumerate order/content\n";
    }
  }
  auto list_missing = fsx.enumerate((scratch / "ghost").string());
  EXPECT_TRUE(!list_missing);
  EXPECT_EQ(cap::core::error_code_name(list_missing.error().code),
            "missing-resource");
  auto list_file = fsx.enumerate(a);
  EXPECT_TRUE(!list_file);
  EXPECT_EQ(cap::core::error_code_name(list_file.error().code),
            "invalid-input");

  // canonicalize: resolves dot segments, rejects missing paths
  std::error_code ec;
  std::filesystem::create_directory(scratch / "sub", ec);
  const auto canonical_self =
      (scratch / ".." / scratch.filename() / "a.bin").string();
  auto canon = fsx.canonicalize(canonical_self);
  EXPECT_TRUE(static_cast<bool>(canon));
  if (canon) {
    EXPECT_EQ(*canon, std::filesystem::canonical(a).string());
  }
  auto canon_missing = fsx.canonicalize((scratch / "ghost.txt").string());
  EXPECT_TRUE(!canon_missing);
  EXPECT_EQ(cap::core::error_code_name(canon_missing.error().code),
            "missing-resource");

  // Fault injection (permission denials): exercises the kernel-error branches.
  std::error_code fec;
  const std::string ro_dir = (scratch / "ro").string();
  std::filesystem::create_directory(scratch / "ro", fec);
  std::filesystem::permissions(scratch / "ro",
                               std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_exec,
                               fec);
  auto ro_write = fsx.write_atomic((scratch / "ro" / "x.bin").string(), data);
  EXPECT_TRUE(!ro_write); // temp open EACCES
  const std::string secret = (scratch / "secret.bin").string();
  EXPECT_TRUE(static_cast<bool>(fsx.write_atomic(secret, data)));
  std::filesystem::permissions(secret, std::filesystem::perms::none, fec);
  auto unreadable = fsx.read(secret);
  EXPECT_TRUE(!unreadable); // open EACCES
  std::filesystem::permissions(secret,
                               std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_write,
                               fec);
  // 0555 still lists fine; iteration fails only without read: use 0111.
  std::filesystem::create_directory(scratch / "opaque", fec);
  std::filesystem::permissions(scratch / "opaque",
                               std::filesystem::perms::owner_exec, fec);
  auto op_enum = fsx.enumerate((scratch / "opaque").string());
  EXPECT_TRUE(!op_enum); // directory iteration EACCES
  const auto blocked = scratch / "blocked";
  const auto blocked_inner = blocked / "inner";
  fec.clear();
  const bool blocked_parent_created =
      std::filesystem::create_directory(blocked, fec);
  EXPECT_TRUE(blocked_parent_created);
  EXPECT_TRUE(!fec);
  const bool blocked_target_created =
      std::filesystem::create_directory(blocked_inner, fec);
  EXPECT_TRUE(blocked_target_created);
  EXPECT_TRUE(!fec);
  if (blocked_parent_created && blocked_target_created) {
    const bool target_exists = std::filesystem::exists(blocked_inner, fec);
    EXPECT_TRUE(target_exists);
    EXPECT_TRUE(!fec);
    EXPECT_EQ(::access(blocked_inner.c_str(), F_OK), 0);
    const auto restore_perms = std::filesystem::perms::owner_read |
                               std::filesystem::perms::owner_write |
                               std::filesystem::perms::owner_exec;
    std::filesystem::permissions(blocked, std::filesystem::perms::none, fec);
    EXPECT_TRUE(!fec);
    if (!fec) {
      errno = 0;
      const int access_result = ::access(blocked_inner.c_str(), F_OK);
      const int access_error = errno;
      if (access_result == -1 && access_error == EACCES) {
        auto blocked_canon = fsx.canonicalize(blocked_inner.string());
        EXPECT_TRUE(!blocked_canon);
        if (!blocked_canon)
          EXPECT_EQ(cap::core::error_code_name(blocked_canon.error().code),
                    "missing-resource");
      } else if (access_result == 0) {
        std::cout << "SKIP: canonicalize permission denial (host bypasses "
                     "directory permissions)\n";
      } else {
        EXPECT_EQ(access_error, EACCES);
      }
      std::filesystem::permissions(blocked, restore_perms, fec);
      EXPECT_TRUE(!fec);
    }
  }
  std::filesystem::permissions(scratch / "opaque",
                               std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_write |
                                   std::filesystem::perms::owner_exec,
                               fec);

  std::error_code cleanup_ec;
  std::filesystem::remove_all(scratch, cleanup_ec);
  (void)cleanup_ec;
  return cap_test::summary("cap-core filesystem");
}
