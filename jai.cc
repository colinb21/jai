#include <cassert>
#include <cstring>
#include <filesystem>
#include <functional>
#include <utility>

#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using std::filesystem::path;

path prog;

struct Fd {
  int fd_{-1};

  Fd() noexcept = default;
  explicit Fd(int fd) : fd_(fd) {}
  Fd(Fd &&other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
  ~Fd() { reset(); }

  Fd &operator=(Fd &&other) noexcept
  {
    reset();
    fd_ = std::exchange(other.fd_, -1);
    return *this;
  }
  Fd &operator=(int fd) noexcept
  {
    reset();
    fd_ = fd;
    return *this;
  }

  int operator*() const noexcept
  {
    assert(*this);
    return fd_;
  }
  explicit operator bool() const { return fd_ >= 0; }
  void reset() noexcept
  {
    if (*this) {
      close(fd_);
      fd_ = -1;
    }
  }
};

template<std::invocable F = std::function<void()>> struct Defer {
  F f_;

  Defer(F f) noexcept : f_(std::move(f)) {}
  ~Defer()
  {
    if constexpr (requires { bool(f_); })
      if (!f_)
        return;
    f_();
  }
  template<std::same_as<F> U = F> requires std::assignable_from<U, F &&>
  Defer &operator=(Defer &&other) noexcept
  {
    f_ = std::move(other.f_);
    return *this;
  }
  template<std::same_as<F> U = F> requires requires(U u) {
    u = F{};
    requires !bool(U{});
  }
  void reset() noexcept
  {
    f_ = F{};
  }
};

struct Config {
  std::string user;
  uid_t uid{};
  gid_t gid{};
  path home;
  Fd homefd;
  Fd jaifd;

  void init();
};

template<typename... Args>
[[noreturn]] void
syserr(std::format_string<Args...> fmt, Args &&...args)
{
  throw std::system_error(
      errno, std::system_category(),
      std::vformat(fmt.get(), std::make_format_args(args...)));
}

template<typename E = std::runtime_error, typename... Args>
[[noreturn]] void
err(std::format_string<Args...> fmt, Args &&...args)
{
  throw E(std::vformat(fmt.get(), std::make_format_args(args...)));
}

template<std::integral I>
I
parsei(const char *s)
{
  const char *const e = s + strlen(s);
  I ret{};

  auto [p, ec] = std::from_chars(s, e, ret, 10);

  if (ec == std::errc::invalid_argument || p != e)
    err<std::invalid_argument>("{}: not an integer", s);
  if (ec == std::errc::result_out_of_range)
    err<std::out_of_range>("{}: overflow", s);

  return ret;
}

Fd
mkudir(const Fd &dir, path p, uid_t u, gid_t g)
{
  int dfd = p.is_absolute() ? -1 : *dir;
  if (Fd e{openat(dfd, p.c_str(), O_RDONLY)}) {
    struct stat sb;
    if (fstat(*e, &sb))
      syserr("stat({})", p.string());
    if (!S_ISDIR(sb.st_mode))
      err("{} must be a directory", p.string());
    return e;
  }
  if (errno != ENOENT)
    syserr("open({})", p.string());

  if (mkdirat(dfd, p.c_str(), 0755))
    syserr("mkdir({})", p.string());
  Fd d{openat(dfd, p.c_str(), O_RDONLY)};
  if (!d)
    syserr("open({})", p.string());

  struct stat sb1, sb2;
  if (fstat(*d, &sb1))
    syserr("stat({})", p.string());
}

void
Config::init()
{
  char buf[512];
  struct passwd pwbuf, *pw{};

  uid = getuid();

  const char *envuser = getenv("SUDO_USER");
  if (!uid && envuser) {
    if (getpwnam_r(envuser, &pwbuf, buf, sizeof(buf), &pw))
      err("cannot file password entry for user {}", envuser);
    user = envuser;
    uid = pw->pw_uid;
    gid = pw->pw_gid;
    home = pw->pw_dir;
  }
  else {
    if (getpwuid_r(uid, &pwbuf, buf, sizeof(buf), &pw))
      err("cannot file password entry for uid {}", uid);
    user = pw->pw_name;
    gid = pw->pw_gid;
    home = pw->pw_dir;
  }

  if (!(homefd = open(home.c_str(), O_RDONLY)))
    syserr("{}", home.string());
}

int
main(int argc, char **argv)
{
  if (argc > 0)
    prog = argv[0];
}
