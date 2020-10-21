#ifndef SERVICE_API_SERVICE_JOB_HPP
#define SERVICE_API_SERVICE_JOB_HPP

#include <CloudAPI/cloud/CloudObject.hpp>
#include <chrono/ClockTimer.hpp>
#include <json/Json.hpp>
#include <var/String.hpp>

#include "Report.hpp"

namespace service {

class JobOptions {
  API_AC(JobOptions, var::StringView, id);
  API_AC(JobOptions, var::StringView, team);
  API_AC(JobOptions, var::Data, key);
  API_AC(JobOptions, var::StringView, permissions);
  API_AC(JobOptions, json::JsonValue, input);
  API_AC(JobOptions, chrono::MicroTime, timeout);

public:
  JobOptions() { set_permissions("private"); }
};

class Job : public DocumentAccess<Job> {
public:

  class IOValue : public json::JsonKeyValue, public cloud::CloudAccess {
  public:
    IOValue(const char *key, const json::JsonValue &value)
      : JsonKeyValue(key, value) {}

    IOValue(
      const char *key,
      const var::Data &crypto_key,
      const json::JsonValue &value)
      : JsonKeyValue(key, json::JsonObject()) {
      encrypt_value(value, crypto_key);
    }

    JSON_ACCESS_STRING_WITH_KEY(IOValue, iv, initialization_vector);
    JSON_ACCESS_STRING(IOValue, blob);

    IOValue &
    encrypt_value(const json::JsonValue &value, const var::Data &crypto_key);
    json::JsonValue decrypt_value(const var::Data &crypto_key) const;
  };

  class Object : public json::JsonObject {
  public:
    Object() {}
    Object(const json::JsonObject &object) : json::JsonObject(object) {}

    JSON_ACCESS_STRING_WITH_KEY(Object, documentId, document_id);
    JSON_ACCESS_STRING(Object, type);
    JSON_ACCESS_OBJECT_LIST(Object, IOValue, input);
    JSON_ACCESS_OBJECT_LIST(Object, IOValue, output);

    bool is_valid() const { return get_type().is_empty() == false; }
  };

  class Server : public cloud::CloudAccess {
  public:
    Server() {}
    ~Server();
    typedef json::JsonValue (*callback_t)(
      void *context,
      const var::StringView type,
      const json::JsonValue &input_value);

    Server &create(const JobOptions &options);
    Server &listen();

  private:
    API_AC(Server, var::String, type);
    API_AB(Server, stop, false);
    API_AC(Server, chrono::MicroTime, timeout);
    API_AF(Server, callback_t, callback, nullptr);
    API_AF(Server, void *, context, nullptr);
    API_RAC(Server, var::String, id);
    API_RAC(Server, Document::Id, document_id);
    API_AC(Server, var::Data, crypto_key);

    chrono::ClockTimer m_timeout_timer;

    static bool listen_callback_function(
      void *context,
      const var::StringView event,
      const json::JsonValue &data) {
      return reinterpret_cast<Server *>(context)->listen_callback(event, data);
    }

    bool
    listen_callback(const var::StringView event, const json::JsonValue &data);
  };

  Job(const var::StringView id = "") : DocumentAccess("jobs", id) {}

  bool ping(const JobOptions &options);
  json::JsonValue publish(const JobOptions &options);

  JSON_ACCESS_STRING(Job, key);
  JSON_ACCESS_STRING(Job, id);
  JSON_ACCESS_STRING(Job, type);

private:
  API_AC(Job, chrono::MicroTime, timeout);
  API_AB(Job, stop, false);
};

} // namespace service

#endif // SERVICE_API_SERVICE_JOB_HPP
