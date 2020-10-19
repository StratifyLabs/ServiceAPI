#include <var.hpp>

#include "service/Report.hpp"

using namespace service;

Report::Report(const Id &id) : DocumentAccess("reports", id) {}

Report &Report::save(const fs::File &content) {

  Document::save(cloud);
  API_RETURN_VALUE_IF_ERROR(*this);

  if (cloud.create_storage_object(content, get_storage_path()) < 0) {
    set_error_message(cloud().storage_error());
    // need to remove the docoument
    return String();
  }

  return *this;
}

Report &Report::download_contents(const fs::File &destination) {
  cloud.get_storage_object(get_storage_path(), destination);
  return *this;
}
