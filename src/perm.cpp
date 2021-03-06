#include <exceptions.hpp>
#include <logger.hpp>
#include <sstream>

using suex::permissions::Entity;
using suex::permissions::Group;
using suex::permissions::User;

std::ostream &permissions::operator<<(std::ostream &os, const Entity &entity) {
  os << (entity.Deny() ? "deny" : "permit") << " " << entity.Owner().Name()
     << " as " << entity.AsUser().Name() << " ";

  std::ostringstream opts_ss;
  opts_ss << (entity.PromptForPassword() ? "" : "nopass ")
          << (entity.KeepEnvironment() ? "keepenv " : "")
          << (entity.CacheAuth() ? "persist " : "");

  os << "options " << (opts_ss.tellp() == 0 ? "- " : opts_ss.str()) << "cmd "
     << entity.Command();
  return os;
}

bool Entity::CanExecute(const User &user, const std::string &cmd) const {
  if (Owner().Id() != RunningUser().Id()) {
    return false;
  }

  if (AsUser().Id() != user.Id()) {
    return false;
  };

  std::string prefix;
  DEFER(logger::debug() << prefix << cmd_re << " ~= " << cmd << std::endl);
  // no need to pre-compile the regular expression
  // it's evaluated only once during execution
  if (re2::RE2::FullMatch(cmd, re2::RE2(cmd_re))) {
    prefix = "[!] ";
    return true;
  }

  return false;
}

int setgroups(const User &user) {
  // walk through all the groups that a user has and set them
  int ngroups = 0;
  const char *name = user.Name().c_str();
  std::vector<gid_t> groupvec{};

  while (true) {
    if (getgrouplist(name, static_cast<gid_t>(user.GroupId()),
                     &groupvec.front(), &ngroups) < 0) {
      groupvec.resize(static_cast<uint64_t>(ngroups));
      continue;
    }
    return setgroups(static_cast<size_t>(ngroups), &groupvec.front());
  }
}

void permissions::Set(const User &user) {
  if (setgroups(user) < 0) {
    throw suex::PermissionError("execution of setgroups(%d) failed",
                                user.GroupId());
  }

  if (setgid(static_cast<gid_t>(user.GroupId())) < 0) {
    throw suex::PermissionError("execution of setgid(%d) failed",
                                user.GroupId());
  }

  if (setuid(static_cast<uid_t>(user.Id())) < 0) {
    throw suex::PermissionError("execution of setuid(%d) failed", user.Id());
  }
}

void User::Initialize(const struct passwd *pw) {
  uid_ = pw->pw_uid;
  name_ = pw->pw_name;
  gid_ = pw->pw_gid;
  home_dir_ = pw->pw_dir;
  shell_ = pw->pw_shell;
}

User::User(uid_t uid) : uid_{-1} {
  struct passwd *pw = getpwuid(uid);
  if (pw == nullptr) {
    return;
  }
  Initialize(pw);
}

User::User(const std::string &user) : name_{user}, uid_{-1} {
  // try to extract the password struct
  // if the user is empty, use the current user,
  // otherwise try to take the one that was passed.

  // if both fail, also try to load the user as a uid.
  const struct passwd *pw =
      user.empty() ? getpwuid(static_cast<uid_t>(RunningUser().Id()))
                   : getpwnam(user.c_str());

  if (pw == nullptr) {
    try {
      pw = getpwuid(static_cast<uid_t>(std::stol(user, nullptr, 10)));

    } catch (std::invalid_argument &) {
      // the user string is not a number
    }
    if (pw == nullptr) {
      // not found
      return;
    }
  }

  Initialize(pw);
}

User::User(const User &user) {
  uid_ = user.uid_;
  name_ = user.name_;
  gid_ = user.gid_;
  home_dir_ = user.home_dir_;
  shell_ = user.shell_;
}

bool User::operator==(const User &other) const { return uid_ == other.uid_; }

bool User::operator!=(const User &other) const { return !(other == *this); }

bool User::operator<(const User &other) const { return uid_ < other.uid_; }

bool User::operator>(const User &other) const { return other < *this; }

bool User::operator<=(const User &other) const { return !(other < *this); }

bool User::operator>=(const User &other) const { return !(*this < other); }

Group::Group(gid_t gid) : gid_{-1} {
  struct group *gr = getgrgid(gid);
  if (gr == nullptr) {
    return;
  }
  Initialize(gr);
}

Group::Group(const std::string &grp) : name_{grp}, gid_{-1} {
  // try to extract the group struct
  // if the group is empty, and the user exists -> use the user's group,
  //
  // otherwise, try to extract the extracted group.

  // if those fail, also try to load the group as a gid.
  if (grp.empty()) {
    return;
  }

  struct group *gr = getgrnam(grp.c_str());
  if (gr == nullptr) {
    try {
      gr = getgrgid(static_cast<gid_t>(std::stol(grp, nullptr, 10)));

    } catch (std::invalid_argument &) {
      // the group string is not a number
    }
    if (gr == nullptr) {
      // group not found
      return;
    }
  }

  Initialize(gr);
}

bool Group::operator==(const Group &other) const { return gid_ == other.gid_; }
bool Group::operator!=(const Group &other) const { return !(other == *this); }

Group::Group(const Group &grp) {
  name_ = grp.name_;
  gid_ = grp.gid_;
  members_ = grp.members_;
}
bool Group::operator<(const Group &other) const { return gid_ < other.gid_; }
bool Group::operator>(const Group &other) const { return other < *this; }
bool Group::operator<=(const Group &other) const { return !(other < *this); }
bool Group::operator>=(const Group &other) const { return !(*this < other); }

void Group::Initialize(const struct group *gr) {
  gid_ = gr->gr_gid;
  name_ = std::string(gr->gr_name);

  if (RunningUser().GroupId() == gid_) {
    members_.emplace(RunningUser());
  }

  for (auto it = gr->gr_mem; (*it) != nullptr; it++) {
    members_.emplace(User(*it));
  }
}
