#ifndef SERVICE_API_SERVICE_JOB_HPP
#define SERVICE_API_SERVICE_JOB_HPP

#include <CloudAPI/cloud/CloudObject.hpp>
#include <json/Json.hpp>
#include <var/String.hpp>

#include "Report.hpp"

namespace service {

class JobOptions {
  API_AC(JobOptions, var::String, id);
  API_AC(JobOptions, var::String, team);
  API_AC(JobOptions, var::Data, key);
  API_AC(JobOptions, var::String, permissions);
  API_AC(JobOptions, json::JsonValue, input);
  API_AC(JobOptions, chrono::MicroTime, timeout);

public:
  JobOptions() { set_permissions("private"); }
};

class JobIOValue : public json::JsonKeyValue, public cloud::CloudAccess {
public:
  JobIOValue(const var::String &key, const json::JsonValue &value)
    : JsonKeyValue(key, value) {}

  JobIOValue(
    const var::String &key,
    const var::Data &crypto_key,
    const json::JsonValue &value)
    : JsonKeyValue(key, json::JsonObject()) {
    encrypt_value(value, crypto_key);
  }

  JSON_ACCESS_STRING_WITH_KEY(JobIOValue, iv, initialization_vector);
  JSON_ACCESS_STRING(JobIOValue, blob);

  JobIOValue &
  encrypt_value(const json::JsonValue &value, const var::Data &crypto_key);
  json::JsonValue decrypt_value(const var::Data &crypto_key);
};

class JobObject : public json::JsonObject {
public:
  JobObject() {}
  JobObject(const json::JsonObject &object) : json::JsonObject(object) {}

  JSON_ACCESS_STRING_WITH_KEY(JobObject, documentId, document_id);
  JSON_ACCESS_STRING(JobObject, type);
  JSON_ACCESS_OBJECT_LIST(JobObject, JobIOValue, input);
  JSON_ACCESS_OBJECT_LIST(JobObject, JobIOValue, output);

  bool is_valid() const { return get_type().is_empty() == false; }
};

class JobDocumentOptions : public DocumentOptionsAccess<JobDocumentOptions> {
public:
  JobDocumentOptions() { set_path("jobs"); }
};

class JobDocument : public DocumentAccess<JobDocument> {
public:
  JobDocument() {}
  JobDocument(const json::JsonObject &object) : DocumentAccess(object) {}
  JSON_ACCESS_STRING(JobDocument, key);
  JSON_ACCESS_STRING(JobDocument, id);
  JSON_ACCESS_STRING(JobDocument, type);
};

class Job : public cloud::CloudAccess {
public:
  Job();

  bool ping(const JobOptions &options);
  json::JsonValue publish(const JobOptions &options);

private:
  API_AC(Job, chrono::MicroTime, timeout);
  API_AB(Job, stop, false);
};

class JobServer : public cloud::CloudAccess {
public:
  JobServer() {}
  ~JobServer();
  typedef json::JsonValue (*callback_t)(
    void *context,
    const var::String &type,
    const json::JsonValue &input_value);

  var::String create(const JobOptions &options);
  bool listen();

private:
  API_AC(JobServer, var::String, type);
  API_AB(JobServer, stop, false);
  API_AC(JobServer, chrono::MicroTime, timeout);
  API_AF(JobServer, callback_t, callback, nullptr);
  API_AF(JobServer, void *, context, nullptr);
  API_RAC(JobServer, var::String, id);
  API_RAC(JobServer, var::String, document_id);
  API_AC(JobServer, var::Data, crypto_key);

  chrono::Timer m_timeout_timer;

  static bool listen_callback_function(
    void *context,
    const var::String &event,
    const json::JsonValue &data) {
    return reinterpret_cast<JobServer *>(context)->listen_callback(event, data);
  }

  bool listen_callback(const var::String &event, const json::JsonValue &data);
};

} // namespace service

#endif // SERVICE_API_SERVICE_JOB_HPP
