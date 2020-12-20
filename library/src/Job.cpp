// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include <chrono.hpp>
#include <crypto.hpp>
#include <fs.hpp>
#include <json.hpp>
#include <thread.hpp>
#include <var.hpp>

#include "service/Job.hpp"

using namespace service;

json::JsonValue
Job::publish(const JsonValue &input, const chrono::MicroTime &timeout) {

  Path object_path = Path("jobs") / get_document_id();

  // does the job exists
  Job::Object job_object
    = cloud()
        .get_database_value(object_path, cloud::Cloud::IsRequestShallow(true))
        .to_object();

  if (job_object.is_valid()) {
    ClockTimer timeout_timer;
    // need to wait for the job to complete

    Aes::Key crypto_key(Aes::Key::Construct().set_key(get_key()));

    IOValue input_value("", crypto_key, input);

    timeout_timer.restart();
    KeyString input_id = cloud().create_database_object(
      Path(object_path) / "input",
      input_value.get_value());
    if (input_id.is_empty()) {
      CLOUD_PRINTER_TRACE(
        "Failed to create object " + cloud().database_traffic());
      return json::JsonNull();
    }

    json::JsonObject object;

    // wait for result to post
    do {

      object = cloud().get_database_value(Path(object_path) / "output");
      if (object.at(input_id).is_valid()) {
        // job is complete -- delete the output
        cloud().remove_database_object(Path(object_path) / "output" / input_id);
        return IOValue("", object.at(input_id)).decrypt_value(crypto_key);
      }
      wait(5_seconds);

    } while ((timeout.seconds() == 0 || timeout_timer < timeout) && !is_stop());
  }

  return JsonNull();
}

bool Job::ping(const var::StringView id) {
  cloud().get_database_value(
    Path("jobs") / id,
    NullFile(),
    cloud::Cloud::IsRequestShallow(true));
  bool result = is_success();
  API_RESET_ERROR();
  return result;
}

void Job::IOValue::encrypt_value(
  const json::JsonValue &value,
  const crypto::Aes::Key &key) {

  var::String string_value
    = JsonDocument()
        .set_flags(JsonDocument::Flags::compact)
        .stringify(value);
  const size_t padding = Aes::get_padding(string_value.length());
  string_value += (String("\n") * padding);

  API_ASSERT(string_value.length() % 16 == 0);

  set_initialization_vector(key.get_initialization_vector_string());

  DataFile encrypted_file
    = DataFile()
        .write(
          ViewFile(string_value),
          AesCbcEncrypter()
            .set_key256(key.key256())
            .set_initialization_vector(key.initialization_vector()))
        .move();

  set_blob(Base64().encode(encrypted_file.data()));
}

json::JsonValue Job::IOValue::decrypt_value(const crypto::Aes::Key &key) const {

  // iv is 16 bytes -- 32 characters
  const StringView iv_string = get_initialization_vector();

  const Data iv_data = Data::from_string(iv_string);
  const Data cipher_data = Base64().decode(get_blob());

  return JsonDocument().load(
    DataFile()
      .reserve(cipher_data.size())
      .write(
        ViewFile(cipher_data),
        AesCbcDecrypter().set_initialization_vector(iv_data).set_key256(
          key.key256()))
      .seek(0));
}

Job::Server::~Server() {
  // if job exists -- delete it from the database
  if (id().is_empty() == false) {
    // delete job
    const Path job_path = Path("jobs") / id();
    cloud().remove_database_object(job_path.string_view());
    cloud().remove_document(job_path.string_view());
  }
}

Job::Server &
Job::Server::start(const var::StringView type, const Job &job_document) {

  API_ASSERT(id().is_empty());
  API_ASSERT(job_document.get_document_id().is_empty());
  API_ASSERT(job_document.get_permissions().is_empty() == false);

  Job job(job_document);
  job.set_type(type).set_key(crypto_key().get_key256_string()).save();

  m_id = job.get_document_id();

  cloud().create_database_object(
    "jobs",
    Job::Object().set_type(type),
    job.get_document_id());

  if (is_success()) {
    m_id = job.get_document_id();
  }

  const PathString job_path = PathString("jobs") / job.get_document_id();
  m_timeout_timer.restart();
  fs::LambdaFile listen_file;

  listen_file.set_context(this).set_write_callback(
    [](void *context, int location, const var::View view) -> int {
      Job::Server *self = reinterpret_cast<Job::Server *>(context);
      self->process_input(JsonDocument().from_string(
        StringView(view.to_const_char(), view.size())));

      return self->is_stop() == false ? view.size() : -1;
    });

  cloud().listen_database(job_path, listen_file);

  return *this;
}

void Job::Server::process_input(const json::JsonValue &data) {
  if (data.is_valid() && data.is_object()) {
    const Path path = Path("jobs") / id();

    Job::Object object
      = cloud().get_database_value(path.string_view()).to_object();

    auto input_list = object.get_input();

    for (const auto &input : input_list) {

      if (callback()) {

        JsonValue output = callback()(
          context(),
          object.get_type(),
          input_list.at(input.key()).decrypt_value(crypto_key()));

        cloud().create_database_object(
          Path(path) / "output",
          Job::IOValue("", crypto_key(), output).get_value(),
          input.key());

        cloud().remove_database_object(Path(path) / "input" / input.key());
      }
    }
  } else {
    if (callback()) {
      callback()(context(), "", JsonNull());
    }
  }
}
