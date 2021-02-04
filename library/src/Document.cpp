// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include <chrono.hpp>
#include <fs.hpp>
#include <json.hpp>
#include <printer.hpp>
#include <var.hpp>

#include "service/Document.hpp"

using namespace service;

var::Vector<json::JsonObject>
Document::list(var::StringView path, var::StringView mask) {
  JsonObject response = cloud().list_documents(path, mask);
  var::Vector<json::JsonObject> result;
  JsonArray documents = response.at("documents").to_array();
  for (u32 i = 0; i < documents.count(); i++) {
    String name = documents.at(i).to_object().at("name").to_string();

    // parse name down to the ID
    size_t pos = StringView(name).reverse_find(path);
    if (pos != String::npos) {
      name(String::Erase().set_length(pos));
    }

    result.push_back(
      JsonObject()
        .insert("name", JsonString(name.cstring()))
        .insert(
          "document",
          cloud::CloudMap(documents.at(i).to_object()).to_json()));
  }

  return result;
}

Document::Document(const var::StringView path, const Id &id)
  : m_path(path), m_id(id) {
  if (id.is_empty() == false) {
    api::ErrorGuard error_guard;
    to_object() = cloud().get_document(Path(path) / id);
    m_is_existing = is_success();
    CLOUD_PRINTER_TRACE(
      "document " | (Path(path) / id).string_view() | " exists? "
      | (m_is_existing ? "true" : "false"));
  }
}

void Document::interface_save() {
  API_RETURN_IF_ERROR();

  if (m_is_imported) {
    // check to see if doc exists
    m_is_imported = false;
    if (get_document_id().is_empty() == false) {
      api::ErrorGuard error_guard;
      cloud().get_document(Path(path()) / get_document_id());
      m_is_existing = is_success();
    }
  }

  CLOUD_PRINTER_TRACE(
    "saving document to cloud " | path().string_view() | " id: "
    | get_document_id());
  CLOUD_PRINTER_TRACE(
    "is existing? "
    | (is_existing() ? StringView("true") : StringView("false")));
  set_timestamp(DateTime::get_system_time().ctime());
  set_user_id(cloud().credentials().get_uid_cstring());
  {
    api::ErrorGuard error_guard;
    convert_tags_to_list(); // tags -> tagList
    sanitize_tag_list();    // remove any tag duplicates
  }

  if (get_team_id() == "") {
    CLOUD_PRINTER_TRACE("no team specified, ensure `team` entry");
    // this will ensure 'team' is present in the object
    // get_team_id() can be "" if not present or if ""
    set_team_id("");
  }

  API_ASSERT(
    get_permissions() == "public" || get_permissions() == "private"
    || get_permissions() == "searchable");

  if (get_document_id().is_empty() || !m_is_existing) {
    CLOUD_PRINTER_TRACE("document path is " | path().string_view());
    CLOUD_PRINTER_TRACE("creating new document with id: " | get_document_id());
    const auto result = cloud().create_document(
      path().string_view(),
      to_object(),
      get_document_id());

    CLOUD_PRINTER_TRACE("new document id is " | result);
    if (result != "") {
      // once document is uploaded it should be modified to include the id
      m_id = result;
    } else {
      CLOUD_PRINTER_TRACE("there was an error creating the document");
      JsonObject error
        = JsonDocument().from_string(cloud().document_error()).to_object();
      if (
        error.at("error").to_object().at("status").to_string()
        == "ALREADY_EXISTS") {
        API_RETURN_ASSIGN_ERROR("", EEXIST);
      } else {
        API_RETURN_ASSIGN_ERROR("", EIO);
        return;
      }
    }
  }

  // add keys from object to update mask
  cloud().document_update_mask_fields().clear();
  set_document_id(id());
  cloud().patch_document(get_path_with_id().string_view(), to_object());
  return;
}

void Document::interface_import_file(const fs::File &file) {
  to_object() = JsonDocument().load(file);
  m_id = get_document_id();
  convert_tags_to_list(); // tags -> tagList
  m_is_imported = true;
}

void Document::interface_export_file(const fs::File &file) const {
  JsonDocument().save(*this, file);
}

Document &Document::import_binary_file_to_base64(
  var::StringView path,
  const var::StringView key) {

  File input(path, fs::OpenMode::read_only());

  to_object().insert(
    key,
    JsonString(DataFile()
                 .reserve(input.size())
                 .write(input, Base64Encoder())
                 .data()
                 .add_null_terminator()));
  return *this;
}

void Document::convert_tags_to_list() {
  StringView tags(to_object().at("tags").to_string_view());
  if (tags.is_empty() == false) {
    set_tag_list(tags.split(","));
    to_object().remove("tags");
  }
}

void Document::sanitize_tag_list() {
  var::StringViewList list = get_tag_list();
  var::StringList sanitized_list;

  for (const auto &item : list) {
    bool is_present = false;
    for (const auto &sanitized_item : sanitized_list) {
      if (item == sanitized_item) {
        is_present = true;
      }
    }
    if (!is_present) {
      sanitized_list.push_back(String(item));
    }
  }

  set_tag_list(sanitized_list);
}

const Document &Document::export_base64_to_binary_file(
  const var::StringView path,
  const var::StringView key) const {

  var::StringView input(to_object().at(key).to_string_view());

  File(fs::File::IsOverwrite::yes, path)
    .write(ViewFile(View(input)), Base64Encoder());

  return *this;
}
