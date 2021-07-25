// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#ifndef SERVICE_API_SERVICE_BUILD_HPP
#define SERVICE_API_SERVICE_BUILD_HPP

#include <crypto/Ecc.hpp>
#include <crypto/Sha256.hpp>
#include <fs/DataFile.hpp>
#include <fs/ViewFile.hpp>
#include <sos/Auth.hpp>
#include <sos/Link.hpp>
#include <var/Base64.hpp>

#include "Document.hpp"

namespace service {

class Build : public DocumentAccess<Build> {

  static var::Data sign_data(const var::Data &data, const crypto::Dsa &dsa) {
    fs::DataFile data_file;
    data_file.data() = data;
    sos::Auth::sign(data_file, dsa);
    return data_file.data();
  }

public:
  class SecretKeyInfo {
  public:
    u32 invalid_position() const { return static_cast<u32>(-1); }

  private:
    API_ACCESS_FUNDAMENTAL(SecretKeyInfo, u32, position, invalid_position());
    API_ACCESS_FUNDAMENTAL(SecretKeyInfo, u32, size, 0);
  };

  class SectionImageInfo : public json::JsonKeyValue {
  public:
    explicit SectionImageInfo(const var::StringView key)
      : JsonKeyValue(key, json::JsonObject()) {}

    SectionImageInfo(const var::StringView key, const json::JsonObject &object)
      : JsonKeyValue(key, object) {}

    JSON_ACCESS_STRING(SectionImageInfo, image);
    JSON_ACCESS_INTEGER(SectionImageInfo, padding);
    JSON_ACCESS_INTEGER(SectionImageInfo, size);
    JSON_ACCESS_BOOL(SectionImageInfo, signed);

    var::Data get_image_data() const {
      return var::Base64().decode(var::StringView(get_image_cstring()));
    }

    SectionImageInfo &sign(const crypto::Dsa &dsa) {
      if (is_signed()) {
        return *this;
      }
      const auto signed_data = sign_data(get_image_data(), dsa);
      set_image_data(signed_data);
      set_size(signed_data.size());
      set_signed(true);
      return *this;
    }

    crypto::Dsa::Signature get_signature() const {
      auto data = get_image_data();
      return sos::Auth::get_signature(fs::ViewFile(data));
    }

    SectionImageInfo &set_image_data(var::View image_view) {
      return set_image(var::Base64().encode(image_view).string_view());
    }
  };

  class ImageInfo : public json::JsonValue {
  public:
    JSON_ACCESS_CONSTRUCT_OBJECT(ImageInfo);

    JSON_ACCESS_STRING(ImageInfo, name);
    JSON_ACCESS_STRING(ImageInfo, image);
    JSON_ACCESS_STRING(ImageInfo, hash);
    JSON_ACCESS_BOOL(ImageInfo, signed);
    JSON_ACCESS_INTEGER(ImageInfo, size);
    JSON_ACCESS_INTEGER(ImageInfo, padding);
    JSON_ACCESS_STRING_WITH_KEY(ImageInfo, secretKey, secret_key);
    JSON_ACCESS_STRING_WITH_KEY(ImageInfo, publicKey, public_key);
    JSON_ACCESS_INTEGER_WITH_KEY(
      ImageInfo,
      secretKeyPosition,
      secret_key_position);
    JSON_ACCESS_INTEGER_WITH_KEY(ImageInfo, secretKeySize, secret_key_size);
    JSON_ACCESS_OBJECT_LIST_WITH_KEY(
      ImageInfo,
      SectionImageInfo,
      sections,
      section_list);

    var::Data get_image_data() const {
      return var::Base64().decode(get_image());
    }

    ImageInfo &sign(const crypto::Dsa &dsa) {
      if (is_signed()) {
        return *this;
      }
      const auto signed_data = sign_data(get_image_data(), dsa);
      set_image_data(signed_data);
      set_size(signed_data.size());
      set_signed(true);
      auto local_section_list = section_list();
      for (auto &section : local_section_list) {
        section.sign(dsa);
      }
      return *this;
    }

    crypto::Dsa::Signature get_signature() const {
      auto data = get_image_data();
      return sos::Auth::get_signature(fs::ViewFile(data));
    }

    ImageInfo &set_image_data(const var::View &image_view) {
      return set_image(var::Base64().encode(image_view));
    }

    bool operator==(const ImageInfo &info) const {
      return get_name() == info.get_name();
    }
  };

  class Construct {
    API_AC(Construct, var::StringView, project_id);
    API_AC(Construct, var::StringView, build_id);

    API_AC(Construct, var::StringView, binary_path);
    API_AC(Construct, var::StringView, project_path);
    API_AC(Construct, var::StringView, build_name);
    API_AC(Construct, var::StringView, architecture);
    API_AC(Construct, var::StringView, url);
  };

  Build(const Construct &options = Construct());

  enum class Type { unknown, application, os, data };

  static const var::StringView application_type() { return "app"; }
  static const var::StringView os_type() { return "os"; }
  static const var::StringView data_type() { return "data"; }
  static const var::StringView path() { return "build"; }

