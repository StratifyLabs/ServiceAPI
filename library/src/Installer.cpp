#include <chrono.hpp>
#include <fs.hpp>
#include <json.hpp>
#include <printer.hpp>
#include <sos.hpp>
#include <var.hpp>

#include "service/Installer.hpp"
#include "service/Thing.hpp"

using namespace service;

Installer::Installer(sos::Link *link_connection)
  : m_connection(link_connection) {}

Installer &Installer::install(const Options &options) {

  if (connection()->is_connected_and_is_not_bootloader()) {
    set_architecture(String(connection()->info().sys_info().arch()));
  } else {
    set_architecture(options.architecture());
  }

  const Vector<AppUpdate> app_update_list = options.is_update_apps()
                                              ? get_app_update_list(options)
                                              : Vector<AppUpdate>();

  if (options.is_update_os()) {
    update_os(Options(options).set_reconnect(
      options.is_reconnect() || options.is_update_apps()));

    options.is_update_apps();
    if (is_success()) {
      // reconnect
    }
    return *this;
  }

  if (options.is_update_apps()) {
    update_apps(app_update_list, options);
    return *this;
  }

  if (!options.url().is_empty()) {
    install_url(options);
    return *this;
  }

  if (!options.project_id().is_empty()) {
    install_id(options);
    return *this;
  }

  if (!options.binary_path().is_empty()) {
    install_binary(options);
    return *this;
  }

  if (!options.project_path().is_empty()) {
    install_path(options);
    return *this;
  }

  return *this;
}

void Installer::install_url(const Options &options) {
  Build b = Build(Build::Construct().set_url(options.url()));
  return install_build(b, options);
}

void Installer::install_id(const Options &options) {
  Project p(Project::Id(options.project_id()));
  set_project_name(p.get_name());
  set_project_id(p.get_document_id());

  Build b(Build::Construct()
            .set_project_id(options.project_id())
            .set_architecture(architecture())
            .set_build_name(options.build_name())
            .set_build_id(p.get_build_id(options.version())));

  b.set_type(p.get_type());
  b.set_application_architecture(architecture());

  return install_build(b, options);
}

void Installer::install_binary(const Options &options) {

  if (fs::Path::suffix(options.binary_path()) == "json") {
    Build b = Build(Build::Construct().set_binary_path(options.binary_path()));
    set_project_name(String(b.get_name()));
    return install_build(b, options);
  }

  DataFile image = std::move(DataFile().write(File(options.binary_path())));

  if (options.is_application()) {
    Appfs::Info source_image_info = Appfs().get_info(options.binary_path());

    if (source_image_info.is_valid() == false) {
      API_RETURN_ASSIGN_ERROR("invalid image", EINVAL);
    }

    set_project_name(source_image_info.name());
    set_project_id(source_image_info.id());

    install_application_image(image, options);
    return;
  }

  if (options.is_os()) {
    return install_os_image(Build(Build::Construct()), image, options);
  }
}

void Installer::install_path(const Options &options) {
  Build b(Build::Construct()
            .set_project_path(options.project_path())
            .set_build_name(options.build_name())
            .set_architecture(architecture()));

  set_project_id(String(b.get_project_id()));
  set_project_name(String(fs::Path::name(options.project_path())));

  if (
    b.decode_build_type() == Build::Type::application
    && !options.is_application()) {
    API_RETURN_ASSIGN_ERROR("app type mismatch", false);
  }

  if (b.decode_build_type() == Build::Type::os && !options.is_os()) {
    API_RETURN_ASSIGN_ERROR("os type mismatch", false);
  }

  install_build(b, options);
}

var::Vector<Installer::AppUpdate>
Installer::get_app_update_list(const Options &options) {
  var::Vector<AppUpdate> result;
  StringViewList directory_list = options.update_app_directories().split("?");
  for (const auto item : directory_list) {
    result << (get_app_update_list_from_directory(item, options));
  }
  return result;
}

var::Vector<Installer::AppUpdate> Installer::get_app_update_list_from_directory(
  const var::StringView directory_path,
  const Options &options) {
  var::Vector<AppUpdate> result;

  fs::PathList directory_list
    = FileSystem(connection()->driver()).read_directory(Dir(directory_path));

  for (const auto item : directory_list) {
    var::PathString full_path = var::PathString(directory_path) / item;
    Appfs::Info info = Appfs(connection()->driver()).get_info(full_path);

    if (info.is_valid()) {
      result.push_back(
        AppUpdate().set_info(info).set_path(String(full_path.cstring())));
    }
  }

  return result;
}

