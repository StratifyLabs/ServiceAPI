#ifndef SERVICE_API_SERVICE_PROJECT_HPP
#define SERVICE_API_SERVICE_PROJECT_HPP

#include <sos/Link.hpp>
#include <sys/Version.hpp>
#include <var/String.hpp>

#include "Build.hpp"

namespace service {

/*!
 * \brief The Project class
 * \details A project can either be
 * an application, board support package or data.
 *
 */
class Project : public DocumentAccess<Project> {
public:
  class BuildItem : public json::JsonKeyValue {
  public:
    explicit BuildItem(
      const char *key,
      const json::JsonValue &value = json::JsonString())
      : JsonKeyValue(key, value) {}

    JSON_ACCESS_KEY_VALUE_PAIR_STRING(BuildItem, id, version);
  };

  class ThreadStackItem : public json::JsonKeyValue {
  public:
    explicit ThreadStackItem(
      const char *key,
      const json::JsonValue &value = json::JsonInteger())
      : JsonKeyValue(key, value) {}

    JSON_ACCESS_KEY_VALUE_PAIR_INTEGER(ThreadStackItem, name, stack_size);
  };

  using BuildList = json::JsonKeyValueList<BuildItem>;

  class BuildOptions {
  public:
  private:
    API_ACCESS_COMPOUND(BuildOptions, var::String, type);
    API_ACCESS_COMPOUND(BuildOptions, var::String, make_options);
    API_ACCESS_COMPOUND(BuildOptions, var::String, architecture);
    API_ACCESS_COMPOUND(BuildOptions, var::String, clean);
    API_ACCESS_COMPOUND(BuildOptions, var::String, generator);
    API_ACCESS_COMPOUND(BuildOptions, var::String, cmake_options);
    API_ACCESS_COMPOUND(BuildOptions, var::String, path);
    API_ACCESS_COMPOUND(BuildOptions, var::String, name);
    API_ACCESS_COMPOUND(BuildOptions, var::String, target);
    API_ACCESS_COMPOUND(BuildOptions, var::String, touch_path);
    API_ACCESS_BOOL(BuildOptions, dry_run, false);
    API_ACCESS_BOOL(BuildOptions, reconfigure, false);
  };

  explicit Project(const Id &id = Id());

  static var::StringView file_name() { return "sl_settings.json"; }

  // project has top-level description (including change history)
  // project has a list of builds (versioned-builds)
  // can each build contain the git hash data?
  // a project can also have a list of dependencies (versioned-builds)
  // dependencies can be applications or data

  class PublishBuild {
  public:
    // PublishBuild() { set_path("projects"); }

  private:
    API_ACCESS_BOOL(PublishBuild, dry_run, false);
    API_ACCESS_COMPOUND(PublishBuild, var::StringView, change_description);
    API_ACCESS_COMPOUND(PublishBuild, var::StringView, file_path);
    API_ACCESS_COMPOUND(PublishBuild, var::StringView, architecture);
    API_ACCESS_COMPOUND(PublishBuild, var::StringView, version);
    API_ACCESS_COMPOUND(PublishBuild, var::StringView, build_name);
  };

  Project &publish_build(const PublishBuild &options);
  inline Project &operator()(const PublishBuild &options) {
    return publish_build(options);
  }

  static const var::StringView application_type() {
    return Build::application_type();
  }

  static const var::StringView os_type() { return Build::os_type(); }
  static const var::StringView data_type() { return Build::data_type(); }

  JSON_ACCESS_STRING(Project, name);
  JSON_ACCESS_STRING(Project, version);
  JSON_ACCESS_STRING(Project, type);
  JSON_ACCESS_STRING(Project, github);
  JSON_ACCESS_STRING(Project, description);
  JSON_ACCESS_STRING_WITH_KEY(Project, hardwareId, hardware_id);
  JSON_ACCESS_STRING(Project, publisher);
  JSON_ACCESS_STRING(Project, permissions);
  JSON_ACCESS_STRING(Project, readme);
  JSON_ACCESS_STRING_WITH_KEY(Project, ramSize, ram_size);
  JSON_ACCESS_STRING_WITH_KEY(Project, team, team_id);
  JSON_ACCESS_OBJECT_LIST_WITH_KEY(Project, BuildItem, buildList, build_list);
  JSON_ACCESS_OBJECT_LIST_WITH_KEY(
    Project,
    ThreadStackItem,
    threadStackList,
    thread_stack_list);

  Build download_build(const var::StringView version) const;

  bool is_build_version_valid(const sys::Version &build_version) const;

  Id get_build_id(const var::StringView version) const;

  bool is_update_available(const var::String &current_version);

  bool operator<(const sys::Version &version) const {
    return compare(version) < 0;
  }

  bool operator>(const sys::Version &version) const {
    return compare(version) > 0;
  }

  bool operator>=(const sys::Version &version) const {
    return compare(version) >= 0;
  }

  bool operator<=(const sys::Version &version) const {
    return compare(version) <= 0;
  }

  bool operator==(const sys::Version &version) const {
    return compare(version) == 0;
  }

  bool operator!=(const sys::Version &version) const {
    return compare(version) != 0;
  }

  json::JsonArray list();

private:
  Path get_storage_path(const PublishBuild &options) const;

  int compare(const sys::Version &version) const;
};

} // namespace service

#endif // SERVICE_API_SERVICE_PROJECT_HPP
