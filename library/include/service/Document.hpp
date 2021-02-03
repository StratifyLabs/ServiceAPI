// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#ifndef CLOUD_API_CLOUD_DOCUMENT_HPP
#define CLOUD_API_CLOUD_DOCUMENT_HPP

#include <cloud/Cloud.hpp>
#include <crypto/Random.hpp>
#include <json/Json.hpp>
#include <printer/Printer.hpp>
#include <var/String.hpp>

namespace service {

class Document : public cloud::CloudAccess, public json::JsonObject {
public:
  using Id = var::KeyString;

  enum class Permissions { private_, public_, searchable };

  class Path : public var::StackString<Path, 256> {
  public:
    Path() {}
    Path(const char *a) : var::StackString<Path, 256>(a) {}
    Path(const var::StringView a) : var::StackString<Path, 256>(a) {}
    Path(const var::String &a) : var::StackString<Path, 256>(a.cstring()) {}

    Path &operator/(const char *a) { return append("/").append(a); }
    Path &operator/(const Id &a) { return append("/").append(a.cstring()); }
    Path &operator/(const Path &a) { return append("/").append(a.cstring()); }
    Path &operator/(const var::StringView a) { return append("/").append(a); }
    Path &operator/(const var::String &a) {
      return append("/").append(a.cstring());
    }

    // implicit conversion
    operator const char *() const { return m_buffer; }
    operator const var::StringView() {
			return var::StringView(m_buffer);
    }
  };

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
    API_AC(Tag, var::KeyString, key);
    API_AC(Tag, var::KeyString, value);
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

  bool is_existing() const { return m_is_existing; }

protected:
  Document &
  import_binary_file_to_base64(var::StringView path, const var::StringView key);

  const Document &export_base64_to_binary_file(
    const var::StringView path,
    const var::StringView key) const;

  void interface_import_file(const fs::File &file);
  void interface_export_file(const fs::File &file) const;
  virtual void interface_save();

protected:
  void set_id(const var::StringView id) { m_id = id; }

private:
  Path m_path;
  Id m_id;
  bool m_is_existing = false;

  Path get_path_with_id() const {
    return Path(path()).append("/").append(id());
  }

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

  Derived &export_file(const fs::File &a) {
    interface_export_file(a);
    return static_cast<Derived &>(*this);
  }

  Derived &save() {
    interface_save();
    return static_cast<Derived &>(*this);
  }

  Derived &set_id(const var::StringView id) {
    Document::set_id(id);
    return static_cast<Derived &>(*this);
  }

  Derived &import_file(const fs::File &a) {
    interface_import_file(a);
    return static_cast<Derived &>(*this);
  }

  Derived &set_permissions(Permissions permissions) {
    var::StringView p;
    switch (permissions) {
    case Permissions::private_:
      p = "private";
      break;
    case Permissions::public_:
      p = "public";
      break;
    case Permissions::searchable:
      p = "searchable";
      break;
    }
    set_permissions(p);
    return static_cast<Derived &>(*this);
  }

  API_WRITE_ACCESS_COMPOUND_ALIAS(
    Document,
    Derived,
    var::StringView,
    document_id)
  API_WRITE_ACCESS_COMPOUND_ALIAS(Document, Derived, var::StringView, team_id)
  API_WRITE_ACCESS_COMPOUND_ALIAS(Document, Derived, var::StringView, user_id)
  API_WRITE_ACCESS_COMPOUND_ALIAS(
    Document,
    Derived,
    var::StringView,
    permissions)
  API_WRITE_ACCESS_COMPOUND_ALIAS(Document, Derived, var::StringList, tag_list)
  API_WRITE_ACCESS_FUNDAMENTAL_ALIAS(Document, Derived, s32, timestamp)
};

} // namespace service

#endif // CLOUD_API_CLOUD_DOCUMENT_HPP