void Installer::update_apps(
  const var::Vector<AppUpdate> &app_list,
  const Options &options) {
  for (const AppUpdate &app : app_list) {
    Project app_project(Project::Id(app.info().id()));

    sys::Version current_version = sys::Version::from_u16(app.info().version());

    Printer::Object po(printer(), app_project.get_name());
    printer().key("id", app_project.get_document_id());
    if (!app_project.get_team_id().is_empty()) {
      printer().key("team", app_project.get_team_id());
    }
    printer().key("type", app_project.get_type());
    if (app_project > current_version) {
      printer().key("currentVersion", current_version.string_view());
      printer().key("latestVersion", app_project.get_version());
      printer().key(
        "update",
        String(app_project.get_version()) + " from "
          + current_version.string_view());

      install_id(Options(options)
                   .set_os(false)
                   .set_application()
                   .set_project_id(app_project.get_document_id())
                   .set_team_id(app_project.get_team_id()));

    } else {
      printer().key("currentVersion", current_version.string_view());
      printer().key("latestVersion", app_project.get_version());
      printer().key(
        "update",
        String(current_version.string_view()) + " is the latest version");
    }
  }

  if (app_list.count() == 0) {
    printer().key("update", "no apps available to check for updates");
  }
}

void Installer::update_os(const Options &options) {

  Project os_project(Project::Id(connection()->info().sys_info().id()));

  sys::Version current_version(
    connection()->info().sys_info().system_version());

  Printer::Object po(printer(), os_project.get_name());
  printer().key("id", os_project.get_document_id());
  if (!os_project.get_team_id().is_empty()) {
    printer().key("team", os_project.get_team_id());
  }
  printer().key("type", os_project.get_type());
  if (os_project > current_version) {
    printer().key("currentVersion", current_version.string_view());
    printer().key("latestVersion", os_project.get_version());
    printer().key(
      "update",
      String(os_project.get_version()) + " from "
        + current_version.string_view());
    return install_id(Options(options)
                        .set_os()
                        .set_application(false)
                        .set_project_id(os_project.get_document_id())
                        .set_team_id(os_project.get_team_id())
                        .set_synchronize_thing());
  } else {
    printer().key("currentVersion", current_version.string_view());
    printer().key("latestVersion", os_project.get_version());
    printer().key(
      "update",
      current_version.string_view() + " is the latest version");
  }
}

void Installer::install_build(Build &build, const Options &options) {

  if (build.decode_build_type() == Build::Type::application) {
    install_application_build(build, options);
    return;
  }

  if (build.decode_build_type() == Build::Type::os) {
    install_os_build(build, options);
    return;
  }
}

void Installer::install_application_build(
  Build &build,
  const Options &options) {

  DataFile image(OpenMode::read_write());
  image.data() = build.get_image(options.build_name());

  if (image.size() == 0) {
    API_RETURN_ASSIGN_ERROR("", EINVAL);
  }

  printer().key(
    "build",
    build.normalize_name(options.build_name()).string_view());
  printer().key("version", build.get_version());

  install_application_image(
    image,
    Options(options).set_version(build.get_version()));
}

void Installer::install_os_build(Build &build, const Options &options) {

  // insert secret key
  if (options.is_insert_key()) {

    CLOUD_PRINTER_TRACE("inserting secret key");
    /*
     * Options for the key in preferential order
     *
     * 1. If a key is provided, use it
     * 2. if synchronize_thing() and is NOT rekey() and team is not empty -- use
     * the thing key
     * 3. Generate a new key
     *
     * 2 and 3 and handled as part of Build::insert_secret_key()
     *
     *
     */

    StringView existing_secret_key = options.secret_key();

    StringView thing_team = connection()->info().sys_info().team_id();
    if (thing_team.is_empty()) {
      thing_team = options.team_id();
    }

    if (options.secret_key().is_empty() && !options.is_rekey_thing()) {
      Thing thing(connection()->info().sys_info());
      if (thing.is_error()) {
        // no thing there
        API_RETURN_ASSIGN_ERROR("secret key not accessible", EPERM);
      }

      existing_secret_key = thing.get_secret_key();
    }

    build.insert_secret_key(
      options.build_name(),
      var::Data::from_string(existing_secret_key));

    StringView secret_key
      = build.build_image_info(options.build_name()).get_secret_key();

    Printer::Object po(printer(), "secretKey");
    printer().key("key256", secret_key);
    printer().key(
      "key128",
      secret_key.get_substring_with_length(secret_key.length() / 2));
  }

  // append hash
  if (options.is_append_hash()) {
    build.append_hash(options.build_name());
  }

  if (fs::Path::suffix(options.destination()) == "json") {
    Link::Path link_path(options.destination(), connection()->driver());

    build.remove_other_build_images(options.build_name());

    if (link_path.is_host_path()) {
      build.export_file(File(File::IsOverwrite::yes, link_path.path()));

      printer().key("destination", link_path.path_description());
      return;
    } else {
      API_RETURN_ASSIGN_ERROR(
        "cannot save JSON export to device (use `host@` prefix)",
        EINVAL);
    }
  }

  DataFile image(OpenMode::read_only());
  image.data() = build.get_image(options.build_name());

  printer().key(
    "build",
    build.normalize_name(options.build_name()).string_view());

  printer().key("version", build.get_version());

  install_os_image(
    build,
    image,
    Options(options).set_reconnect(
      options.is_reconnect() || options.is_synchronize_thing()));

  if (is_success() && options.is_synchronize_thing()) {
    // update the thing with the new version that is installed
    Printer::Object po(printer(), "thing");

    Thing thing(connection()->info().sys_info());
    thing.set_secret_key(
      build.build_image_info(options.build_name()).get_secret_key());

    printer() << thing;

    if (Document::is_permissions_valid(build.get_permissions())) {
      if (thing.get_permissions().is_empty()) {
        thing.set_permissions(String(build.get_permissions()));
      }

      thing.save();
    }
  }
}

