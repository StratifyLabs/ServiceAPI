// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include "service/Team.hpp"

using namespace service;

Team::Team(const Id &id) : DocumentAccess("teams", id) {}
