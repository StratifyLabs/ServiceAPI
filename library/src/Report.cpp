#include <var.hpp>

#include "service/Report.hpp"

using namespace service;

Report::Report(const Id &id) : DocumentAccess("reports", id) {}

Report &Report::save(const fs::File &content) {
  DocumentAccess<Report>::save();
  cloud().create_storage_object(content, get_storage_path());
  return *this;
}

Report &Report::download_contents(const fs::File &destination) {
  cloud().get_storage_object(get_storage_path(), destination);
  return *this;
}
