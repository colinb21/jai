// -*-C++-*-

#pragma once

#include "err.h"

#include <linux/sched.h>
#include <sched.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

inline const char *
env_or_empty(std::string_view var)
{
  const char *p = getenv(std::string(var).c_str());
  return p ? p : "";
}

// Calls exp("VAR"sv) to expand strings like "123${VAR}456".
template<typename Exp = decltype(env_or_empty)>
std::string
var_expand(std::string_view in, Exp &&exp = env_or_empty)
    requires requires(std::string r) { r += exp(in); }
{
  std::string ret;
  for (std::size_t i = 0, e = in.size(); i < e;)
    if (in[i] == '\\')
      ret += (++i < e ? in[i++] : '\\');
    else if (size_t j;
             in.substr(i, 2) == "${" && (j = in.find('}', i + 2)) != in.npos) {
      ret += exp(in.substr(i + 2, j - i - 2));
      i = j + 1;
    }
    else
      ret += in[i++];
  return ret;
}

inline pid_t
xfork(std::uint64_t flags = 0)
{
  clone_args ca{.flags = flags, .exit_signal = SIGCHLD};
  if (auto ret = syscall(SYS_clone3, &ca, sizeof(ca)); ret != -1)
    return ret;
  syserr("clone3");
}

#define xsetns(fd, type)                                                       \
  do {                                                                         \
    if (setns(fd, type)) {                                                     \
      warn("setns({}, {}): {}", fdpath(fd), #type, strerror(errno));           \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

extern const std::string jai_defaults;
extern const std::string default_conf;
