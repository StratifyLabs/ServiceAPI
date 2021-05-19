// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include <crypto.hpp>
#include <fs.hpp>
#include <var.hpp>

#include "service/Report.hpp"

using namespace service;

Report::Report() : DocumentAccess("reports", "") {}

Report::Report(
  const Id &id,
  const fs::FileObject &destination,
  IsDownloadContents is_download_contents)
  : DocumentAccess("reports", id) {

  if (is_download_contents == IsDownloadContents::yes) {
    download_contents(destination);
  }
}

Report &Report::save(const fs::FileObject &content) {

  const size_t padding = Aes::get_padding(content.size());

  set_key(secret_key().get_key256_string())
    .set_iv(secret_key().get_initialization_vector_string())
    .set_padding(padding);

  DataFile encrypted_file;
  encrypted_file.write(
    DataFile().write(content).write(NullFile(padding)).seek(0),
    AesCbcEncrypter()
      .set_initialization_vector(secret_key().initialization_vector())
      .set_key256(secret_key().key256()));

  DocumentAccess<Report>::save();
  API_RETURN_VALUE_IF_ERROR(*this);

  cloud_service().storage().create_object(get_storage_path(), encrypted_file.seek(0));
  return *this;
}

void Report::download_contents(const fs::FileObject &destination) {

  if (get_key().is_empty()) {
    cloud_service().storage().get_object(get_storage_path(), destination);

  } else {

    DataFile encrypted_file;
    cloud_service().storage().get_object(get_storage_path(), encrypted_file);

    m_secret_key = Aes::Key(
      Aes::Key::Construct().set_initialization_vector(get_iv()).set_key(
        get_key()));

    const size_t truncated_size = encrypted_file.size() - get_padding();

    if (get_key().is_empty()) {
      destination.write(DataFile()
                          .write(encrypted_file.seek(0))
                          .resize(truncated_size)
                          .seek(0));
    } else {
      destination.write(
        DataFile()
          .write(
            encrypted_file.seek(0),
            AesCbcDecrypter()
              .set_initialization_vector(secret_key().initialization_vector())
              .set_key256(secret_key().key256()))
          .resize(truncated_size)
          .seek(0));
    }
  }
}
