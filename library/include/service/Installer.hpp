#ifndef SERVICE_API_SERVICE_INSTALLER_HPP
#define SERVICE_API_SERVICE_INSTALLER_HPP

#include <chrono/ClockTimer.hpp>
#include <sos/Link.hpp>
#include <var/String.hpp>

#include "Project.hpp"

namespace service {

class Installer : public cloud::CloudObject {
public:
  class Options {

    // if id is provided -- download and install
    API_ACCESS_COMPOUND(Options, var::StringView, project_id);
    API_ACCESS_COMPOUND(Options, var::StringView, team_id);
    API_ACCESS_COMPOUND(Options, var::StringView, url);
    API_ACCESS_BOOL(Options, update_os, false);
    API_ACCESS_BOOL(Options, update_apps, false);

    // update directories
    API_ACCESS_COMPOUND(Options, var::StringView, update_app_directories);

    // path is a path to a project
    API_ACCESS_COMPOUND(Options, var::StringView, project_path);
    API_ACCESS_COMPOUND(Options, var::StringView, version);
    API_ACCESS_COMPOUND(Options, var::StringView, build_name);

    // path to an image to install
    API_ACCESS_COMPOUND(Options, var::StringView, binary_path);
    API_ACCESS_BOOL(Options, application, false);
    API_ACCESS_BOOL(Options, os, false);

    API_ACCESS_COMPOUND(Options, var::StringView, build_path);

    // application options
    API_ACCESS_COMPOUND(Options, var::StringView, destination);
    API_ACCESS_COMPOUND(Options, var::StringView, suffix);
    API_ACCESS_COMPOUND(Options, var::StringView, architecture);
    API_ACCESS_BOOL(Options, tightly_coupled_data, false);
    API_ACCESS_BOOL(Options, tightly_coupled_code, false);
    API_ACCESS_BOOL(Options, external_data, false);
    API_ACCESS_BOOL(Options, external_code, false);
    API_ACCESS_BOOL(Options, clean, false);
    API_ACCESS_BOOL(Options, force, false);
    API_ACCESS_BOOL(Options, kill, false);
    API_ACCESS_BOOL(Options, flash, false);
    API_ACCESS_BOOL(Options, startup, false);
    API_ACCESS_BOOL(Options, authenticated, false);
    API_ACCESS_FUNDAMENTAL(Options, u32, ram_size, 0);
    API_ACCESS_FUNDAMENTAL(Options, u32, access_mode, 0555);

    // OS options
    API_ACCESS_BOOL(Options, verify, false);
    API_ACCESS_BOOL(Options, append_hash, false);
    API_ACCESS_BOOL(Options, reconnect, false);
    API_ACCESS_COMPOUND(Options, chrono::MicroTime, delay);
    API_ACCESS_FUNDAMENTAL(Options, u32, retry_reconnect_count, 50);

    API_ACCESS_BOOL(Options, insert_key, false);
    API_ACCESS_COMPOUND(Options, var::StringView, secret_key);

    // thing options
    API_ACCESS_BOOL(
      Options,
      synchronize_thing,
      false); // keep thing synchronized to actions
    API_ACCESS_BOOL(
      Options,
      rekey_thing,
      false); // only valid if team is not empty

  public:
    Options() { set_delay(500_milliseconds); }
  };

  class AppUpdate {
    API_ACCESS_COMPOUND(AppUpdate, var::StringView, path);
    API_ACCESS_COMPOUND(AppUpdate, sos::Appfs::Info, info);

  public:
    bool is_valid() const { return path().is_empty() == false; }
  };

  explicit Installer(sos::Link *connection);

  Installer &install(const Options &options);

  void print_transfer_info(
    const fs::File &image,
    const chrono::ClockTimer &transfer_timer);

private:
  API_ACCESS_FUNDAMENTAL(Installer, sos::Link *, connection, nullptr);
  API_ACCESS_COMPOUND(Installer, var::StringView, project_name);
  API_ACCESS_COMPOUND(Installer, var::StringView, project_id);
  API_ACCESS_COMPOUND(Installer, var::StringView, architecture);

  bool install_url(const Options &options);
  bool install_id(const Options &options);
  bool install_path(const Options &options);
  bool install_binary(const Options &options);

  var::Vector<AppUpdate> get_app_update_list(const Options &options);
  var::Vector<AppUpdate> get_app_update_list_from_directory(
    const var::StringView directory_path,
    const Options &options);
  bool
  update_apps(const var::Vector<AppUpdate> &app_list, const Options &options);
  bool update_os(const Options &options);

  bool import_build_from_project_path(const Options &options);

  bool install_build(Build &build, const Options &options);

  bool install_application_build(Build &build, const Options &options);

  bool install_os_build(Build &build, const Options &options);

  bool install_os_image(
    const Build &build,
    const fs::File &image,
    const Options &options);

  bool install_application_image(const fs::File &image, const Options &options);

  int save_image_locally(
    const Build &build,
    const fs::File &image,
    const Options &options);

  bool reconnect(const Options &options);

  int kill_application(int app_pid);
  int clean_application();
};

} // namespace service

#endif // SERVICE_API_SERVICE_INSTALLER_HPP
