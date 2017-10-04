#include <vector>
#include <getopt.h>
#include <perm.h>
#include <iostream>

#define DEFAULT_CONFIG_PATH "/etc/doas.conf"
#define DEFAULT_AUTH_SERVICE "su"

static const User running_user = User(getuid());

class Options {
 public:
  Options(int argc, char *argv[]);

  char *const *CommandArguments() const {
    return args_.data();
  }

  const std::string &ConfigurationPath() const { return config_path_; }

  const std::string &AuthenticationService() const { return pam_service_name_; }

  const User &AsUser() const { return user_; }

 private:
  int Parse(int argc, char **argv);
  void ParsePermissions(const std::string &username);
  std::vector<char *> args_{};
  std::string config_path_{DEFAULT_CONFIG_PATH};
  std::string pam_service_name_{DEFAULT_AUTH_SERVICE};
  std::string binary_{};
  User user_ = User(0);
};

