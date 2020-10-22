
#include <chrono.hpp>
#include <fs.hpp>
#include <json.hpp>
#include <printer.hpp>
#include <sys.hpp>
#include <var.hpp>

#include "service/Build.hpp"
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

Project &Project::publish_build(const PublishBuild &options) {

  // does the current version already exist
  sys::Version version(get_version());
  set_user_id(cloud().credentials().get_uid_cstring());

  // check the current version against the versions in the build list
  if (is_build_version_valid(version) == false) {
    return *this;
  }

  const StringView permissions = get_permissions();

  if (is_permissions_valid(permissions) == false) {
    return *this;
  }

  if ((permissions == "private") && get_team_id().is_empty()) {
    return *this;
  }

  // if project hasn't been published before, upload it now and add the document
  // id
  if (id().is_empty()) {

    save();
    if (is_error()) {
      return *this;
    }

    printer().key("id", id().string_view());

    Build::Type type = Build::decode_build_type(get_type());
    String type_command;
    if (type == Build::Type::os) {
      type_command = "os";
    } else {
      type_command = "app";
    }
    printer().key(
      "tip",
      String().format(
        "build project with `sl %s.build:path=%s` then use `sl "
        "%s.publish:path=%s,changes=<change description>`",
        String(type_command).cstring(),
        String(options.file_path()).cstring(),
        String(type_command).cstring(),
        String(options.file_path()).cstring()));
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
    set_error_message(
      "the local team ID `" + get_team_id()
      + "` does not match the team for the published project `"
      + existing_project.get_team_id() + "`");
    return *this;
  }

  if (
    existing_project.get_team_id().is_empty()
    && existing_project.get_user_id() != cloud().credentials().get_uid()) {
    // set_error_message("project permissions error (not owner)");
    return *this;
  }

  // import the build and upload it
  CLOUD_PRINTER_TRACE("import build from " + options.file_path());
  Build build(Build::Construct().set_project_path(options.file_path()));

  // add the README if it is available

  DataFile readme = std::move(DataFile().write(
    File((var::PathString(options.file_path()) / "README.md").string_view()),
    Base64Encoder()));

  CLOUD_PRINTER_TRACE("loaded readme " + readme.data().string_view());
  set_readme(readme.data().string_view());
  build.set_readme(readme.data().string_view());
  build.set_description(options.change_description());
  build.set_version(version.string_view());
  build.set_team_id(get_team_id());
  build.set_permissions(get_permissions());

  CLOUD_PRINTER_TRACE("assigned readme to build");
  String build_id;
  // printer().open_array("build.upload");
  CLOUD_PRINTER_TRACE("uploading build");
  build.save().remove_build_image_data();

  printer().object("buildUpload", build.to_object());

  CLOUD_PRINTER_TRACE("update project build list");

  BuildList project_build_list = get_build_list();
  project_build_list.push_back(
    BuildItem(build_id).set_version(version.string_view()));

  set_build_list(project_build_list);

  save();

  printer().object("projectUpload", to_object());
  remove_readme();

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

Project::Path Project::get_storage_path(const PublishBuild &options) const {

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

Build Project::download_build(const var::StringView version) const {
  return Build(Build::Construct()
                 .set_project_id(get_document_id())
                 .set_build_id(get_build_id(version)));
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
        build_id = item.key();
      }

    } else if (version == check_version) {
      return Id(item.key());
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
