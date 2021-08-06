// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include <chrono.hpp>
#include <crypto.hpp>
#include <fs.hpp>
#include <json.hpp>
#include <printer.hpp>
#include <sys.hpp>
#include <var.hpp>

#include "service/Build.hpp"
#include "service/Keys.hpp"
#include "service/Project.hpp"

using namespace service;

Project::Project(const Id &id) : DocumentAccess("projects", id) {}

json::JsonArray Project::list() {
  return JsonArray();
#if 0
  return Document::list(
    "projects",
    "mask.fieldPaths=name"
    "&mask.fieldPaths=description"
    "&mask.fieldPaths=version"
    "&mask.fieldPaths=github"
    "&mask.fieldPaths=documentId"
    "&mask.fieldPaths=permissions"
    "&mask.fieldPaths=type"
    "&mask.fieldPaths=tags");
#endif
}

Project &Project::save_build(const SaveBuild &options) {
  API_RETURN_VALUE_IF_ERROR(*this);

  API_ASSERT(options.project_path().is_empty() == false);
  // does the current version already exist
  sys::Version version(get_version());
  set_user_id(cloud_service().store().credentials().get_uid_cstring());

  // check the current version against the versions in the build list
  if (is_build_version_valid(version) == false) {
    return *this;
  }

  const StringView permissions = get_permissions();

  if (is_permissions_valid(permissions) == false) {
    return *this;
  }

  if ((permissions == "private") && get_team_id().is_empty()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EINVAL);
    return *this;
  }

  // if project hasn't been published before, upload it now and add the document
  // id
  if (id().is_empty()) {

    save();
    API_RETURN_VALUE_IF_ERROR(*this);

    printer().key("id", id().string_view());

    Build::Type type = Build::decode_build_type(get_type());
    const char *type_command = (type == Build::Type::os) ? "os" : "app";
    printer().key(
      "tip",
      String().format(
        "use `sl %s.publish:path=%s,header` to export settings to "
        "`sl_config.h`",
        type_command,
        String(options.project_path()).cstring()));
    return *this;
  }

  Project existing_project(id());

  if (is_success()) {
    printer().object(
      "existing project",
      existing_project,
      Printer::Level::trace);
  }

  if (get_team_id() != existing_project.get_team_id()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EPERM);
    return *this;
  }

  if (
    existing_project.get_team_id().is_empty()
    && existing_project.get_user_id()
         != cloud_service().store().credentials().get_uid()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EPERM);
    return *this;
  }

  // import the build and upload it
  Build build(Build::Construct()
                .set_project_path(options.project_path())
                .set_project_id(get_document_id()));

  // add the README if it is available

  {
    const PathString readme_path
      = PathString(options.project_path()) / "README.md";
    const bool is_readme_available = FileSystem().exists(readme_path);
    if (is_readme_available) {
      DataFile readme
        = DataFile().write(File(readme_path), Base64Encoder()).move();
      CLOUD_PRINTER_TRACE("setting readme to " + readme.data().string_view());
      set_readme(readme.data().string_view());
    }
  }

  crypto::Aes::Key key;

  if (options.sign_key().is_empty() == false) {
    Keys keys_document(options.sign_key());

    if (keys_document.is_existing() == false) {
      API_RETURN_VALUE_ASSIGN_ERROR(
        *this,
        "sign key does not exist " | options.sign_key(),
        EINVAL);
    }

    API_RETURN_VALUE_IF_ERROR(*this);

    auto dsa = keys_document.get_digital_signature_algorithm(
      Aes::Key(Aes::Key::Construct()
                 .set_key(options.sign_key_password())
                 .set_initialization_vector(keys_document.get_iv())));

    const auto list = build.build_image_list();

    build.sign(dsa);

    //the boot builds need to be keyed with the public key
    for (auto image_info: list) {
      if( image_info.get_name().find("_boot_") != StringView::npos ){
        printer::Printer::Object po(printer(), "addPublicKey_" & image_info.get_name());
        build.insert_public_key(image_info, keys_document.get_dsa_public_key().data());
      }
    }

    printer::Printer::Object po(printer(), "signatureInfo");
    printer()
      .key("keyId", keys_document.get_document_id())
      .key("publicKey", keys_document.get_public_key());

    auto show_section = [&](const char *name, const Data &data) {
      printer::Printer::Object po(printer(), name);
      const ViewFile view_file(data);

      const auto signature_info = sos::Auth::get_signature_info(view_file.seek(0));
      const auto is_verified
        = sos::Auth::verify(view_file.seek(0), keys_document.get_dsa_public_key());

      printer()
        .key("dataSize", NumberString(data.size()))
        .key("hash", View(signature_info.hash()).to_string<GeneralString>())
        .key("signature", signature_info.signature().to_string())
        .key_bool("isVerified", is_verified);

    };

    for (const auto &image_info : list) {
      printer::Printer::Object po(printer(), image_info.get_name());
      {
        const auto data = image_info.get_image_data();
        show_section("text/data", data);
      }

      const auto section_list = image_info.get_section_list();
      for (const auto &section : section_list) {
        const auto data = section.get_image_data();
        show_section(section.key().cstring(), data);
      }
    }

    API_RETURN_VALUE_IF_ERROR(*this);
  }

  CLOUD_PRINTER_TRACE("Creating and saving the build to the cloud");
  build.set_readme(get_readme())
    .set_description(options.change_description())
    .set_version(version.string_view())
    .set_team_id(get_team_id())
    .set_permissions(get_permissions())
    .set_key(key.get_key256_string())
    .set_iv(key.get_initialization_vector_string())
    .save();

  printer().object("buildUpload", build, printer::Printer::Level::trace);

  auto project_build_list = get_build_list();
  project_build_list.push_back(
    BuildItem(build.id()).set_version(version.string_view()));
  set_build_list(project_build_list);

  CLOUD_PRINTER_TRACE("Saving the project document");
  save();

  printer().object("projectUpload", to_object());

  {
    // readme may not be present so ignore the error
    api::ErrorScope es;
    remove_readme();
  }

  return *this;
}

