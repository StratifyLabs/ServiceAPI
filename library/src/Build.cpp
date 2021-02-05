// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include <sdk/types.h>

#include <crypto.hpp>
#include <cxxabi.h>
#include <fs.hpp>
#include <inet.hpp>
#include <json.hpp>
#include <sos.hpp>
#include <swd/Elf.hpp>
#include <sys.hpp>
#include <var.hpp>

#include "service/Build.hpp"
#include "service/Project.hpp"

using namespace service;

Build::Build(const Construct &options)
  : DocumentAccess(
    Path("projects") / options.project_id() / "builds",
    Id(options.build_id())) {

  set_application_architecture(options.architecture());

  if (options.project_path().is_empty() == false) {
    CLOUD_PRINTER_TRACE("import compiled project at " & options.project_path());
    import_compiled(ImportCompiled()
                      .set_path(options.project_path())
                      .set_build(options.build_name()));
    CLOUD_PRINTER_TRACE("done importing " | options.project_path());
    return;
  }

  if (options.binary_path().is_empty() == false) {
    if (fs::Path::suffix(options.binary_path()) == "elf") {
      CLOUD_PRINTER_TRACE("need to imlement importing elf binary");
    }

    return;
  }

  if (options.url().is_empty() == false) {
    import_url(options.url());
    return;
  }

  migrate_build_info_list_20200518();

  crypto::Aes::Key key(
    Aes::Key::Construct().set_key(get_key()).set_initialization_vector(
      get_iv()));

  // download the build images
  if (options.build_name().is_empty() == false) {

    API_ASSERT(options.project_id().is_empty() == false);
    API_ASSERT(options.build_name().is_empty() == false);

    ImageInfo image_info = build_image_info(options.build_name());

    DataFile image;
    cloud().get_storage_object(
      create_storage_path(options.build_name()),
      image);

    if (get_key().is_empty() == false) {

      DataFile decrypted_image
        = DataFile()
            .write(
              image.seek(0),
              AesCbcDecrypter()
                .set_key256(key.key256())
                .set_initialization_vector(key.initialization_vector()))
            .move();

      image.data() = decrypted_image.data().resize(image_info.get_size());
    }

    image_info.set_image_data(image.data());
    return;
  }
}

Build::ImageInfo Build::import_elf_file(const var::StringView path) {
  File elf_file(path);
  swd::Elf elf(elf_file);

  CLOUD_PRINTER_TRACE("importing ELF file " | path);
  DataFile data_image;

  typedef struct MCU_PACK {
    u32 address;
    u32 size;
  } secret_key_t;

  secret_key_t key = {0};

  // using the legacy approach
  mcu_board_config_t mcu_board_config = {0};
  json::JsonKeyValueList<SectionImageInfo> section_list;

  // Data image is the loadable sections of the ELF file
  auto program_header_list
    = elf.get_program_header_list(swd::Elf::ProgramHeaderType::load);

  auto symbol_list = elf.get_symbol_list();

  swd::Elf::Symbol mcu_board_config_symbol
    = symbol_list.find(swd::Elf::Symbol("mcu_board_config"));

  if (mcu_board_config_symbol.size()) {
    CLOUD_PRINTER_TRACE("loading mcu board config (deprecated in v4)");
    elf.load(mcu_board_config_symbol, ViewFile(var::View(mcu_board_config)));
    key.address = mcu_board_config.secret_key_address;
    key.size = mcu_board_config.secret_key_size;
  } else {
    CLOUD_PRINTER_TRACE("no mcu_board_config find sos_config");

    swd::Elf::Symbol sos_config_symbol
      = symbol_list.find(swd::Elf::Symbol("sos_config"));

    if (sos_config_symbol.size()) {
      CLOUD_PRINTER_TRACE("found sos_config " | NumberString(sos_config_symbol.size()) | ", loading key data ");
      elf.load(sos_config_symbol, ViewFile(View(key)));
    }
  }

  CLOUD_PRINTER_TRACE("key size is " | NumberString(key.size));

  u32 text_start_location = 0;
  for (const swd::Elf::ProgramHeader &program_header : program_header_list) {

    printer().object(
      "programHeader",
      program_header,
      printer::Printer::Level::debug);
    const auto name = elf.get_section_name(program_header);

    if (name == ".text" || name == ".data") {
      if( name == ".text" ){
        text_start_location = program_header.physical_address();
      }
      CLOUD_PRINTER_TRACE("adding section text/data to build");
      data_image.write(
        elf.file().seek(program_header.offset()),
        File::Write().set_size(program_header.file_size()));
    } else {
      CLOUD_PRINTER_TRACE("adding section " + name + " to build");
      section_list.push_back(SectionImageInfo(name).set_image_data(
        DataFile()
          .write(
            elf.file().seek(program_header.offset()),
            File::Write().set_size(program_header.file_size()))
          .data()));
    }
  }

  CLOUD_PRINTER_TRACE(
    "loaded " | NumberString(section_list.count()) | " additional sections");
  return Build::ImageInfo()
    .set_image_data(data_image.data())
    .set_size(data_image.size())
    .set_secret_key_position((key.address - text_start_location) & ~0x01)
    .set_secret_key_size(key.size)
    .set_section_list(section_list);
}

