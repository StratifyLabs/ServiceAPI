// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include <chrono.hpp>
#include <var.hpp>

#include "service/Thing.hpp"

using namespace service;

Thing::Thing(const Id &id) : DocumentAccess("things", id) {}

Thing::Thing(const sos::Sys::Info &info)
  : DocumentAccess(
    "things",
    Id(info.serial_number().to_string().string_view())) {}

Thing &Thing::set_system_info(const sos::Sys::Info &info) {
  set_document_id(info.serial_number().to_string());
  set_team_id(info.team_id());
  set_system_information(SystemInformation(info));
  return *this;
}