bool Project::is_build_version_valid(const sys::Version &build_version) const {

  JsonKeyValueList<BuildItem> list = build_list();
  if (list.count() == 0) {
    return true;
  }

  for (const BuildItem &item : list) {
    sys::Version version(item.get_version());

    if (build_version <= version) {
      printer().warning(String().format(
        "current is not greater than latest %s <= %s",
        build_version.cstring(),
        version.cstring()));
      return false;
    }
  }

  return true;
}

Project::Path Project::get_storage_path(const SaveBuild &options) const {

  Id build_id = get_build_id(options.version());
  if (build_id.is_empty()) {
    return String();
  }

  Build build;
  build.set_type(get_type());
  build.set_application_architecture(options.architecture());
  const StringView normalized_build_name
    = build.normalize_name(options.build_name());

  return Project::Path("builds") / get_document_id() / build_id
         / normalized_build_name / get_name();
}

Project::Id Project::get_build_id(const var::StringView version) const {

  sys::Version latest_version("0.0");
  JsonKeyValueList<BuildItem> list = build_list();

  Id build_id;
  const sys::Version check_version(version);

  for (const BuildItem &item : list) {
    const sys::Version version(item.get_version());

    if (check_version.string_view().is_empty()) {
      // grab the latest
      if (version > latest_version) {
        latest_version = version;
        build_id = item.key().string_view();
      }

    } else if (version == check_version) {
      return Id(item.key().string_view());
    }
  }

  return build_id;
}

int Project::compare(const sys::Version &version) const {
  BuildList build_list = this->get_build_list();
  sys::Version latest_version;

  for (const BuildItem &item : build_list) {
    const sys::Version build_version(item.get_version());
    if (build_version > latest_version) {
      latest_version = build_version;
    }
  }

  return sys::Version::compare(latest_version, version);
}