Build &Build::import_compiled(const ImportCompiled &options) {
  API_RETURN_VALUE_IF_ERROR(*this);
  const auto project_settings_path = options.path() / Project::file_name();
  CLOUD_PRINTER_TRACE("import " | project_settings_path.string_view());
  Project project_settings = Project().import_file(File(project_settings_path));

  API_RETURN_VALUE_IF_ERROR(*this);

  CLOUD_PRINTER_TRACE("checking for path and project name match");
  if (fs::Path::name(options.path()) != project_settings.get_name()) {
    API_RETURN_VALUE_ASSIGN_ERROR(
      *this,
      "project folder name does not match project name in `settings.json` for `"
        & options.path() & "`",
      EINVAL);
  }

  CLOUD_PRINTER_TRACE("importing build entries from the project");
  set_name(project_settings.get_name())
    .set_project_id(project_settings.get_document_id())
    .set_version(project_settings.get_version())
    .set_type(project_settings.get_type())
    .set_permissions(project_settings.get_permissions());

  CLOUD_PRINTER_TRACE("build type is " | get_type());

  if (get_permissions().is_empty()) {
    CLOUD_PRINTER_TRACE("Setting default permissions to public");
    set_permissions("public");
  }

  set_ram_size(project_settings.get_ram_size());
  set_image_included(true);

  // check for a valid build name if provided
  if (options.build().is_empty() == false) {
    const PathString build_path
      = options.path() / normalize_name(options.build()).string_view();
    if (FileSystem().exists(build_path) == false) {
      CLOUD_PRINTER_TRACE("build name provided but doesn't exist");
      API_RETURN_VALUE_ASSIGN_ERROR(*this, build_path.cstring(), ENOENT);
    }
  }

  const auto build_directory_list = FileSystem().read_directory(options.path());

  Vector<ImageInfo> local_build_image_list;
  for (const auto &build_directory_entry : build_directory_list) {

    if (
      application_architecture().is_empty()
      && (get_arch(build_directory_entry).is_empty() == false)) {
      // pulls arch from the build directory if not provided
      m_application_architecture = get_arch(build_directory_entry);
    }

    bool is_included
      = (build_directory_entry.string_view().find("build_") == 0)
        && (build_directory_entry.string_view().find("_link") == StringView::npos);

    if (is_included) {
      CLOUD_PRINTER_TRACE("checking build directory " & build_directory_entry);
    }

    const var::NameString build_name = normalize_name(build_directory_entry);
    if (is_included) {
      const var::NameString option_name = normalize_name(options.build());

      is_included
        = (options.build().is_empty() || (build_name.string_view() == option_name.string_view()));

      if (is_included == false) {
        CLOUD_PRINTER_TRACE("excluding " & options.build());
      }
    }

    if (is_included) {

      const PathString elf_path = options.path() / build_directory_entry
                                  / normalize_elf_name(
                                      project_settings.get_name(),
                                      build_directory_entry)
                                      .string_view();

      CLOUD_PRINTER_TRACE("elf path " & elf_path);

      if (FileSystem().exists(elf_path) == false) {
        CLOUD_PRINTER_TRACE("elf file " & elf_path & " is missing");
        API_RETURN_VALUE_ASSIGN_ERROR(*this, elf_path.cstring(), EINVAL);
      }

      ImageInfo image_info = import_elf_file(elf_path);

      DataFile data_image;
      data_image.data() = image_info.get_image_data();

      if (is_application()) {
        // make sure settings are populated in the binary

        const sys::Version version(project_settings.get_version());
        Appfs::FileAttributes(
          data_image.seek(0).set_flags(OpenMode::read_write()))
          .set_name(String(project_settings.get_name()))
          .set_id(String(project_settings.get_document_id()))
          .set_startup(false)
          .set_flash(false)
          .set_ram_size(0)
          .set_version(version.to_bcd16())
          .apply(data_image);
      }

      local_build_image_list.push_back(
        image_info.set_name(build_directory_entry)
          .set_image_data(data_image.data()));

      ImageInfo printable_info
        = json::JsonObject().copy(local_build_image_list.back()).to_object();
      printable_info.set_image("<image data>");
      for (auto &section : printable_info.section_list()) {
        section.set_image("<image data>");
      }

      printer().object(
        build_directory_entry,
        printable_info,
        printer::Printer::Level::debug);
    }
  }

  CLOUD_PRINTER_TRACE(
    "Update build image list with "
    | NumberString(local_build_image_list.count()) | " items");
  set_build_image_list(local_build_image_list);

  return *this;
}