void Installer::install_application_image(
  const fs::FileObject &image,
  const Options &options) {

  {
    save_image_locally(
      Build(Build::Construct()),
      image,
      Options(options).set_application());

    if (is_error()) {
      return;
    }
  }

  int app_pid = sos::TaskManager().get_pid(project_name());
  if (options.is_kill()) {
    if (app_pid > 0) {
      printer().key("kill", project_name());
      kill_application(app_pid);
    }
  } else {
    if (app_pid > 0 && !options.is_force()) {
      return;
    }
  }

  if (options.is_clean()) {
    clean_application();
  }

  const bool is_flash_available
    = options.is_flash() ? Appfs(connection()->driver()).is_flash_available()
                         : false;

  if (is_error()) {
    return;
  }

  sys::Version version(options.version());

  Appfs::FileAttributes attributes;
  attributes.set_name(project_name() + options.suffix())
    .set_id(project_id())
    .set_startup(options.is_startup())
    .set_flash(is_flash_available)
    .set_code_external(options.is_external_code())
    .set_data_external(options.is_external_data())
    .set_code_tightly_coupled(options.is_tightly_coupled_code())
    .set_data_tightly_coupled(options.is_tightly_coupled_data())
    .set_ram_size(options.ram_size())
    .set_authenticated(options.is_authenticated())
    .set_access_mode(options.access_mode())
    .set_version(version.to_bcd16())
    .apply(image);

  {
    Printer::Object po(printer(), "appfsAttributes");
    printer().key("name", attributes.name());
    printer().key("version", version.string_view());
    printer().key("id", attributes.id());
    printer().key_bool("flash", attributes.is_flash());
    printer().key_bool("startup", attributes.is_startup());
    printer().key_bool("externalcode", attributes.is_code_external());
    printer().key_bool("externaldata", attributes.is_data_external());
    printer().key_bool(
      "tightlycoupledcode",
      attributes.is_code_tightly_coupled() != 0);
    printer().key_bool(
      "tightlycoupleddata",
      attributes.is_data_tightly_coupled() != 0);
    printer().key_bool("authenticated", attributes.is_authenticated());
    if (attributes.ram_size() == 0) {
      printer().key("ramsize", String("<default>"));
    } else {
      printer().key(
        "ramsize",
        NumberString(attributes.ram_size()).string_view());
    }
  }

  if (connection()->is_connected() == false) {
    API_RETURN_ASSIGN_ERROR("not connected", EIO);
  }

  printer().progress_key() = "installing";
  chrono::ClockTimer transfer_timer;
  transfer_timer.start();
  sos::Appfs(
    Appfs::Construct()
      .set_executable(true)
      .set_overwrite(true)
      .set_mount(
        options.destination().is_empty() ? "/app" : options.destination())
      .set_name(attributes.name()))
    .append(image, printer().progress_callback());
  transfer_timer.stop();
  printer().progress_key() = "progress";

  if (is_success()) {
    print_transfer_info(image, transfer_timer);
  }

  return;
}

