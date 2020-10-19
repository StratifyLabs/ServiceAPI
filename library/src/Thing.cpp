#include <sapi/calc.hpp>
#include <sapi/chrono.hpp>
#include <sapi/hal/Core.hpp>
#include <sapi/var.hpp>

#include "service/Thing.hpp"

using namespace service;

Thing::Thing(const Id &id) : DocumentAccess("things", id) {}

Thing::Thing(const sos::Sys::Info &info)
  : DocumentAccess("things", Id(info.serial_number().to_string().string())) {}

Thing &Thing::set_system_info(const sos::Sys::Info &info) {
  set_document_id(info.serial_number().to_string());
  set_team_id(info.team_id());
  set_system_information(ThingSystemInformation(info));
  return *this;
}

var::String Thing::upload() {

  String id;

  id = get_system_information().get_serial_number();

  if (id.is_empty()) {
    CLOUD_PRINTER_TRACE("no id available for thing");
    return String();
  }

  set_user_id(cloud().credentials().get_uid());

  CLOUD_PRINTER_TRACE("upload thing " + id);

  if (get_team_id().is_empty()) {
    CLOUD_PRINTER_TRACE("upload to public things");
  } else {
    CLOUD_PRINTER_TRACE("upload to team " + get_team_id());
  }

  bool is_create = Thing().download(ThingOptions().set_document_id(id)) < 0;

  CLOUD_PRINTER_TRACE(
    "Thing needs to be created? " + String::number(is_create));

  return Document::upload(
    ThingOptions().set_document_id(id).set_create(is_create));
}