Build &Build::import_url(const var::StringView url) {

  DataFile response;
  printer().set_progress_key("downloading");
  HttpSecureClient().connect(url).get(
    url,
    Http::Get().set_response(&response).set_progress_callback(
      printer().progress_callback()));
  printer().set_progress_key("progress");
  to_object() = JsonDocument().load(response.seek(0)).to_object();
  return *this;
}

var::Data Build::get_image(const var::StringView name) const {
  return build_image_info(name).get_image_data();
}

Build &Build::set_image(const var::StringView name, const var::Data &image) {
  build_image_info(name).set_image_data(image);
  return *this;
}

Build &Build::insert_secret_key(
  const var::StringView build_name,
  const var::View secret_key) {
  API_RETURN_VALUE_IF_ERROR(*this);

  ImageInfo image_info = build_image_info(normalize_name(build_name));

  const u32 location = image_info.get_secret_key_position();
  const u32 size = image_info.get_secret_key_size();
  CLOUD_PRINTER_TRACE("secret key location is " | NumberString(location, "0x%08x"));
  CLOUD_PRINTER_TRACE("secret key size is " | NumberString(size));
  if (size == 0) {
    return *this;
  }

  Aes::Key new_key;

  View secret_key_view
    = secret_key.size() ? secret_key : View(new_key.key256());

  CLOUD_PRINTER_TRACE("provided key size is " | NumberString(secret_key.size()));
  CLOUD_PRINTER_TRACE("generated key size is " | NumberString(new_key.key256().count()));

  Data image_data = image_info.get_image_data();
  ViewFile(image_data).seek(location).write(secret_key_view);

  const auto key_string = secret_key_view.to_string();
  CLOUD_PRINTER_TRACE("final key is " | key_string);

  image_info
    .set_secret_key(key_string)
    .set_image_data(image_data);

  return *this;
}

void Build::interface_remove() {
  // delete the storage objects associated with the build

#if CAN_DELETE_OBJECTS
  // right now storage objects do not have permissions for deletion

  // is this an old-style build commit
  json::JsonArray array = at("buildList");
  if (array.count() && array.at(0).is_string()) {
    CLOUD_PRINTER_TRACE("using legacy build list");
    for (u32 i = 0; i < array.count(); i++) {
      api::ErrorGuard error_guard;
      const auto path = create_storage_path(array.at(i).to_string_view());
      CLOUD_PRINTER_TRACE(
        "Removing legacy build storage " | path.string_view());
      cloud().remove_storage_object(path.string_view());
    }
  } else {
    const auto list = get_build_image_list();
    for (const ImageInfo &build_image_info : list) {
      api::ErrorGuard error_guard;
      const auto path = create_storage_path(build_image_info.get_name());
      CLOUD_PRINTER_TRACE("Removing build storage " | path.string_view());
      cloud().remove_storage_object(path.string_view());
    }
  }
  API_RETURN_IF_ERROR();
#endif

  CLOUD_PRINTER_TRACE("Removing build document");
  Document::interface_remove();
}