void Installer::install_os_image(
  const Build &build,
  const FileObject &image,
  const Options &options) {

  save_image_locally(build, image, Options(options).set_os());
  Printer::Object po(printer(), "bootloader");
  {
    if (!connection()->is_bootloader()) {

      // bootloader must be invoked
      connection()->reset_bootloader();
      chrono::wait(options.delay());
      // now reconnect to the device
      reconnect(options);
    }

    if (is_error()) {
      return;
    }

    ClockTimer transfer_timer;
    printer().progress_key() = "installing";
    transfer_timer.start();
    connection()->update_os(
      Link::UpdateOs()
        .set_image(&image)
        .set_bootloader_retry_count(options.retry_reconnect_count())
        .set_printer(&printer())
        .set_verify(options.is_verify()));

    transfer_timer.stop();
    printer().progress_key() = "progress";

    if (is_error()) {
      return;
    }

    print_transfer_info(image, transfer_timer);

    connection()->reset();
  }

  if (options.is_reconnect()) {
    reconnect(options);
  }

  return;
}

void Installer::reconnect(const Options &options) {
  CLOUD_PRINTER_TRACE(String().format(
    "reconnect %d retries at %dms intervals",
    options.retry_reconnect_count(),
    options.delay().milliseconds()));

  printer().progress_key() = "reconnecting";
  for (u32 i = 0; i < options.retry_reconnect_count(); i++) {
    connection()->reconnect(1, options.delay());
    if (is_success()) {
      break;
    }
    API_RESET_ERROR();
    printer().update_progress(
      static_cast<int>(i),
      api::ProgressCallback::indeterminate_progress_total());
  }
  if (connection()->is_connected() == false) {
    return;
  }

  printer().update_progress(0, 0);
  printer().progress_key() = "progress";
}

void Installer::save_image_locally(
  const Build &build,
  const fs::FileObject &image,
  const Options &options) {
  if (!options.destination().is_empty()) {
    CLOUD_PRINTER_TRACE("saving image to " + options.destination());
    String destination;
    Link::Path link_path(options.destination(), connection()->driver());

    if (link_path.is_host_path()) {

      FileInfo info = FileSystem().get_info(link_path.path());

      if (link_path.path().is_empty() || info.is_directory()) {
        // if directory do <dir>/<project>_<build_name> with .bin for os images
        destination
          = (link_path.path().is_empty() ? String() : (link_path.path() + "/"))
            + project_name() + "_"
            + Build(Build::Construct())
                .set_type(
                  options.is_os() ? Build::os_type()
                                  : Build::application_type())
                .set_application_architecture(architecture())
                .normalize_name(options.build_name())
            + (options.is_os() ? ".bin" : "");

      } else {
        destination = link_path.path().get_string();
      }

      File(File::IsOverwrite::yes, destination).write(image);

      printer().key("image", "host@" + destination);

      JsonKeyValueList<Build::SectionImageInfo> section_image_info
        = build.build_image_info(options.build_name()).get_section_list();

      for (const Build::SectionImageInfo &image_info : section_image_info) {
        var::PathString section_destination(fs::Path::no_suffix(destination));

        section_destination.append(".").append(image_info.key()).append(".bin");
        printer().key(image_info.key(), "host@" + section_destination);

        File(File::IsOverwrite::yes, section_destination)
          .write(image_info.get_image_data());
      }
    }
  }
}

void Installer::kill_application(int app_pid) {
  API_ASSERT(app_pid > 0);

  TaskManager task_manager(connection()->driver());
  task_manager.kill_pid(app_pid, LINK_SIGINT);

  // give a little time for the program to shut down
  int retries = 0;
  while (((app_pid = task_manager.get_pid(project_name())) > 0)
         && (retries < 5)) {
    retries++;
    chrono::wait(100_milliseconds);
  }
}

void Installer::clean_application() {
  {
    printer().key("clean", project_name());
    const var::PathString unlink_path
      = var::PathString("/app/flash") / project_name();

    while (is_success()) {
      FileSystem(connection()->driver()).remove(unlink_path.string_view());
      // delete all versions
    }
    API_RESET_ERROR();
  }

  {
    const var::PathString unlink_path
      = var::PathString("/app/ram") / project_name();
    while (is_success()) {
      FileSystem(connection()->driver()).remove(unlink_path.string_view());
    }
    API_RESET_ERROR();
  }
}

void Installer::print_transfer_info(
  const fs::FileObject &image,
  const chrono::ClockTimer &transfer_timer) {
  API_RETURN_IF_ERROR();

  const u32 size = image.size();
  Printer::Object po(printer(), "transfer");
  printer().key("size", NumberString(size).string_view());

  printer().key(
    "duration",
    NumberString(transfer_timer.microseconds() * 1.0f / 1000000.0f, "%0.3fs")
      .string_view());

  printer().key(
    "rate",
    NumberString(
      size * 1.0f / transfer_timer.microseconds() * 1000000.0f / 1024.0f,
      "%0.3fKB/s")
      .string_view());
}
