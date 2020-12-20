// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include "service/Hardware.hpp"

using namespace service;

Hardware::Hardware(const Id &id) : DocumentAccess("hardware", id) {}
