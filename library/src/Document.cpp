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

Document::Document(const var::StringView path, const Id &id) {
  if (id.is_empty() == false) {
    to_object() = cloud().get_document(
      Path(path).append("/").append(id.cstring()).cstring());
  }
}

Document::Document(const fs::File &file) {
  to_object() = JsonDocument().load(file);
}

var::String Document::save_to_cloud() {

  DocumentOptions updated_options(options);

  set_timestamp(DateTime::get_system_time().ctime());
  set_user_id(cloud().credentials().get_uid_cstring());
  convert_tags_to_list(); // tags -> tagList
  sanitize_tag_list();    // remove any tag duplicates

  if (get_team_id() == "") {
    // this will ensure 'team' is present in the object
    // get_team_id() can be "" if not present or if ""
    set_team_id("");
  }

  API_ASSERT(
    get_permissions() == "public" || get_permissions() == "private"
    || get_permissions() == "searchable");

  if (options.document_id().is_empty() || options.is_create()) {
    // if id.is_empty() then we add a new document to path otherwise modify a
    // document

    const var::String result = cloud().create_document(
      options.create_parent_path(),
      to_object(),
      options.document_id());

    if (result != "") {
      // once document is uploaded it should be modified to include the id
      set_document_id(result.cstring());
      updated_options.set_document_id(result);
    } else {
      JsonObject error
        = JsonDocument().from_string(cloud().document_error()).to_object();

      if (
        error.at(("error")).to_object().at(("status")).to_string()
        == "ALREADY_EXISTS") {

      } else {

        return String();
      }
    }
  }

  // add keys from object to update mask
  cloud().document_update_mask_fields().clear();
  JsonObject::KeyList key_list = to_object().key_list();
  // cloud().document_update_mask_fields() = to_object().key_list();
  for (const auto &key : key_list) {
    cloud().document_update_mask_fields().push_back(String(key.cstring()));
  }
  set_document_id(updated_options.document_id().cstring());
  cloud().patch_document(updated_options.create_full_path(), to_object());

  return updated_options.document_id();
}

Document &Document::load(var::StringView path) {
  to_object() = JsonDocument().load(path);
  convert_tags_to_list(); // tags -> tagList
  return *this;
}

Document &Document::save(var::StringView path) {
  convert_tags_to_list(); // tags -> tagList
  JsonDocument().save(*this, File(File::IsOverwrite::yes, path));
  return *this;
}

Document &
Document::import_binary_file_to_base64(var::StringView path, const char *key) {

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
  StringView tags(to_object().at("tags").to_cstring());
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
  var::StringView path,
  const char *key) const {

  var::StringView input(to_object().at(key).to_cstring());

  File(fs::File::IsOverwrite::yes, path)
    .write(ViewFile(View(input)), Base64Decoder());

  return *this;
}
