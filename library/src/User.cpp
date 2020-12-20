// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include "service/User.hpp"

using namespace service;

User::User(const Id &id)
  : DocumentAccess("users", id)

{}
