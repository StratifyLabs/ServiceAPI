#ifndef SERVICE_API_SERVICE_REPORT_HPP
#define SERVICE_API_SERVICE_REPORT_HPP

#include <crypto/Aes.hpp>
#include <fs/File.hpp>

#include "Document.hpp"

namespace service {


class Report : public DocumentAccess<Report> {
public:
  Report();
  Report(const Id &id, const fs::FileObject &destination);

  JSON_ACCESS_STRING(Report, name);

  // document ID, team ID, user ID, and timestamp is inherited from Document

  // available if a thing is associatd with this report
  JSON_ACCESS_STRING_WITH_KEY(Report, thingId, thing_id);
  // if a project is associated with this report
  JSON_ACCESS_STRING_WITH_KEY(Report, projectId, project_id);
  JSON_ACCESS_STRING(Report, key);
  JSON_ACCESS_STRING(Report, iv);
  JSON_ACCESS_INTEGER(Report, padding);

  Report &save(const fs::FileObject &contents);


  Path get_storage_path() const {
    API_ASSERT(!id().is_empty());
    return (Path("reports") / id()).append(".aes");
  }

private:
  API_AC(Report, crypto::Aes::Key, secret_key);
  void download_contents(const fs::FileObject &destination);
};

} // namespace service

#endif // SERVICE_API_SERVICE_REPORT_HPP
