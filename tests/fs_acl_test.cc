#include "../fs.h"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <linux/posix_acl_xattr.h>
#include <system_error>

std::filesystem::path prog;

namespace {

using namespace acl;

struct TempDir {
  std::filesystem::path path;

  TempDir() {
    char tmpl[] = "/tmp/fs-acl-test.XXXXXX";
    char *p = mkdtemp(tmpl);
    assert(p);
    path = p;
  }

  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

template <typename Ex, typename F> void expect_throw(F &&f) {
  try {
    f();
    assert(false);
  } catch (const Ex &) {
  }
}

bool acl_unsupported(const std::system_error &e) {
  int ec = e.code().value();
  return ec == ENODATA || ec == ENOSYS || ec == ENOTSUP ||
         ec == EOPNOTSUPP;
}

void test_serialize_deserialize_roundtrip() {
  ACL want{owner("rwx"),     uid(1001, "r--"), fgroup("-w-"),
           gid(1002, "--x"), mask("rw-"),      other("---")};
  assert(deserialize(serialize(want)) == want);
}

void test_deserialize_rejects_invalid_input() {
  expect_throw<std::runtime_error>([] {
    XattrVal raw(sizeof(posix_acl_xattr_header) - 1);
    deserialize(raw);
  });

  expect_throw<std::runtime_error>([] {
    auto raw = serialize({owner("rwx")});
    raw[0] = std::byte{};
    raw[1] = std::byte{};
    raw[2] = std::byte{};
    raw[3] = std::byte{};
    deserialize(raw);
  });
}

void test_normalize_adds_required_entries() {
  ACL want{owner("rwx"), uid(424242, "r-x"), fgroup("---"), mask("r-x"),
           other("---")};
  assert(normalize({uid(424242, "r-x"), owner("rwx")}) == want);
}

void test_normalize_uses_last_duplicate_and_group_class_mask() {
  ACL want{owner("rwx"), uid(424242, "r--"), fgroup("-w-"), mask("rw-"),
           other("--x")};
  ACL got = normalize({uid(424242, "rwx"), other("--x"), owner("rwx"),
                       uid(424242, "r--"), fgroup("-w-")});
  assert(got == want);
}

void test_fd_acl_roundtrip() {
  TempDir td;
  Fd dirfd = xopenat(AT_FDCWD, td.path, O_RDONLY | O_DIRECTORY);
  Fd filefd =
      xopenat(*dirfd, "file", O_CREAT | O_RDWR | O_TRUNC | O_CLOEXEC, 0600);
  uint32_t uid = getuid();
  uint32_t gid = getgid();

  ACL access = normalize({owner("rw-"), acl::uid(uid, "r--")});
  fdsetacl(*filefd, access);
  auto got_access = fdgetacl(*filefd);
  assert(got_access);
  assert(normalize(*got_access) == access);

  auto subdir = td.path / "dir";
  assert(std::filesystem::create_directory(subdir));
  Fd subdirfd = xopenat(AT_FDCWD, subdir, O_RDONLY | O_DIRECTORY);

  ACL def = normalize({owner("rwx"), acl::uid(uid, "r-x"),
                       acl::gid(gid, "-wx")});
  fdsetacl(*subdirfd, def, kAclDefault);
  auto got_default = fdgetacl(*subdirfd, kAclDefault);
  assert(got_default);
  assert(normalize(*got_default) == def);
}

} // namespace

int main() {
  test_serialize_deserialize_roundtrip();
  test_deserialize_rejects_invalid_input();
  test_normalize_adds_required_entries();
  test_normalize_uses_last_duplicate_and_group_class_mask();

  try {
    test_fd_acl_roundtrip();
  } catch (const std::system_error &e) {
    if (!acl_unsupported(e))
      throw;
    std::fprintf(stderr, "skipping ACL xattr round-trip checks: %s\n",
                 e.what());
  }
}