void Build::interface_save() {

  // upload the build images to storage /builds/project_id/build_id/arch/name
  int count = 1;

  Aes::Key key(
    Aes::Key::Construct().set_key(get_key()).set_initialization_vector(
      get_iv()));

  const auto list = get_build_image_list();

  remove_build_image_data();
  DocumentAccess<Build>::interface_save();
  API_RETURN_IF_ERROR();

  for (const ImageInfo &build_image_info : list) {

    Data data = build_image_info.get_image_data();

    Array<u8, 16> padding;
    View padding_view(padding);

    data.append(padding_view.fill(0).truncate(Aes::get_padding(data.size())));

    DataFile encrypted_file
      = DataFile()
          .reserve(data.size() + 16)
          .write(
            ViewFile(data),
            AesCbcEncrypter()
              .set_key256(key.key256())
              .set_initialization_vector(key.initialization_vector()))
          .move();

    KeyString message;
    message.format("%d of %d", count, list.count());
    cloud().create_storage_object(
      create_storage_path(build_image_info.get_name()),
      encrypted_file.seek(0),
      message.string_view());

    count++;
  }
}

var::PathString Build::get_build_file_path(
  const var::StringView path,
  const var::StringView build) {

  return (var::PathString(path) / build / get_name())
    .append(decode_build_type() == Type::os ? ".bin" : "");
}

bool Build::is_application() const {
  return decode_build_type(get_type()) == Type::application;
}

bool Build::is_os() const { return decode_build_type(get_type()) == Type::os; }

bool Build::is_data() const {
  return decode_build_type(get_type()) == Type::data;
}

Build::Type Build::decode_build_type(const var::StringView type) {
  if ((type == "app") || (type == "StratifyApp")) {
    return Type::application;
  }

  if (
    (type == "bsp") || (type == "os") || (type == "kernel")
    || (type == "StratifyKernel")) {
    return Type::os;
  }

  if (type == "data") {
    return Type::data;
  }

  return Type::unknown;
}

Build::Type Build::decode_build_type() const {
  return decode_build_type(get_type());
}

var::StringView Build::encode_build_type(Type type) {
  String result;
  switch (type) {
  case Type::application:
    result = "app";
    break;
  case Type::os:
    result = "os";
    break;
  case Type::data:
    result = "data";
    break;
  default:
    result = "unknown";
    break;
  }
  return result;
}

var::NameString Build::normalize_name(const var::StringView build_name) const {
  var::NameString result;
  if (build_name.find("build_") == 0) {
    result.append(build_name);
  } else {
    result.append("build_").append(build_name);
  }

  if (
    (application_architecture().is_empty() == false)
    && get_arch(result).is_empty()) {
    result.append("_").append(application_architecture());
  }

  return result;
}

var::NameString Build::normalize_elf_name(
  const var::StringView project_name,
  const var::StringView build_directory) const {
  var::NameString result(project_name);

  StringView build_name
    = build_directory.get_substring_at_position(StringView("build_").length());
  result.append("_").append(build_name);

  return result.append(".elf");
}

var::StringView Build::get_arch(const var::StringView name) {
  if (name.find("_v7em_f5dh") != String::npos) {
    return "v7em_f5dh";
  }
  if (name.find("_v7em_f5sh") != String::npos) {
    return "v7em_f5sh";
  }
  if (name.find("_v7em_f4sh") != String::npos) {
    return "v7em_f4sh";
  }
  if (name.find("_v7em") != String::npos) {
    return "v7em";
  }
  if (name.find("_v7m") != String::npos) {
    return "v7m";
  }
  return var::StringView();
}

void Build::migrate_build_info_list_20200518() {
  JsonArray build_list_array = to_object().at("buildList");

  Vector<ImageInfo> migrated_list;

  for (u32 i = 0; i < build_list_array.count(); i++) {
    if (build_list_array.at(i).is_string() == false) {
      return;
    }

    migrated_list.push_back(ImageInfo()
                              .set_name(build_list_array.at(i).to_string())
                              .set_image("")
                              .set_hash("")
                              .set_secret_key_position(0)
                              .set_secret_key_size(0)
                              .set_secret_key(""));
  }

  set_build_image_list(migrated_list);
}
