#include <chrono.hpp>
#include <crypto.hpp>
#include <fs.hpp>
#include <json.hpp>
#include <var.hpp>

#include "service/Job.hpp"

using namespace service;

json::JsonValue Job::publish(const JobOptions &options) {

  Path object_path = Path("jobs") / get_document_id();

  // does the job exists
  Job::Object job_object
    = cloud()
        .get_database_value(object_path, cloud::Cloud::IsRequestShallow(true))
        .to_object();

  if (job_object.is_valid()) {
    ClockTimer timeout_timer;
    // need to wait for the job to complete

    var::Data crypto_key = Base64().decode(get_key());

    IOValue input_value("", crypto_key, options.input());

    timeout_timer.restart();
    KeyString input_id = cloud().create_database_object(
      object_path + "/input",
      input_value.get_value());
    if (input_id.is_empty()) {
      CLOUD_PRINTER_TRACE(
        "Failed to create object " + cloud().database_traffic());
      return json::JsonNull();
    }

    json::JsonObject object;

    // wait for result to post
    do {

      object = cloud().get_database_value(object_path + "/output");
      if (object.at(input_id).is_valid()) {
        // job is complete -- delete the output
        cloud().remove_database_object(object_path + "/output/" + input_id);
        return IOValue("", object.at(input_id)).decrypt_value(crypto_key);
      }

    } while ((options.timeout().seconds() == 0
              || timeout_timer.micro_time() < options.timeout())
             && !is_stop());
  }

  return JsonNull();
}

bool Job::ping(const JobOptions &options) {

  cloud().get_database_value(
    (Path("jobs") / options.id()).string_view(),
    NullFile(),
    cloud::Cloud::IsRequestShallow(true));

  return is_success();
}

Job::Server::~Server() {
  // if job exists -- delete it from the database
  if (m_id.is_empty() == false) {
    // delete job
    cloud().remove_database_object((Path("jobs") / m_id));
  }

  if (m_document_id.is_empty() == false) {
    cloud().remove_document((Path("jobs") / m_id));
  }
}

Job::Server &Job::Server::create(const JobOptions &options) {
  if (m_id.is_empty() == false) {
    // delete the job
    cloud().remove_database_object("jobs/" + m_id);
    m_id = String();
  }

  var::Data key(256 / 8);
  Random().seed().randomize(key);
  set_crypto_key(key);

  const String string_key = Base64().encode(key);

  Job job = std::move(Job()
                        .set_permissions(options.permissions())
                        .set_team_id(options.team())
                        .set_key(string_key.string_view())
                        .save());

  cloud().create_database_object(
    "jobs",
    Job::Object().set_type(type()),
    job.get_document_id());

  return *this;
}

Job::Server &Job::Server::listen() {
  const String job_path = "jobs/" + id();
  m_timeout_timer.restart();

  // cloud().listen_database_stream(job_path, listen_callback_function, this);

  return *this;
}

Job::IOValue &Job::IOValue::encrypt_value(
  const json::JsonValue &value,
  const var::Data &crypto_key) {

  const var::String string_value
    = JsonDocument()
        .set_option_flags(JsonDocument::OptionFlags::compact)
        .stringify(value);

  Aes::InitializationVector iv;
  Random().seed().randomize(iv);

  set_initialization_vector(View(iv).to_string());

  set_blob(Base64().encode(
    DataFile()
      .write(
        ViewFile(string_value),
        AesCbcEncrypter().set_key256(crypto_key).set_initialization_vector(iv))
      .data()));

  return *this;
}

json::JsonValue Job::IOValue::decrypt_value(const var::Data &crypto_key) const {

  // iv is 16 bytes -- 32 characters
  StringView iv_string = get_initialization_vector();

  const Data iv_data = Data::from_string(iv_string);
  const Data cipher_data = Base64().decode(get_blob());

  return JsonDocument().load(
    DataFile()
      .reserve(cipher_data.size())
      .write(
        ViewFile(cipher_data),
        AesCbcDecrypter().set_initialization_vector(iv_data).set_key256(
          crypto_key)));
}

bool Job::Server::listen_callback(
  const var::StringView event,
  const json::JsonValue &data) {
  MCU_UNUSED_ARGUMENT(event);
  if (data.is_valid() && data.is_object()) {
    const String path = "jobs/" + id();

    Job::Object object = cloud().get_database_value(path).to_object();

    json::JsonKeyValueList<Job::IOValue> input_list = object.get_input();

    json::JsonValue::KeyList key_list = input_list.get_key_list();

    for (const auto key : key_list) {

      if (callback() && (object.input().at(key).is_valid())) {

        JsonValue output = callback()(
          context(),
          object.get_type(),
          input_list.at(key).decrypt_value(crypto_key()));

        cloud().create_database_object(
          Path(path) / "output",
          Job::IOValue("", crypto_key(), output).get_value(),
          key);

        cloud().remove_database_object(path + "/input/" + key);
      }
    }
  } else {
    if (callback()) {
      callback()(context(), "", JsonNull());
    }
  }
  return is_stop() == false;
}
