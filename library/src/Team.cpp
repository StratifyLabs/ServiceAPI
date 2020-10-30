#include "service/Team.hpp"

using namespace service;

Team::Team(const Id &id) : DocumentAccess("teams", id) {}
