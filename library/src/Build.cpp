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
    Path("projects/").append(options.project_id()).append("/builds"),
    Id(options.build_id())) {

  if (options.project_path().is_empty() == false) {

    API_ASSERT(options.build_name().is_empty() == false);

    import_compiled(ImportCompiled()
                      .set_path(options.project_path())
                      .set_build(options.build_name())
                      .set_application_architecture(options.architecture()));
    return;
  }

  if (options.binary_path().is_empty() == false) {
    if (fs::Path::suffix(options.binary_path()) == "elf") {
    }

    return;
  }

  if (options.url().is_empty() == false) {
    import_url(options.url());
    return;
  }

  migrate_build_info_list_20200518();

  // download the build images
  if (options.build_name().is_empty() == false) {
    ImageInfo image_info = build_image_info(options.build_name());

    DataFile image;
    cloud().get_storage_object(
      create_storage_path(options.build_name()),
      image);

    image_info.set_image_data(image.data());
  }
}

Build::ImageInfo Build::import_elf_file(const var::StringView path) {
  File elf_file(path);
  swd::Elf elf(&elf_file);

  DataFile data_image;
  mcu_board_config_t mcu_board_config = {0};
  json::JsonKeyValueList<SectionImageInfo> section_list;

  // Data image is the loadable sections of the ELF file
  auto program_header_list
    = elf.get_program_header_list(swd::Elf::ProgramHeaderType::load);

  auto symbol_list = elf.get_symbol_list();

  swd::Elf::Symbol mcu_board_config_symbol
    = symbol_list.find(swd::Elf::Symbol("mcu_board_config"));

  if (mcu_board_config_symbol.size()) {
    elf.load(mcu_board_config_symbol, ViewFile(var::View(mcu_board_config)));
  }

  for (const swd::Elf::ProgramHeader &program_header : program_header_list) {

    const var::StringView name = elf.get_section_name(program_header);

    if (name == ".text" || name == ".data") {
      data_image.write(
        elf.file().seek(program_header.offset()),
        File::Write().set_size(program_header.memory_size()));
    } else {
      section_list.push_back(SectionImageInfo(name).set_image_data(
        DataFile()
          .write(
            elf.file().seek(program_header.offset()),
            File::Write().set_size(program_header.memory_size()))
          .data()));
    }
  }

  return Build::ImageInfo()
    .set_image_data(data_image.data())
    .set_secret_key_position(mcu_board_config.secret_key_address)
    .set_secret_key_size(mcu_board_config.secret_key_size)
    .set_section_list(section_list)
    .calculate_hash();
}

Build &Build::import_compiled(const ImportCompiled &options) {

  Project project_settings = Project().import_file(
    File(var::PathString(options.path()) / Project::file_name()));

  API_RETURN_VALUE_IF_ERROR(*this);

  set_name(project_settings.get_name())
    .set_project_id(project_settings.get_document_id())
    .set_version(project_settings.get_version())
    .set_type(project_settings.get_type())
    .set_permissions(project_settings.get_permissions())
    .set_application_architecture(options.application_architecture());

  if (get_permissions().is_empty()) {
    set_permissions("public");
  }

  set_ram_size(project_settings.get_ram_size_cstring());
  set_image_included(true);

  fs::PathList build_directory_list
    = FileSystem().read_directory(options.path());

  Vector<ImageInfo> local_build_image_list;
  for (const auto &build_directory_entry : build_directory_list) {

    if (options.application_architecture().is_empty()) {
      if (get_arch(build_directory_entry).is_empty() == false) {
        m_application_architecture = get_arch(build_directory_entry);
      }
    }

    bool is_included
      = (build_directory_entry.string_view().find("build_") == 0)
        && (build_directory_entry.string_view().find("_link") == StringView::npos);

    const var::NameString build_name = normalize_name(build_directory_entry);
    if (is_included) {
      const var::NameString option_name = normalize_name(options.build());

      is_included
        = (options.build().is_empty() || build_name.string_view() == option_name.string_view());
    }

    if (is_included) {

      const PathString elf_path = PathString(options.path())
                                  / build_directory_entry
                                  / normalize_elf_name(
                                    project_settings.get_name(),
                                    build_directory_entry);

      if (FileSystem().exists(elf_path) == false) {
        API_RETURN_VALUE_ASSIGN_ERROR(*this, elf_path.cstring(), EINVAL);
      }

      ImageInfo image_info = import_elf_file(elf_path);

      DataFile data_image;
      data_image.data() = image_info.get_image_data();

      if (is_application()) {
        // make sure settings are populated in the binary

        const sys::Version version(project_settings.get_version());

        Appfs::FileAttributes(data_image.seek(0))
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
          .set_image_data(data_image.data())
          .calculate_hash());
    }
  }

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
  return std::move(build_image_info(name).get_image_data());
}

Build &Build::set_image(const var::StringView name, const var::Data &image) {
  build_image_info(name).set_image_data(image);
  return *this;
}

Build &Build::insert_secret_key(
  const var::StringView build_name,
  const var::View secret_key) {

  ImageInfo image_info = build_image_info(normalize_name(build_name));

  const u32 location = image_info.get_secret_key_position();
  const u32 size = image_info.get_secret_key_size();

  Data generate_secret_key(size);
  Random().seed().randomize(View(generate_secret_key));

  View secret_key_view
    = secret_key.size() ? secret_key : View(generate_secret_key);

  Data image_data = image_info.get_image_data();
  ViewFile(image_data).seek(location).write(secret_key_view);

  image_info
    .set_secret_key(
      StringView(secret_key_view.to_const_char(), secret_key_view.size()))
    .set_image_data(image_data);

  return *this;
}

Build &Build::append_hash(const var::StringView build_name) {
  ImageInfo image_info = build_image_info(build_name);
  image_info.calculate_hash();

  Vector<SectionImageInfo> section_info_list = image_info.section_list();
  for (SectionImageInfo &section : section_info_list) {
    section.calculate_hash();
  }
  return *this;
}

Build &Build::save(const Save &options) {

  const var::Vector<ImageInfo> list = get_build_image_list();

  var::String build_id;
  save();

  API_RETURN_VALUE_IF_ERROR(*this);

  StringView name = get_name();

  // upload the build images to storage /builds/project_id/build_id/arch/name
  int count = 1;

  for (const ImageInfo &build_image_info : list) {

    cloud().create_storage_object(
      create_storage_path(build_image_info.get_name()),
      ViewFile(build_image_info.get_image_data()),
      StackString64().format("%d of %d", count, list.count()));

    count++;
  }

  return *this;
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

  if (!application_architecture().is_empty() && get_arch(result).is_empty()) {
    result.append("_").append(application_architecture().string_view());
  }

  return result;
}

var::NameString Build::normalize_elf_name(
  const var::StringView project_name,
  const var::StringView build_directory) const {
  var::NameString result(project_name);

  if (!application_architecture().is_empty() && get_arch(result).is_empty()) {
    result &StringView("_") & application_architecture().string_view();
  }

  if (build_directory.find("release") == StringView::npos) {
    StringView build_name = build_directory.get_substring_at_position(
      StringView("build_").length());
    result.append("_").append(build_name);
  }

  return result &= ".elf";
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