  bool is_application() const;
  bool is_os() const;
  bool is_data() const;

  class ImportCompiled {
    API_ACCESS_COMPOUND(ImportCompiled, var::StringView, path);
    API_ACCESS_COMPOUND(ImportCompiled, var::StringView, build);

  public:
  };

  Build &import_compiled(const ImportCompiled &options);
  inline Build &operator()(const ImportCompiled &options) {
    return import_compiled(options);
  }

  JSON_ACCESS_STRING(Build, name);
  JSON_ACCESS_STRING(Build, version);
  JSON_ACCESS_STRING(Build, readme);
  JSON_ACCESS_STRING(Build, description);
  JSON_ACCESS_STRING(Build, type);
  JSON_ACCESS_STRING(Build, signature);
  JSON_ACCESS_STRING_WITH_KEY(Build, projectId, project_id);
  JSON_ACCESS_ARRAY_WITH_KEY(Build, ImageInfo, buildList, build_image_list);
  JSON_ACCESS_STRING_ARRAY_WITH_KEY(
    Build,
    buildList,
    build_image_list_20200518); // migration list
  JSON_ACCESS_STRING_WITH_KEY(Build, ramSize, ram_size);
  JSON_ACCESS_BOOL_WITH_KEY(Build, isBuildImage, image_included);
  JSON_ACCESS_STRING(Build, key);
  JSON_ACCESS_STRING(Build, iv);

  Build &remove_build_image_data() {
    auto build_list = build_image_list();
    for (ImageInfo &image_info : build_list) {
      image_info.set_image("<base64>");

      auto section_list = image_info.section_list();
      for (auto &section_info : section_list) {
        section_info.set_image("<base64>");
      }
    }
    set_image_included(false);
    return *this;
  }

  Build &remove_other_build_images(const var::StringView keep_name) {
    var::Vector<ImageInfo> build_list = build_image_list();
    var::Vector<ImageInfo> updated_build_list;
    for (ImageInfo &image_info : build_list) {
      if (image_info.get_name() == normalize_name(keep_name).string_view()) {
        updated_build_list.push_back(image_info);
      }
    }
    set_build_image_list(updated_build_list);
    return *this;
  }

  SecretKeyInfo get_secret_key(const var::StringView build_name) {
    const ImageInfo image_info = build_image_info(normalize_name(build_name));
    if (image_info.is_valid()) {
      return SecretKeyInfo()
        .set_position(image_info.get_secret_key_position())
        .set_size(image_info.get_secret_key_size());
    }
    return SecretKeyInfo();
  }

  ImageInfo build_image_info(const var::StringView build_name) const {
    return build_image_list().find(
      ImageInfo().set_name(normalize_name(build_name).cstring()));
  }

  Build &insert_secret_key(
    const var::StringView build_name,
    const var::View secret_key);

  Build &insert_public_key(
    const var::StringView build_name,
    const var::View public_key);

  Build &insert_public_key(ImageInfo &image_info, const var::View public_key);

  bool is_build_image_included() const {
    // buildIncludesImage is a legacy support thing
    return is_image_included()
           || to_object().at(("buildIncludesImage")).to_bool();
  }

  Build &import_url(const var::StringView url);
  // int download(const BuildOptions &options);

  var::Data get_image(const var::StringView name) const;
  Build &set_image(const var::StringView name, const var::Data &image);

  var::NameString normalize_name(const var::StringView build_name) const;
  var::NameString normalize_elf_name(
    const var::StringView project_name,
    const var::StringView build_directory) const;

  static Type decode_build_type(const var::StringView type);
  static var::StringView encode_build_type(Type type);

  Type decode_build_type() const;

  Build &set_application_architecture(const var::StringView value) {
    m_application_architecture = value;
    return *this;
  }

  Build &sign(const crypto::Dsa &dsa);

  var::StringView application_architecture() const {
    if (decode_build_type() == Type::application) {
      return m_application_architecture;
    }
    return var::StringView();
  }

protected:
  void interface_save() override;
  void interface_remove() override;

private:
  var::KeyString m_application_architecture;

  Document::Path create_storage_path(const var::StringView build_name) const {
    return Document::Path("builds") / get_project_id() / id()
           / normalize_name(build_name).string_view() / get_name();
  }

  var::PathString
  get_build_file_path(const var::StringView path, const var::StringView build);

  class ImportElfFile {
    API_AC(ImportElfFile, json::JsonObject, project_settings);
    API_AC(ImportElfFile, var::StringView, path);
    API_AC(ImportElfFile, var::StringView, build_name);
  };

  ImageInfo import_elf_file(const var::StringView path);

  var::String calculate_hash(var::Data &image);
  void migrate_build_info_list_20200518();

  static var::StringView get_arch(const var::StringView name);

  bool insert_pure_code_secret_key(
    var::Data &image_data,
    const var::View secret_key);
};

} // namespace service

#endif // SERVICE_API_SERVICE_BUILD_HPP
