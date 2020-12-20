// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#ifndef SERVICE_API_SERVICE_INSTALLER_HPP
#define SERVICE_API_SERVICE_INSTALLER_HPP

#include <chrono/ClockTimer.hpp>
#include <sos/Link.hpp>
#include <var/String.hpp>

#include "Project.hpp"

namespace service {

class Installer : public cloud::CloudObject {
public:
  class Install {

    // if id is provided -- download and install
    API_ACCESS_COMPOUND(Install, var::StringView, project_id);
    API_ACCESS_COMPOUND(Install, var::StringView, team_id);
    API_ACCESS_COMPOUND(Install, var::StringView, url);
    API_ACCESS_BOOL(Install, update_os, false);
    API_ACCESS_BOOL(Install, update_apps, false);

    // update directories
    API_ACCESS_COMPOUND(Install, var::StringView, update_app_directories);

    // path is a path to a project
    API_ACCESS_COMPOUND(Install, var::StringView, project_path);
    API_ACCESS_COMPOUND(Install, var::StringView, version);
    API_ACCESS_COMPOUND(Install, var::StringView, build_name);

    // path to an image to install
    API_ACCESS_COMPOUND(Install, var::StringView, binary_path);
    API_ACCESS_BOOL(Install, application, false);
    API_ACCESS_BOOL(Install, os, false);

    API_ACCESS_COMPOUND(Install, var::StringView, build_path);

    // application options
    API_ACCESS_COMPOUND(Install, var::StringView, destination);
    API_ACCESS_COMPOUND(Install, var::StringView, suffix);
    API_ACCESS_COMPOUND(Install, var::StringView, architecture);
    API_ACCESS_BOOL(Install, tightly_coupled_data, false);
    API_ACCESS_BOOL(Install, tightly_coupled_code, false);
    API_ACCESS_BOOL(Install, external_data, false);
    API_ACCESS_BOOL(Install, external_code, false);
    API_ACCESS_BOOL(Install, clean, false);
    API_ACCESS_BOOL(Install, force, false);
    API_ACCESS_BOOL(Install, kill, false);
    API_ACCESS_BOOL(Install, flash, false);
    API_ACCESS_BOOL(Install, startup, false);
    API_ACCESS_BOOL(Install, authenticated, false);
    API_ACCESS_FUNDAMENTAL(Install, u32, ram_size, 0);
    API_ACCESS_FUNDAMENTAL(Install, u32, access_mode, 0555);

    // OS options
    API_ACCESS_BOOL(Install, verify, false);
    API_ACCESS_BOOL(Install, append_hash, false);
    API_ACCESS_BOOL(Install, reconnect, false);
    API_ACCESS_COMPOUND(Install, chrono::MicroTime, delay);
    API_ACCESS_FUNDAMENTAL(Install, u32, retry_reconnect_count, 50);

    API_ACCESS_BOOL(Install, insert_key, false);
    API_ACCESS_COMPOUND(Install, var::StringView, secret_key);

    // thing Install
    API_ACCESS_BOOL(
      Install,
      synchronize_thing,
      false); // keep thing synchronized to actions
    API_ACCESS_BOOL(
      Install,
      rekey_thing,
      false); // only valid if team is not empty

  public:
    Install() { set_delay(500_milliseconds); }
  };

  class AppUpdate {
    API_ACCESS_COMPOUND(AppUpdate, var::StringView, path);
    API_ACCESS_COMPOUND(AppUpdate, sos::Appfs::Info, info);

  public:
    bool is_valid() const { return path().is_empty() == false; }
  };

  explicit Installer(sos::Link *connection);

  Installer &install(const Install &options);
  Installer &operator()(const Install &options) { return install(options); }

  void print_transfer_info(
    const fs::FileObject &image,
    const chrono::ClockTimer &transfer_timer);

private:
  API_ACCESS_FUNDAMENTAL(Installer, sos::Link *, connection, nullptr);
  API_ACCESS_COMPOUND(Installer, var::StringView, project_name);
  API_ACCESS_COMPOUND(Installer, var::StringView, project_id);
  API_ACCESS_COMPOUND(Installer, var::StringView, architecture);

  void install_url(const Install &options);
  void install_id(const Install &options);
  void install_path(const Install &options);
  void install_binary(const Install &options);

  var::Vector<AppUpdate> get_app_update_list(const Install &options);
  var::Vector<AppUpdate> get_app_update_list_from_directory(
    const var::StringView directory_path,
    const Install &options);
  void
  update_apps(const var::Vector<AppUpdate> &app_list, const Install &options);
  void update_os(const Install &options);

  void import_build_from_project_path(const Install &options);

  void install_build(Build &build, const Install &options);

  void install_application_build(Build &build, const Install &options);

  void install_os_build(Build &build, const Install &options);

  void install_os_image(
    const Build &build,
    const fs::FileObject &image,
    const Install &options);

  void install_application_image(
    const fs::FileObject &image,
    const Install &options);

  void save_image_locally(
    const Build &build,
    const fs::FileObject &image,
    const Install &options);

  void reconnect(const Install &options);

  void kill_application(int app_pid);
  void clean_application();
};

} // namespace service

#endif // SERVICE_API_SERVICE_INSTALLER_HPP
