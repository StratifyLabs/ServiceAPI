#include "service/User.hpp"

using namespace service;

User::User(const Id &id)
  : DocumentAccess("users", id)

{}
