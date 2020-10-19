#ifndef CLOUD_API_CLOUD_DOCUMENT_HPP
#define CLOUD_API_CLOUD_DOCUMENT_HPP

#include <json/Json.hpp>
#include <printer/Printer.hpp>
#include <var/String.hpp>

#include <cloud/Cloud.hpp>

namespace service {

class Document : public cloud::CloudAccess, public json::JsonValue {
public:
  using Id = var::StackString32;
  using Path = var::StackString256;

  class Tag {
  public:
    var::StackString64 get_tag() const {
      if (key().is_empty()) {
        return var::StackString64(value().cstring());
      } else {
        return var::StackString64(key().cstring())
          .append(":")
          .append(value().cstring());
      }
    }

  private:
    API_AC(Tag, var::StackString32, key);
    API_AC(Tag, var::StackString32, value);
  };

  static bool is_permissions_valid(var::StringView value) {
    if (value == "public" || value == "private" || value == "searchable") {
      return true;
    }
    return false;
  }

  static var::StringView get_valid_permissions() {
    return "`public`, `private`, or `searchable`";
  }

  // download from the cloud
  explicit Document(const var::StringView document_path, const Id &id = "");

  bool is_valid() const { return to_object().is_empty() == false; }

  // documents also have a list of authorized users
  JSON_ACCESS_STRING_WITH_KEY(Document, documentId, document_id);
  JSON_ACCESS_INTEGER(Document, timestamp);
  JSON_ACCESS_STRING_WITH_KEY(Document, team, team_id);
  JSON_ACCESS_STRING_WITH_KEY(Document, uid, user_id);
  JSON_ACCESS_STRING(Document, permissions);
  JSON_ACCESS_STRING_ARRAY_WITH_KEY(Document, tagList, tag_list);

  const Id &id() const { return m_id; }

  const Path &path() const { return m_path; }

protected:
  Document &import_binary_file_to_base64(var::StringView path, const char *key);

  const Document &
  export_base64_to_binary_file(var::StringView path, const char *key) const;

  virtual void interface_import_file(const fs::File &file);
  virtual void interface_export_file(const fs::File &file) const;
  virtual void interface_save();

private:
  Path m_path;
  Id m_id;

  void convert_tags_to_list();
  void sanitize_tag_list();
  static int
  import_json_recursive(const json::JsonObject &input, Document &output);
  static int
  export_json_recursive(const Document &input, json::JsonObject &output);
  static json::JsonObject
  convert_map_to_object(const json::JsonObject &input_map, var::StringView key);
  static json::JsonValue convert_field_to_value(
    const json::JsonObject &input_map,
    var::StringView key);

  var::Vector<json::JsonObject>
  list(var::StringView path, var::StringView mask);
};

template <class Derived> class DocumentAccess : public Document {
public:
  DocumentAccess(const var::StringView document_path, const Id &id)
    : Document(document_path, id) {}

  Derived &export_file(const fs::File &a) const {
    Document::interface_export_file(a);
    return static_cast<Derived &>(*this);
  }

  Derived &save() {
    Document::interface_save();
    return static_cast<Derived &>(*this);
  }

  Derived &import_file(const fs::File &a) {
    Document::interface_import_file(a);
    return static_cast<Derived &>(*this);
  }

  API_WRITE_ACCESS_COMPOUND_ALIAS(Document, Derived, var::String, document_id)
  API_WRITE_ACCESS_COMPOUND_ALIAS(Document, Derived, var::String, team_id)
  API_WRITE_ACCESS_COMPOUND_ALIAS(Document, Derived, var::String, user_id)
  API_WRITE_ACCESS_COMPOUND_ALIAS(Document, Derived, var::String, permissions)
  API_WRITE_ACCESS_COMPOUND_ALIAS(Document, Derived, var::StringList, tag_list)
  API_WRITE_ACCESS_FUNDAMENTAL_ALIAS(Document, Derived, s32, timestamp)
};

} // namespace service

#endif // CLOUD_API_CLOUD_DOCUMENT_HPP
