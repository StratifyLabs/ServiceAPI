#include <chrono.hpp>
#include <fs.hpp>
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
    set_architecture(String(connection()->sys_info().arch()));
  } else {
    set_architecture(options.architecture());
  }

  const Vector<AppUpdate> app_update_list = options.is_update_apps()
                                              ? get_app_update_list(options)
                                              : Vector<AppUpdate>();

  if (options.is_update_os()) {
    if (
      update_os(Options(options).set_reconnect(
        options.is_reconnect() || options.is_update_apps()))
      == false) {
      return *this;
    }

    if (options.is_update_apps() == false) {
      return *this;
    } else {
      // need to reconnect
    }
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

bool Installer::install_url(const Options &options) {
  Build b = Build(Build::Construct().set_url(options.url()));
  return install_build(b, options);
}

bool Installer::install_id(const Options &options) {
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

bool Installer::install_binary(const Options &options) {

  if (fs::Path(options.binary_path()).suffix() == "json") {
    Build b = Build(Build::Construct().set_binary_path(options.binary_path()));
    set_project_name(String(b.get_name()));
    return install_build(b, options);
  }

  DataFile image = std::move(DataFile().write(File(options.binary_path())));

  if (options.is_application()) {
    Appfs::Info source_image_info = Appfs().get_info(options.binary_path());

    if (source_image_info.is_valid() == false) {
      API_RETURN_VALUE_ASSIGN_ERROR(false, "invalid image", EINVAL);
      return false;
    }

    set_project_name(source_image_info.name());
    set_project_id(source_image_info.id());

    return install_application_image(image, options);
  }

  if (options.is_os()) {
    return install_os_image(Build(Build::Construct()), image, options);
  }

  return false;
}

bool Installer::install_path(const Options &options) {
  Build b(Build::Construct()
            .set_project_path(options.project_path())
            .set_build_name(options.build_name())
            .set_architecture(architecture()));

  set_project_id(String(b.get_project_id()));
  set_project_name(String(fs::Path(options.project_path()).name()));

  if (
    b.decode_build_type() == Build::Type::application
    && !options.is_application()) {
    API_RETURN_VALUE_ASSIGN_ERROR(false, "app type mismatch", false);
  }

  if (b.decode_build_type() == Build::Type::os && !options.is_os()) {
    API_RETURN_VALUE_ASSIGN_ERROR(false, "os type mismatch", false);
  }

  return install_build(b, options);
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

  FileSystem::PathList directory_list
    = FileSystem(connection()->driver()).read_directory(Dir(directory_path));

  for (const auto item : directory_list) {
    fs::Path full_path = fs::Path(directory_path).append("/").append(item);
    Appfs::Info info = Appfs(connection()->driver()).get_info(full_path);

    if (info.is_valid()) {
      result.push_back(
        AppUpdate().set_info(info).set_path(String(full_path.cstring())));
    }
  }

  return result;
}

bool Installer::update_apps(
  const var::Vector<AppUpdate> &app_list,
  const Options &options) {
  for (const AppUpdate &app : app_list) {
    Project app_project(Project::Id(app.info().id()));

    sys::Version current_version = sys::Version::from_u16(app.info().version());

    PrinterObject po(printer(), app_project.get_name());
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
      if (
        install_id(Options(options)
                     .set_os(false)
                     .set_application()
                     .set_project_id(app_project.get_document_id())
                     .set_team_id(app_project.get_team_id()))
        == false) {
        return false;
      }
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

  return true;
}

bool Installer::update_os(const Options &options) {

  Project os_project(Project::Id(connection()->sys_info().id()));

  sys::Version current_version(connection()->sys_info().system_version());

  PrinterObject po(printer(), os_project.get_name());
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

  return true;
}

bool Installer::install_build(Build &build, const Options &options) {

  if (build.decode_build_type() == Build::Type::application) {
    CLOUD_PRINTER_TRACE("installing application build");
    return install_application_build(build, options);
  }

  if (build.decode_build_type() == Build::Type::os) {
    CLOUD_PRINTER_TRACE("installing OS build");
    return install_os_build(build, options);
  }

  set_error_message("cannot install build of type `" + build.get_type() + "`");
  return false;
}

bool Installer::install_application_build(
  Build &build,
  const Options &options) {

  CLOUD_PRINTER_TRACE("load image for " + options.build_name());
  DataFile image(OpenMode::read_write());
  image.data() = build.get_image(options.build_name());

  if (image.data().size() == 0) {
    set_error_message(
      "can't install build with name `"
      + build.normalize_name(options.build_name()) + "`");
    CLOUD_PRINTER_TRACE("image not found for " + options.build_name());
    return false;
  }

  printer().key(
    "build",
    build.normalize_name(options.build_name()).string_view());
  printer().key("version", build.get_version());
  return install_application_image(
    image,
    Options(options).set_version(build.get_version()));
}

bool Installer::install_os_build(Build &build, const Options &options) {

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

    StringView thing_team = connection()->sys_info().team_id();
    if (thing_team.is_empty()) {
      thing_team = options.team_id();
    }

    if (options.secret_key().is_empty() && !options.is_rekey_thing()) {
      Thing thing(connection()->sys_info());
      if (thing.is_error()) {
        // no thing there
        API_RETURN_VALUE_ASSIGN_ERROR(
          false,
          "secret key not accessible",
          EPERM);
        return false;
      }

      existing_secret_key = thing.get_secret_key();
    }

    build.insert_secret_key(
      options.build_name(),
      var::Data::from_string(existing_secret_key));

    StringView secret_key
      = build.build_image_info(options.build_name()).get_secret_key();

    PrinterObject po(printer(), "secretKey");
    printer().key("key256", secret_key);
    printer().key(
      "key128",
      secret_key.get_substring_with_length(secret_key.length() / 2));
  }

  // append hash
  if (options.is_append_hash()) {
    build.append_hash(options.build_name());
  }

  if (fs::Path(options.destination()).suffix() == "json") {
    Link::Path link_path(options.destination(), connection()->driver());

    build.remove_other_build_images(options.build_name());

    if (link_path.is_host_path()) {
      build.export_file(File(File::IsOverwrite::yes, link_path.path()));

      printer().key("destination", link_path.path_description());
      return true;
    } else {
      API_RETURN_VALUE_ASSIGN_ERROR(
        false,
        "cannot save JSON export to device (use `host@` prefix)",
        EINVAL);
      return false;
    }
  }

  DataFile image(OpenMode::read_only());
  image.data() = build.get_image(options.build_name());

  printer().key(
    "build",
    build.normalize_name(options.build_name()).string_view());

  printer().key("version", build.get_version());

  bool result = install_os_image(
    build,
    image,
    Options(options).set_reconnect(
      options.is_reconnect() || options.is_synchronize_thing()));

  if (result && options.is_synchronize_thing()) {
    // update the thing with the new version that is installed
    PrinterObject po(printer(), "thing");

    Thing thing(connection()->sys_info());
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

  return result;
}

bool Installer::install_application_image(
  const fs::File &image,
  const Options &options) {


  {
    int result;

    result
      = save_image_locally(Build(), image, Options(options).set_application());

    if (result >= 0) {
      return result;
    }
  }

  int app_pid = connection()->get_pid(project_name());
  CLOUD_PRINTER_TRACE("pid is " + String::number(app_pid));
  if (options.is_kill()) {
    if (app_pid > 0) {
      printer().key("kill", project_name());
      if (kill_application(app_pid) < 0) {
        return false;
      }
    } else {
      CLOUD_PRINTER_TRACE(project_name() + " is not running");
    }
  } else {
    if (app_pid > 0 && !options.is_force()) {
      set_error_message(project_name() + " is currently running");
      set_error_message(
        "`sl` cannot install an application "
        "if the application is currently executing. "
        "If you specifiy `kill=true` when installing, "
        "the application will be sent a kill signal "
        "as part of the installation.");
      CLOUD_PRINTER_TRACE("application is already running");
      return false;
    }
  }

  if (options.is_clean()) {
    CLOUD_PRINTER_TRACE("clean");
    clean_application();
  }

  bool is_flash_available;

  if (options.is_flash()) {
    is_flash_available
      = Appfs::is_flash_available(fs::File::LinkDriver(connection()->driver()));
  } else {
    is_flash_available = false;
  }

  CLOUD_PRINTER_TRACE(
    is_flash_available ? String("Flash is available")
                       : String("Flash not available"));

  sys::Version version;
  version.string() = options.version();

  AppfsFileAttributes attributes;
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
    .set_version(version.to_bcd16());

  {
    PrinterObject po(printer(), "appfsAttributes");
    printer().key("name", attributes.name());
    printer().key("version", version.string());
    printer().key("id", attributes.id());
    printer().key("flash", attributes.is_flash());
    printer().key("startup", attributes.is_startup());
    printer().key("externalcode", attributes.is_code_external());
    printer().key("externaldata", attributes.is_data_external());
    printer().key(
      "tightlycoupledcode",
      attributes.is_code_tightly_coupled() != 0);
    printer().key(
      "tightlycoupleddata",
      attributes.is_data_tightly_coupled() != 0);
    printer().key("authenticated", attributes.is_authenticated());
    if (attributes.ram_size() == 0) {
      printer().key("ramsize", String("<default>"));
    } else {
      printer().key("ramsize", String::number(attributes.ram_size()));
    }
  }

  if (attributes.apply(image) < 0) {
    set_error_message("failed to apply file attributes to image");
    return -1;
  }

  if (connection()->is_connected()) {
    CLOUD_PRINTER_TRACE("Is connected");
  } else {
    set_error_message("Not connected");
    return false;
  }

  String destination
    = options.destination().is_empty() ? String("/app") : options.destination();

  Timer transfer_timer;
  printer().progress_key() = "installing";
  transfer_timer.start();
  int result = connection()->install_app(
    image,
    fs::File::Path(destination),
    sos::Link::ApplicationName(attributes.name()),
    printer().progress_callback());
  transfer_timer.stop();
  printer().progress_key() = "progress";

  if (result < 0) {
    set_error_message(
      "Failed to install application " + connection()->error_message());
    return false;
  }
  print_transfer_info(image, transfer_timer);
  return true;
}

bool Installer::install_os_image(
  const Build &build,
  const File &image,
  const Options &options) {
  int result;

  result = save_image_locally(build, image, Options(options).set_os());

  if (result >= 0) {
    return result;
  }

  PrinterObject po(printer(), "bootloader");
  {
    if (!connection()->is_bootloader()) {

      // bootloader must be invoked
      CLOUD_PRINTER_TRACE("invoking bootloader");
      result = connection()->reset_bootloader();
      if (result < 0) {
        set_error_message("Failed to invoke the bootloader");
        set_error_message(
          "Failed to invoke bootloader with connnection"
          " error message "
          + connection()->error_message());

        return false;
      }

      CLOUD_PRINTER_TRACE(
        "waiting " + String::number(options.delay().milliseconds()) + "ms");
      options.delay().wait();

      // now reconnect to the device
      reconnect(options);
    }

    CLOUD_PRINTER_TRACE("Installing OS");
    Timer transfer_timer;
    printer().progress_key() = "installing";
    transfer_timer.start();
    result = connection()->update_os(
      image,
      sos::Link::IsVerify(options.is_verify()),
      printer(),
      sos::Link::BootloaderRetryCount(options.retry_reconnect_count()));
    transfer_timer.stop();
    printer().progress_key() = "progress";
    if (result < 0) {
      CLOUD_PRINTER_TRACE(String().format(
        "failed to install with return value %d -> %s",
        result,
        connection()->error_message().cstring()));
      return false;
    }

    print_transfer_info(image, transfer_timer);

    if (connection()->reset() < 0) {
      set_error_message(
        "Failed to reset the OS with connection error "
        "message: "
        + connection()->error_message());
      return false;
    }
  }

  if (options.is_reconnect()) {
    reconnect(options);
  }

  return true;
}

bool Installer::reconnect(const Options &options) {
  CLOUD_PRINTER_TRACE(String().format(
    "reconnect %d retries at %dms intervals",
    options.retry_reconnect_count(),
    options.delay().milliseconds()));

  printer().progress_key() = "reconnecting";
  for (u32 i = 0; i < options.retry_reconnect_count(); i++) {
    if (
      connection()->reconnect(
        sos::Link::RetryCount(1),
        sos::Link::RetryDelay(options.delay()))
      == 0) {
      break;
    }
    printer().update_progress(
      static_cast<int>(i),
      ProgressCallback::indeterminate_progress_total());
  }
  if (connection()->is_connected() == false) {
    set_error_message("failed to reconnect: " + connection()->error_message());
    return false;
  }

  printer().update_progress(0, 0);
  printer().progress_key() = "progress";
  return true;
}

int Installer::save_image_locally(
  const Build &build,
  const fs::File &image,
  const Options &options) {
  if (!options.destination().is_empty()) {
    CLOUD_PRINTER_TRACE("saving image to " + options.destination());
    String destination;
    LinkPath link_path(options.destination(), connection()->driver());

    if (link_path.is_host_path()) {

      FileInfo info = File::get_info(link_path.path());

      if (link_path.path().is_empty() || info.is_directory()) {
        // if directory do <dir>/<project>_<build_name> with .bin for os images
        destination
          = (link_path.path().is_empty() ? String() : (link_path.path() + "/"))
            + project_name() + "_"
            + Build()
                .set_type(
                  options.is_os() ? Build::os_type()
                                  : Build::application_type())
                .set_application_architecture(architecture())
                .normalize_name(options.build_name())
            + (options.is_os() ? ".bin" : "");

      } else {
        destination = link_path.path();
      }

      if (image.save_copy(destination, File::IsOverwrite(true)) < 0) {
        set_error_message(
          "failed to create destination"
          "file "
          + destination);
        return 0;
      }
      printer().key("image", "host@" + destination);

      JsonKeyValueList<BuildSectionImageInfo> section_image_info
        = build.build_image_info(options.build_name()).get_section_list();

      for (const BuildSectionImageInfo &image_info : section_image_info) {
        String section_destination = FileInfo::no_suffix(destination);
        section_destination += "." + image_info.key() + ".bin";
        printer().key(image_info.key(), "host@" + section_destination);

        if (
          image_info.get_image_data().save(
            section_destination,
            Reference::IsOverwrite(true))
          < 0) {
          printer().warning(
            "failed to save section binary " + section_destination);
        }
      }

      return 1;
    } else {
      if (options.is_os()) {
        set_error_message(
          "cannot write the os directly to the"
          "device filesystem at "
          + link_path.path_description());
        return 0;
      }

      return -1;
    }
  }
  return -1;
}

int Installer::kill_application(int app_pid) {

  if (app_pid <= 0) {
    return 0;
  }

  CLOUD_PRINTER_TRACE(
    String().format("killing %s:%d", project_name().cstring(), app_pid));
  if (connection()->kill_pid(app_pid, LINK_SIGINT) < 0) {
    return -1;
  }

  // give a little time for the program to shut down
  CLOUD_PRINTER_TRACE("Wait for killed program to stop");
  int retries = 0;
  while (((app_pid = connection()->get_pid(project_name())) > 0)
         && (retries < 5)) {
    retries++;
    chrono::wait(chrono::Milliseconds(100));
  }

  return 0;
}

int Installer::clean_application() {
  String unlink_path;
  int count = 0;
  printer().key("clean", project_name());
  unlink_path = String("/app/flash/") + project_name();
  while (File::remove(unlink_path, fs::File::LinkDriver(connection()->driver()))
         >= 0) {
    // delete all versions
    count++;
  }

  unlink_path = String("/app/ram/") + project_name();
  while (File::remove(unlink_path, fs::File::LinkDriver(connection()->driver()))
         >= 0) {
    count++;
  }
  return count;
}

void Installer::print_transfer_info(
  const fs::File &image,
  const chrono::Timer &transfer_timer) {

  const u32 size = image.size();
  PrinterObject po(printer(), "transfer");
  printer().key("size", "%d", size);

  printer().key(
    "duration",
    "%0.3fs",
    transfer_timer.microseconds() * 1.0f / 1000000.0f);

  printer().key(
    "rate",
    "%0.3fKB/s",
    size * 1.0f / transfer_timer.microseconds() * 1000000.0f / 1024.0f);
}
