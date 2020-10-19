#include "service/Hardware.hpp"

using namespace service;

Hardware::Hardware(const Id &id) : DocumentAccess("hardware", id) {}
