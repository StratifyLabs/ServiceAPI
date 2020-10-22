#include <crypto.hpp>
#include <cxxabi.h>
#include <fs.hpp>
#include <inet.hpp>
#include <json.hpp>
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
    import_compiled(ImportCompiled()
                      .set_build(options.build_name())
                      .set_application_architecture(options.architecture()));
    return;
  }

  if (options.url().is_empty()) {
    import_url(options.url());
    return;
  }

  migrate_build_info_list_20200518();

  // download the build images
  if (!options.build_name().is_empty()) {
    ImageInfo image_info = build_image_info(options.build_name());

    DataFile image;
    cloud().get_storage_object(
      create_storage_path(options.build_name()),
      image);

    image_info.set_image_data(image.data());
  }
}

Build &Build::import_compiled(const ImportCompiled &options) {

  Project project_settings = Project().import_file(
    File(var::PathString(options.path()) / Project::file_name()));

  API_RETURN_VALUE_IF_ERROR(*this);

  set_name(project_settings.get_name_cstring())
    .set_project_id(project_settings.get_document_id_cstring())
    .set_version(project_settings.get_version_cstring())
    .set_type(project_settings.get_type_cstring())
    // set_document_id(project_settings.get_document_id_cstring());
    .set_publisher(project_settings.get_publisher_cstring())
    .set_permissions(project_settings.get_permissions_cstring())
    .set_application_architecture(options.application_architecture());

  if (get_permissions().is_empty()) {
    set_permissions("public");
  }

  set_ram_size(project_settings.get_ram_size_cstring());
  set_image_included(true);

  fs::PathList build_directory_list
    = FileSystem().read_directory(Dir(options.path()));

  Vector<ImageInfo> local_build_image_list;
  for (const auto &build_directory_entry : build_directory_list) {
    if (
      (StringView(build_directory_entry.cstring()).find("build_") == 0)
      && (StringView(build_directory_entry.cstring()).find("_link") == String::npos)) {

      if (
        options.build().is_empty()
        || Build::normalize_name(build_directory_entry.string_view())
               .string_view()
             == Build::normalize_name(options.build()).string_view()) {

        var::PathString file_path = get_build_file_path(
          options.path(),
          build_directory_entry.string_view());

        // import the binary data
        File file_image(file_path, fs::OpenMode::read_only());
        DataFile data_image
          = std::move(DataFile(fs::OpenMode::append_write_only())
                        .reserve(file_image.size() + 512));

        mcu_board_config_t mcu_board_config = {0};
        json::JsonKeyValueList<SectionImageInfo> section_list;

        if (is_application()) {
          // make sure settings are populated in the binary
          CLOUD_PRINTER_TRACE("set application binary properties");

          const sys::Version version(project_settings.get_version());

          sos::Appfs::FileAttributes()
            .set_name(String(project_settings.get_name()))
            .set_id(String(project_settings.get_document_id()))
            .set_startup(false)
            .set_flash(false)
            .set_ram_size(0)
            .set_version(version.to_bcd16())
            .apply(file_image);

          data_image.write(
            file_image.seek(0),
            File::Write().set_size(sizeof(appfs_header_t)));

        } else if (is_os()) {

#if 0
          mcu_board_config = load_mcu_board_config(
            fs::File::Path(options.path()),
            project_settings.get_name(),
            Build::Name(build_directory_entry),
            printer());
#endif

          Vector<SectionPathInfo> section_path_list
            = get_section_image_path_list(
              options.path(),
              build_directory_entry.cstring());

          for (const SectionPathInfo &section : section_path_list) {
            section_list.push_back(
              SectionImageInfo(section.name())
                .set_image_data(
                  DataFile().write(File(section.path().string_view())).data()));
          }
        }

        // write file_image to data_image
        data_image.write(file_image);

        local_build_image_list.push_back(
          Build::ImageInfo()
            .set_name(build_directory_entry.cstring())
            .set_image_data(data_image.data())
            .set_secret_key_position(mcu_board_config.secret_key_address)
            .set_secret_key_size(mcu_board_config.secret_key_size)
            .set_section_list(section_list));
      }
    }
  }

  set_build_image_list(local_build_image_list);

  return *this;
}

Build &Build::import_url(const var::StringView url) {

  DataFile response;
  printer().progress_key() = "downloading";
  HttpSecureClient().connect(url).get(
    url,
    Http::Get().set_response(&response).set_progress_callback(
      printer().progress_callback()));
  printer().progress_key() = "progress";
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
  ViewFile(View(image_data)).seek(location).write(secret_key_view);

  image_info.set_secret_key(secret_key_view.to_string().cstring())
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
      ViewFile(build_image_info.get_image_data()),
      create_storage_path(build_image_info.get_name()),
      StackString64().format("%d of %d", count, list.count()));

    count++;
  }

  return *this;
}

var::PathString Build::get_build_file_path(
  const var::StringView path,
  const var::StringView build) {

  return var::PathString(path) / build / get_name()
         += (decode_build_type() == Type::os ? ".bin" : "");
}

var::Vector<Build::SectionPathInfo> Build::get_section_image_path_list(
  const var::StringView path,
  const var::StringView build) {
  var::Vector<SectionPathInfo> result;
  if (is_os() == false) {
    return result;
  }

  const var::PathString binary_path = get_build_file_path(path, build);
  const var::StringView directory_path
    = fs::Path::parent_directory(binary_path.string_view());
  const StringView base_name = fs::Path::base_name(binary_path.string_view());

  // binary is of form <name>.bin
  // are there any files in the output directory with <name>.<section>.bin ?
  fs::PathList file_list = FileSystem().read_directory(Dir(directory_path));

  for (const auto &file : file_list) {
    StringViewList file_name_part_list = file.string_view().split(".");
    if (
      (file_name_part_list.count() == 3)
      && (file_name_part_list.at(0) == base_name)
      && (file_name_part_list.at(2) == "bin")) {
      result.push_back(SectionPathInfo()
                         .set_name(String(file_name_part_list.at(1)))
                         .set_path(directory_path + "/" + file.string_view()));
    }
  }
  return result;
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

  if (!application_architecture().is_empty()) {
    bool is_arch_present = false;
    if (result.string_view().find("_v7em_f5dh") != String::npos) {
      is_arch_present = true;
    }
    if (
      !is_arch_present
      && result.string_view().find("_v7em_f5sh") != String::npos) {
      is_arch_present = true;
    }
    if (
      !is_arch_present
      && result.string_view().find("_v7em_f4sh") != String::npos) {
      is_arch_present = true;
    }
    if (
      !is_arch_present && result.string_view().find("_v7em") != String::npos) {
      is_arch_present = true;
    }
    if (!is_arch_present && result.string_view().find("_v7m") != String::npos) {
      is_arch_present = true;
    }
    if (!is_arch_present) {
      result.append("_").append(application_architecture().string_view());
    }
  }

  return result;
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
