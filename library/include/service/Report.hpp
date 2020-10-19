#ifndef SERVICE_API_SERVICE_REPORT_HPP
#define SERVICE_API_SERVICE_REPORT_HPP

#include <fs/File.hpp>

#include "Document.hpp"

namespace service {

class ReportOptions : public DocumentOptionsAccess<ReportOptions> {
public:
  ReportOptions() { set_path("reports"); }

  API_AF(ReportOptions, const fs::File *, file, nullptr);
};

class Report : public DocumentAccess<Report> {
public:
  explicit Report(const Id &id = Id());

  JSON_ACCESS_STRING(Report, name);

  // document ID, team ID, user ID, and timestamp is inherited from Document

  // available if a thing is associatd with this report
  JSON_ACCESS_STRING_WITH_KEY(Report, thingId, thing_id);
  // if a project is associated with this report
  JSON_ACCESS_STRING_WITH_KEY(Report, projectId, project_id);

  Report &save(const fs::File &contents);

  Report &download_contents(const fs::File &destination);

  Path get_storage_path() const {
    API_ASSERT(!id().is_empty());
    return Path("reports/").append(id().cstring()).append(".md");
  }
};

} // namespace service

#endif // SERVICE_API_SERVICE_REPORT_HPP
