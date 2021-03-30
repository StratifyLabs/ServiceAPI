// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include <chrono.hpp>
#include <crypto.hpp>
#include <fs.hpp>
#include <json.hpp>
#include <printer.hpp>
#include <sos.hpp>
#include <thread.hpp>
#include <var.hpp>

#include "service/Installer.hpp"
#include "service/Thing.hpp"

using namespace service;

Installer::Installer(sos::Link *link_connection)
  : m_connection(link_connection) {}

Installer &Installer::install(const Install &options) {
  API_RETURN_VALUE_IF_ERROR(*this);

  if (connection()->is_connected_and_is_not_bootloader()) {
    set_architecture(connection()->info().architecture());
  } else {
    set_architecture(options.architecture());
  }

  const Vector<AppUpdate> app_update_list = options.is_update_apps()
                                              ? get_app_update_list(options)
                                              : Vector<AppUpdate>();

  if (options.is_update_os()) {
    update_os(Install(options).set_reconnect(
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

void Installer::install_url(const Install &options) {
  Build b = Build(Build::Construct().set_url(options.url()));
  return install_build(b, options);
}

void Installer::install_id(const Install &options) {
  API_RETURN_IF_ERROR();
  Project p(Project::Id(options.project_id()));
  set_project_name(p.get_name());
  set_project_id(p.get_document_id());

  Build b(Build::Construct()
            .set_project_id(options.project_id())
            .set_architecture(architecture())
            .set_build_name(options.build_name())
            .set_build_id(p.get_build_id(options.version())));

  b.set_type(p.get_type());

  return install_build(b, options);
}

void Installer::install_binary(const Install &options) {
  CLOUD_PRINTER_TRACE("install binary at " | options.binary_path());
  API_RETURN_IF_ERROR();

  if (fs::Path::suffix(options.binary_path()) == "json") {
    Build b = Build(Build::Construct().set_binary_path(options.binary_path()));
    set_project_name(String(b.get_name()));
    return install_build(b, options);
  }

  // check if the binary is an elf file
  if (FileSystem().exists(options.binary_path()) == false) {
    API_RETURN_ASSIGN_ERROR(
      options.binary_path() | " binary path not found",
      EINVAL);
  }

  CLOUD_PRINTER_TRACE("load image from binary path");
  DataFile image = DataFile()
                     .write(File(options.binary_path()))
                     .set_flags(OpenMode::read_write())
                     .seek(0)
                     .move();

  if (options.is_application()) {
    CLOUD_PRINTER_TRACE("binary is an application");
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
    CLOUD_PRINTER_TRACE("install OS image from binary");
    return install_os_image(Build(Build::Construct()), image, options);
  }
}

void Installer::install_path(const Install &options) {
  API_RETURN_IF_ERROR();
  CLOUD_PRINTER_TRACE("installing from path " & options.project_path());

  Build b(Build::Construct()
            .set_project_path(options.project_path())
            .set_build_name(options.build_name())
            .set_architecture(architecture()));

  if (is_error()) {
    CLOUD_PRINTER_TRACE("error constructing build. aborting");
    return;
  }

  CLOUD_PRINTER_TRACE("setting installer project id to " | b.get_project_id());
  set_project_id(b.get_project_id());
  set_project_name(fs::Path::name(options.project_path()));
  CLOUD_PRINTER_TRACE("setting installer project name to " | project_name());

  if (
    b.decode_build_type() == Build::Type::application
    && !options.is_application()) {
    CLOUD_PRINTER_TRACE("project type != application");
    API_RETURN_ASSIGN_ERROR("app type mismatch", false);
  }

  if (b.decode_build_type() == Build::Type::os && !options.is_os()) {
    CLOUD_PRINTER_TRACE("project type != os");
    API_RETURN_ASSIGN_ERROR("os type mismatch", false);
  }

  CLOUD_PRINTER_TRACE("installing the build");
  install_build(b, options);
}

var::Vector<Installer::AppUpdate>
Installer::get_app_update_list(const Install &options) {
  var::Vector<AppUpdate> result;
  StringViewList directory_list = options.update_app_directories().split("?");
  for (const auto item : directory_list) {
    result << (get_app_update_list_from_directory(item, options));
  }
  return result;
}

var::Vector<Installer::AppUpdate> Installer::get_app_update_list_from_directory(
  const var::StringView directory_path,
  const Install &options) {
  MCU_UNUSED_ARGUMENT(options);
  var::Vector<AppUpdate> result;

  fs::PathList directory_list
    = Link::FileSystem(connection()->driver()).read_directory(directory_path);

  for (const auto &item : directory_list) {
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
  const Install &options) {
  for (const AppUpdate &app : app_list) {
    Project app_project(Project::Id(app.info().id()));

    sys::Version current_version = sys::Version::from_u16(app.info().version());

    Printer::Object po(printer(), app_project.get_name());
    printer().key("id", app_project.get_document_id());
    printer().key(
      "team",
      app_project.get_team_id().is_empty() ? "<none>"
                                           : app_project.get_team_id());

    printer().key("type", app_project.get_type());
    if (app_project > current_version) {
      printer().key("currentVersion", current_version.string_view());
      printer().key("latestVersion", app_project.get_version());
      printer().key(
        "update",
        String(app_project.get_version()) + " from "
          + current_version.string_view());

      install_id(Install(options)
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

void Installer::update_os(const Install &options) {

  Project os_project(Project::Id(connection()->info().id()));

  sys::Version current_version(connection()->info().system_version());

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
    return install_id(Install(options)
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

void Installer::install_build(Build &build, const Install &options) {
  API_RETURN_IF_ERROR();
  CLOUD_PRINTER_TRACE("Installing build type " | build.get_type());
  if (build.decode_build_type() == Build::Type::application) {
    CLOUD_PRINTER_TRACE("installing application build");
    install_application_build(build, options);
    return;
  }

  if (build.decode_build_type() == Build::Type::os) {
    CLOUD_PRINTER_TRACE("installing os build");
    install_os_build(build, options);
    return;
  }

  CLOUD_PRINTER_TRACE("build type was not recognized " | build.get_type());
}

void Installer::install_application_build(
  Build &build,
  const Install &options) {

  DataFile image(OpenMode::read_write());
  image.data() = build.get_image(options.build_name());

  if (image.size() == 0) {
    API_RETURN_ASSIGN_ERROR(
      ("could not find build `" + options.build_name().to_string() + "`")
        .cstring(),
      ENOENT);
  }

  printer().key(
    "build",
    build.normalize_name(options.build_name()).string_view());
  printer().key("version", build.get_version());

  if (options.is_append_hash()) {
    const auto hash = crypto::Sha256::append_aligned_hash(image);
    printer().key("hash", View(hash).to_string());
  }

  CLOUD_PRINTER_TRACE("installing application image");
  install_application_image(
    image.seek(0),
    Install(options).set_version(build.get_version()));
}

void Installer::install_os_build(Build &build, const Install &options) {

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
    CLOUD_PRINTER_TRACE("existing key is `" | options.secret_key() | "`");

    StringView thing_team = connection()->info().team_id();
    if (thing_team.is_empty()) {
      thing_team = options.team_id();
    }

    if (existing_secret_key.is_empty() && !options.is_rekey_thing()) {
      CLOUD_PRINTER_TRACE("getting secret key from cloud");
      Thing thing(Sys::Info(connection()->info().sys_info()));
      if (thing.is_existing() == false) {
        // no thing there
        API_RETURN_ASSIGN_ERROR("secret key not accessible", EPERM);
      }

      existing_secret_key = thing.get_secret_key();
      CLOUD_PRINTER_TRACE("got secret key `" | options.secret_key() | "`");
    }

    CLOUD_PRINTER_TRACE("using key `" | existing_secret_key | "`");
    // if existing_secret_key secret key is empty, insert_secret_key() generates
    // a key
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

  DataFile image;
  image.data() = build.get_image(options.build_name());

  if (options.is_append_hash()) {
    const crypto::Sha256::Hash hash
      = crypto::Sha256::append_aligned_hash(image);
    printer().key("osHash", View(hash).to_string());
  }

  printer().key(
    "build",
    build.normalize_name(options.build_name()).string_view());

  printer().key("version", build.get_version());

  install_os_image(
    build,
    image,
    Install(options).set_reconnect(
      options.is_reconnect() || options.is_synchronize_thing()));

  if (is_success() && options.is_synchronize_thing()) {
    // update the thing with the new version that is installed
    Printer::Object po(printer(), "thing");

    Thing thing(Sys::Info(connection()->info().sys_info()));
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
  const Install &options) {

  if (!options.destination().is_empty()) {
    CLOUD_PRINTER_TRACE("save locally");
    save_image_locally(
      Build(Build::Construct()),
      image,
      Install(options).set_application());
    return;
  }

  CLOUD_PRINTER_TRACE("check flash available");
  const bool is_flash_available
    = options.is_flash() ? Appfs(connection()->driver()).is_flash_available()
                         : false;

  API_RETURN_IF_ERROR();

  if (
    !is_flash_available
    && (options.destination().is_empty() || (options.destination().find("/app") == 0))
    && Appfs(connection()->driver()).is_ram_available() == false) {
    // no RAM and no Flash
    CLOUD_PRINTER_TRACE("no flash or ram");
    API_RETURN_ASSIGN_ERROR("device@/app/.install", ENOENT);
  }

  CLOUD_PRINTER_TRACE("check is running " & project_name());
  int app_pid
    = sos::TaskManager("", connection()->driver()).get_pid(project_name());
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

  Appfs::FileAttributes attributes(image.seek(0));

  const auto version = options.version().is_empty()
                         ? sys::Version::from_u16(attributes.version())
                         : sys::Version(options.version());

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

  printer().object("appfsAttributes", attributes);

  if (connection()->is_connected() == false) {
    API_RETURN_ASSIGN_ERROR("not connected", EIO);
  }

  printer().set_progress_key("installing");
  chrono::ClockTimer transfer_timer;
  transfer_timer.start();
  sos::Appfs(
    Appfs::Construct()
      .set_executable(true)
      .set_overwrite(true)
      .set_mount(
        options.destination().is_empty() ? "/app" : options.destination())
      .set_name(attributes.name()),
    connection()->driver())
    .append(image.seek(0), printer().progress_callback());
  transfer_timer.stop();
  printer().set_progress_key("progress");

  if (is_success()) {
    print_transfer_info(image, transfer_timer);
  }

  return;
}

void Installer::install_os_image(
  const Build &build,
  const FileObject &image,
  const Install &options) {
  API_RETURN_IF_ERROR();

  if (!options.destination().is_empty()) {
    CLOUD_PRINTER_TRACE("save build image locally");
    save_image_locally(build, image, Install(options).set_os());
    return;
  }

  {
    Printer::Object po(printer(), "bootloader");
    if (!connection()->is_bootloader()) {
      CLOUD_PRINTER_TRACE("invoke the bootloader");
      // bootloader must be invoked
      connection()->reset_bootloader();
      chrono::wait(options.delay());
      // now reconnect to the device

      reconnect(options);
    } else {
      CLOUD_PRINTER_TRACE("connected to bootloader");
    }

    API_RETURN_IF_ERROR();

    ClockTimer transfer_timer;
    printer().set_progress_key("installing");
    transfer_timer.start();
    CLOUD_PRINTER_TRACE("start installing the OS");
    connection()->update_os(
      Link::UpdateOs()
        .set_image(&(image.seek(0)))
        .set_bootloader_retry_count(options.retry_reconnect_count())
        .set_printer(&printer())
        .set_verify(options.is_verify()));

    transfer_timer.stop();
    printer().set_progress_key("progress");

    API_RETURN_IF_ERROR();

    print_transfer_info(image, transfer_timer);

    connection()->reset();
  }

  if (options.is_reconnect()) {
    reconnect(options);
  }

  return;
}

void Installer::reconnect(const Install &options) {
  CLOUD_PRINTER_TRACE(String().format(
    "reconnect %d retries at %dms intervals",
    options.retry_reconnect_count(),
    options.delay().milliseconds()));

  printer().set_progress_key("reconnecting");
  for (u32 i = 0; i < options.retry_reconnect_count(); i++) {
    connection()->reconnect(1, options.delay());
    if (is_success() && connection()->is_connected()) {
      break;
    }
    API_RESET_ERROR();
    printer().update_progress(
      static_cast<int>(i),
      api::ProgressCallback::indeterminate_progress_total());
  }

  printer().update_progress(0, 0);
  printer().set_progress_key("progress");
}

void Installer::save_image_locally(
  const Build &build,
  const fs::FileObject &image,
  const Install &options) {

  API_ASSERT(options.destination().is_empty() == false);

  CLOUD_PRINTER_TRACE("saving image to " + options.destination());
  Link::Path link_path(options.destination(), connection()->driver());
  Link::FileSystem link_filesystem(link_path.driver());

  if (link_path.is_host_path()) {
    CLOUD_PRINTER_TRACE("dest path is on local host");
  } else {
    CLOUD_PRINTER_TRACE("dest path is on target device");
  }
  const auto info = link_filesystem.exists(link_path.path())
                      ? link_filesystem.get_info(link_path.path())
                      : FileInfo();

  CLOUD_PRINTER_TRACE(
    link_path.path() | GeneralString(" is dest a directory ")
    | (info.is_directory() ? "true" : "false"));

  const auto is_directory = link_path.path().is_empty() || info.is_directory();

  const PathString destination
    = is_directory
        ?
        // if directory do <dir>/<project>_<build_name> with .bin for os images
        (link_path.path().is_empty() ? "" : (link_path.path() & "/"))
          & project_name() + "_"
          & Build(Build::Construct())
              .set_type(
                options.is_os() ? Build::os_type() : Build::application_type())
              .set_application_architecture(architecture())
              .normalize_name(options.build_name())
          & (options.is_os() ? ".bin" : "")
        : PathString(link_path.path());

  // parent should be an existing directory
  const auto parent_path = Path::parent_directory(destination);
  if (
    parent_path.is_empty() == false
    && link_filesystem.directory_exists(parent_path) == false) {
    API_RETURN_ASSIGN_ERROR(PathString(parent_path).cstring(), ENOENT);
  }

  const auto append_hash =
    [&](Data &&image_data, const StringView name, bool is_append_hash) -> Data {
    if (is_append_hash == false) {
      return std::move(image_data);
    } else {
      DataFile hashed = DataFile()
                          .reserve(image_data.size())
                          .write(ViewFile(image_data))
                          .move();

      crypto::Sha256::Hash hash = crypto::Sha256::append_aligned_hash(hashed);
      printer().key(name & "Hash", View(hash).to_string());

      return hashed.data();
    }
  };

  Printer::Object sections_object(printer(), "sections");
  printer().key(".text", link_path.prefix() | destination);

  CLOUD_PRINTER_TRACE("save binary file at path " & destination);
  // hash for image was previously added
  Link::File(
    File::IsOverwrite::yes,
    destination,
    OpenMode::read_write(),
    Permissions(0777),
    link_path.driver())
    .write(
      image,
      File::Write().set_progress_callback(printer().progress_callback()));

  JsonKeyValueList<Build::SectionImageInfo> section_image_info
    = build.build_image_info(options.build_name()).get_section_list();

  for (const Build::SectionImageInfo &image_info : section_image_info) {
    if (image_info.key().is_empty() == false) {
      const var::PathString section_destination
        = fs::Path::no_suffix(destination) & image_info.key() & ".bin";

      printer().key(image_info.key(), link_path.prefix() | section_destination);

      Link::File(
        File::IsOverwrite::yes,
        section_destination,
        OpenMode::read_write(),
        Permissions(0777),
        link_path.driver())
        .write(append_hash(
          image_info.get_image_data(),
          image_info.key(),
          options.is_append_hash()));
    }
  }
}

void Installer::kill_application(int app_pid) {
  API_ASSERT(app_pid > 0);

  TaskManager task_manager("", connection()->driver());
  task_manager.kill_pid(app_pid, LINK_SIGINT);

  // give a little time for the program to shut down
  int retries = 0;
  while ((task_manager.get_pid(project_name()) > 0) && (retries < 5)) {
    retries++;
    chrono::wait(100_milliseconds);
  }
}

Installer &Installer::clean_application(const var::StringView name) {
  Link::FileSystem fs(connection()->driver());
  const auto unlink_flash_app = var::PathString("/app/flash") / name;
  const auto unlink_ram_app = var::PathString("/app/ram") / name;

  struct ThreadArgument {
    Printer *printer;
    Mutex mutex;
    bool is_clean_complete;
  };

  ThreadArgument thread_argument;
  thread_argument.printer = &printer();

  Thread progress_thread(
    Thread::Attributes().set_detach_state(Thread::DetachState::joinable),
    Thread::Construct()
      .set_argument(&thread_argument)
      .set_function([](void *args) -> void * {
        ThreadArgument *thread_argument
          = reinterpret_cast<ThreadArgument *>(args);
        Printer *printer = thread_argument->printer;
        printer->set_progress_key("clean");
        bool is_complete = false;
        int count = 0;
        do {
          printer->update_progress(
            count++,
            api::ProgressCallback::indeterminate_progress_total());
          {
            Mutex::Guard mg(thread_argument->mutex);
            is_complete = thread_argument->is_clean_complete;
          }
          wait(250_milliseconds);
        } while (is_complete == false);
        printer->set_progress_key("progress");
        printer->update_progress(0, 0);
        return nullptr;
      }));

  while (fs.exists(unlink_flash_app)) {
    fs.remove(unlink_flash_app);
  }

  while (fs.exists(unlink_ram_app)) {
    fs.remove(unlink_ram_app);
  }

  {
    Mutex::Guard mg(thread_argument.mutex);
    thread_argument.is_clean_complete = true;
  }
  progress_thread.join();
  return *this;
}

void Installer::clean_application() { clean_application(project_name()); }

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
