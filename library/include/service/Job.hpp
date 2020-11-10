#ifndef SERVICE_API_SERVICE_JOB_HPP
#define SERVICE_API_SERVICE_JOB_HPP

#include <chrono/ClockTimer.hpp>
#include <cloud/CloudObject.hpp>
#include <fs/LambdaFile.hpp>
#include <json/Json.hpp>
#include <var/String.hpp>

#include "Report.hpp"

namespace service {

class Job : public DocumentAccess<Job> {
public:
  class IOValue : public json::JsonKeyValue, public cloud::CloudAccess {
  public:
    IOValue(const var::StringView key, const json::JsonValue &value)
      : JsonKeyValue(key, value) {}

    IOValue(
      const var::StringView key,
      const crypto::Aes::Key &crypto_key,
      const json::JsonValue &value)
      : JsonKeyValue(key, json::JsonObject()) {
      encrypt_value(value, crypto_key);
    }

    JSON_ACCESS_STRING_WITH_KEY(IOValue, iv, initialization_vector);
    JSON_ACCESS_STRING(IOValue, blob);

    json::JsonValue decrypt_value(const crypto::Aes::Key &key) const;

  private:
    void
    encrypt_value(const json::JsonValue &value, const crypto::Aes::Key &key);
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
    typedef json::JsonValue (*callback_t)(
      void *context,
      const var::StringView type,
      const json::JsonValue &input_value);

    Server() {}
    Server(const Server &a) = delete;
    Server &operator=(const Server &a) = delete;

    Server(Server &&a) { swap(a); }
    Server &operator=(Server &&a) {
      swap(a);
      return *this;
    }
    ~Server();

    Server &&move() { return std::move(*this); }
    Server &start(const var::StringView type, const Job &job_document);

    var::StringView volatile get_job_id() { return m_id; }

  private:
    API_AB(Server, stop, false);
    API_AF(Server, callback_t, callback, nullptr);
    API_AF(Server, void *, context, nullptr);
    API_AC(Server, Id, id);
    API_AC(Server, crypto::Aes::Key, crypto_key);

    chrono::ClockTimer m_timeout_timer;

    void process_input(const json::JsonValue &data);
    void listen();
    void swap(Server &a) {
      std::swap(m_id, a.m_id);
      std::swap(m_context, a.m_context);
      std::swap(m_callback, a.m_callback);
      std::swap(m_crypto_key, a.m_crypto_key);
      set_cloud(a.cloud());
    }
  };

  Job() : DocumentAccess("jobs", "") {}
  Job(const var::StringView id) : DocumentAccess("jobs", id) {}

  bool ping(const var::StringView id);

  json::JsonValue
  publish(const JsonValue &input, const chrono::MicroTime &timeout);

  JSON_ACCESS_STRING(Job, key);
  JSON_ACCESS_STRING(Job, id);
  JSON_ACCESS_STRING(Job, type);

private:
  API_AC(Job, chrono::MicroTime, timeout);
  API_AB(Job, stop, false);

};

} // namespace service

#endif // SERVICE_API_SERVICE_JOB_HPP
