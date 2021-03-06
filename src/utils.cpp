#include <exceptions.hpp>
#include <gsl/gsl>
#include <iomanip>
#include <logger.hpp>

using suex::permissions::User;

std::string utils::CommandArgsText(const std::vector<char *> &cmdargv) {
  std::ostringstream ss;
  for (char *q : gsl::make_span(cmdargv).subspan(0, cmdargv.size() - 2)) {
    ss << q << " ";
  }

  ss << cmdargv[cmdargv.size() - 2];

  return ss.str();
}

bool utils::BypassPermissions(const User &as_user) {
  // if the user / grp is root, just let them run.
  if (RunningUser().Id() == 0 && RunningUser().GroupId() == 0) {
    return true;
  }

  // if the user / grp are the same as the running user,
  // just run the app without performing any operations
  return RunningUser().Id() == as_user.Id() &&
         RunningUser().GroupId() == as_user.GroupId();
}

std::string utils::GetEditor() {
  std::string editor{env::Get("EDITOR")};

  while (true) {
    try {
      return path::Locate(editor);
    } catch (...) {
      std::cout << "$EDITOR is not set or invalid, please enter editor name: ";
      std::getline(std::cin, editor);
    }
  }
}

bool utils::AskQuestion(const std::string &prompt) {
  std::cout << prompt << " ";
  std::string ans;
  std::getline(std::cin, ans);
  std::transform(ans.begin(), ans.end(), ans.begin(), ::tolower);
  return ans == "y" || ans == "yes";
}

const suex::permissions::User &RunningUser() {
  static const suex::permissions::User user{getuid()};
  return user;
}

const suex::permissions::User &RootUser() {
  static const suex::permissions::User user{0};
  return user;
}

const suex::permissions::Group &WheelGroup() {
  static const suex::permissions::Group grp{"wheel"};
  return grp;
}
