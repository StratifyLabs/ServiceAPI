#ifndef SERVICE_API_SERVICE_TEAM_HPP
#define SERVICE_API_SERVICE_TEAM_HPP

#include "Document.hpp"

namespace service {

class Team : public DocumentAccess<Team> {
public:
  class User : public DocumentAccess<User> {
  public:
    User(const Id &team, const Id &id = "")
      : DocumentAccess(Path("teams") / team / "users", id) {}

    JSON_ACCESS_BOOL(User, create);
    JSON_ACCESS_BOOL(User, remove);
    JSON_ACCESS_BOOL(User, read);
    JSON_ACCESS_BOOL(User, update);
    JSON_ACCESS_BOOL(User, write);
    JSON_ACCESS_BOOL(User, admin);
  };

  Team(const Id &id = Id());
  JSON_ACCESS_STRING(Team, name);
};

} // namespace service

#endif // SERVICE_API_SERVICE_TEAM_HPP
