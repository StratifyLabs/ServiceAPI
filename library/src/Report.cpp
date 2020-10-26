#include <var.hpp>

#include "service/Report.hpp"

using namespace service;

Report::Report(const Id &id) : DocumentAccess("reports", id) {}

Report &Report::save(const fs::FileObject &content) {
  DocumentAccess<Report>::save();
  cloud().create_storage_object(get_storage_path(), content);
  return *this;
}

Report &Report::download_contents(const fs::FileObject &destination) {
  cloud().get_storage_object(get_storage_path(), destination);
  return *this;
}
