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
    Id(options.build_id())) {}

Build &Build::import_compiled(const ImportCompiled &options) {

  Project project_settings
    = Project().load(options.path() + "/" + Project::file_name());

  if (project_settings.is_valid() == false) {
    set_error_message(
      "Failed to open project settings at `" + options.path() + "`");
    return -1;
  }

  set_name(project_settings.get_name_cstring())
    .set_project_id(project_settings.get_document_id_cstring())
    .set_version(project_settings.get_version_cstring())
    .set_type(project_settings.get_type_cstring())
    // set_document_id(project_settings.get_document_id_cstring());
    .set_publisher(project_settings.get_publisher_cstring())
    .set_permissions(project_settings.get_permissions_cstring());
  if (get_permissions().is_empty()) {
    set_permissions("public");
  }

  set_application_architecture(options.application_architecture());

  set_ram_size(project_settings.get_ram_size_cstring());
  set_image_included(true);

  FileSystem::PathList build_directory_list
    = FileSystem().read_directory(Dir(options.path()));

  Vector<BuildImageInfo> local_build_image_list;
  for (const auto &build_directory_entry : build_directory_list) {
    if (
      (StringView(build_directory_entry.cstring()).find("build_") == 0)
      && (StringView(build_directory_entry.cstring()).find("_link") == String::npos)) {

      if (
        options.build().is_empty()
        || Build::normalize_name(build_directory_entry.cstring())
             == Build::normalize_name(options.build().cstring())) {

        String file_path = get_build_file_path(
          options.path().cstring(),
          build_directory_entry.cstring());

        // import the binary data
        File file_image(file_path, fs::OpenMode::read_only());
        DataFile data_image
          = std::move(DataFile(fs::OpenMode::append_write_only())
                        .reserve(file_image.size() + 512));

        mcu_board_config_t mcu_board_config = {0};
        json::JsonKeyValueList<BuildSectionImageInfo> section_list;

        if (is_application()) {
          // make sure settings are populated in the binary
          CLOUD_PRINTER_TRACE("set application binary properties");

          const VersionString version
            = VersionString().set_string(project_settings.get_version());
          appfs_file_t header;
          file_image.read(View(header));

          sos::Appfs::FileAttributes()
            .set_name(String(project_settings.get_name()))
            .set_id(String(project_settings.get_document_id()))
            .set_startup(false)
            .set_flash(false)
            .set_ram_size(0)
            .set_version(version.to_bcd16())
            .apply(&header);

          data_image.write(View(header));
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
            DataFile section_image = DataFile(File::Path(section.path()));

            section_list.push_back(BuildSectionImageInfo(section.name())
                                     .set_image_data(section_image.data()));
          }
        }

        // write file_image to data_image
        data_image.write(file_image);
        file_image.close();

        local_build_image_list.push_back(
          BuildImageInfo()
            .set_name(build_directory_entry.cstring())
            .set_image_data(data_image.data())
            .set_secret_key_position(mcu_board_config.secret_key_address)
            .set_secret_key_size(mcu_board_config.secret_key_size)
            .set_section_list(section_list));
      }
    }
  }

  set_build_image_list(local_build_image_list);

  return 0;
}

int Build::download(const BuildOptions &options) {

  if (Document::download(options) < 0) {
    set_error_message(
      "failed to download the document " + options.document_id());
    return -1;
  }

  migrate_build_info_list_20200518();

  // download the build images
  if (!options.build_name().is_empty()) {
    BuildImageInfo image_info = build_image_info(options.build_name());

    DataFile image(OpenFlags::append_write_only());

    CLOUD_PRINTER_TRACE(
      "downloading build from storage path " + options.storage_path());

    if (cloud().get_storage_object(options.storage_path(), image) < 0) {
      set_error_message(
        "Failed to download binary from " + options.storage_path());
      return -1;
    }

    image_info.set_image_data(image.data());
  }
  return 0;
}

int Build::download(const var::String &url) {

  // download a build from a URL

  DataFile response(fs::OpenMode::append().set_read_write());

  printer().progress_key() = "downloading";
  HttpSecureClient().connect(url).get(
    url,
    Http::Get().set_response(&response).set_progress_callback(
      printer().progress_callback()));

  printer().progress_key() = "progress";

  String response_string(response.data());

  {
    Build downloaded_build = JsonDocument().load(response_string).to_object();

    if (downloaded_build.is_valid() == false) {
      CLOUD_PRINTER_TRACE("response is " + response_string);
      error_message() = "failed to parse downloaded project";
      return -1;
    }

    to_object() = downloaded_build;
  }

  CLOUD_PRINTER_TRACE(
    String().format("build inclues image? %d", is_build_image_included()));

  if (is_build_image_included() == false) {
    CLOUD_PRINTER_TRACE("build does not include image data");
    error_message() = "build document does not include image data";
    return -1;
  }

  CLOUD_PRINTER_TRACE("build downloaded successfully");
  return 0;
}

var::Data Build::get_image(const var::String &name) const {
  BuildImageInfo info = build_image_info(name);
  return info.get_image_data();
}

Build &Build::set_image(const var::String &name, const var::Data &image) {
  BuildImageInfo info = build_image_info(name);
  info.set_image_data(image);
  return *this;
}

Build &Build::insert_secret_key(
  const var::StringView build_name,
  const var::View secret_key) {

  BuildImageInfo image_info = build_image_info(normalize_name(build_name));

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
  BuildImageInfo image_info = build_image_info(build_name);
  image_info.calculate_hash();

  Vector<BuildSectionImageInfo> section_info_list = image_info.section_list();
  for (BuildSectionImageInfo &section : section_info_list) {
    section.calculate_hash();
  }
  return *this;
}

var::String Build::upload(const BuildOptions &options) {

  var::Vector<BuildImageInfo> list = get_build_image_list();

  var::String build_id;
  if (!options.is_dry_run()) {
    build_id = Build(JsonObject().copy(to_object()))
                 .remove_build_image_data()
                 .Document::upload(options);
  } else {
    build_id = "<buildId>";
  }

  String name = String(get_name());

  // upload the build images to storage /builds/project_id/build_id/arch/name
  int count = 1;

  for (const BuildImageInfo &build_image_info : list) {
    const String storage_path = BuildOptions(options.project_id())
                                  .set_project_name(name)
                                  .set_build_name(build_image_info.get_name())
                                  .set_document_id(build_id)
                                  .create_storage_path();

    DataFile input_file;
    input_file.data() = build_image_info.get_image_data();
    if (!options.is_dry_run()) {
      cloud().create_storage_object(
        input_file,
        storage_path,
        StackString64().format("%d of %d", count, list.count()));
    } else {
      printer().key(
        StackString64().format("uploading | %d of %d", count, list.count()),
        storage_path);
    }

    count++;
  }

  return build_id;
}

var::String Build::get_build_file_path(
  const var::StringView path,
  const var::StringView build) {
  int type = decode_build_type();

  return String(
    path + "/" + build + "/" + get_name() + (type == Type::os ? ".bin" : ""));
}

var::Vector<Build::SectionPathInfo> Build::get_section_image_path_list(
  const var::String &path,
  const var::String &build) {
  var::Vector<SectionPathInfo> result;
  if (is_os() == false) {
    return result;
  }

  String binary_path = get_build_file_path(path, build);
  String directory_path = FileInfo::parent_directory(binary_path);
  String base_name = FileInfo::base_name(binary_path);

  // binary is of form <name>.bin
  // are there any files in the output directory with <name>.<section>.bin ?
  StringList file_list = Dir::read_list(directory_path);
  for (const String &file : file_list) {
    StringList file_name_part_list = file.split(".");
    if (
      (file_name_part_list.count() == 3)
      && (file_name_part_list.at(0) == base_name)
      && (file_name_part_list.at(2) == "bin")) {
      result.push_back(SectionPathInfo()
                         .set_name(file_name_part_list.at(1))
                         .set_path(directory_path + "/" + file));
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

JsonObject Build::import_disassembly(
  fs::File::Path path,
  Build::Name build,
  sys::Printer &printer) {
  /*
   * Create a disassembly JSON object
   *
   * info: {
   *   name: buildName,
   *   size: buildSize,
   *   type: application or kernel
   * },
   * functions: [
   *   { name: functionName,
   *     address: functionAddress,
   *     size: functionSize,
   *     line: disassemblyLineNumber,
   *     section: currentSection
   *   }
   *
   * ]
   *
   */

  String lst_path = String() << path.argument() << ".lst";
  String json_path = String() << path.argument() << ".json";
  String binary_path = String() << path.argument();
  String binary_hash;

  PRINTER_TRACE(printer, "check if binary file exists " + binary_path);

  if (File::exists(binary_path) == false) {
    binary_path << ".bin";

    PRINTER_TRACE(printer, "check if other binary file exists " + binary_path);

    if (File::exists(binary_path) == false) {
      return JsonObject();
    }
  }

  PRINTER_TRACE(printer, "check if lst file exists " + lst_path);

  if (File::exists(lst_path) == false) {

    lst_path = String() << path.argument() << "_" << build.argument() << ".lst";

    PRINTER_TRACE(printer, "check if other lst file exists " + lst_path);

    if (File::exists(lst_path) == false) {

      return JsonObject();
    }
  }

  PRINTER_TRACE(printer, "calculate hash for file " + binary_path);

  binary_hash = crypto::Sha256::calculate(binary_path);

  PRINTER_TRACE(printer, "check for json file " + json_path);

  if (File::exists(json_path) == true) {

    JsonObject object = JsonDocument().load(File::Path(json_path)).to_object();

    PRINTER_TRACE(printer, "return existing disassembly info " + json_path);

    if (
      object.at(("info")).to_object().at(("binaryHash")).to_string()
      == binary_hash) {
      // hashes match -- just return the current file
      return object;
    }
  }

  PRINTER_TRACE(printer, "open lst file " + lst_path);

  DataFile f = DataFile(DataFile::Path(lst_path));

  if (f.size() == 0) {
    PRINTER_TRACE(printer, "failed to load lst file " + lst_path);
    return JsonObject();
  }

  String line;
  String current_label;
  String current_section;
  int line_number = 1;

  JsonObject result;
  JsonObject label;
  JsonArray labels;
  String last_address;
  String first_address;
  String last_entry;

  const String section_start = "Disassembly of section ";

  current_section = "";

  const Data &contents = f.data();

  ReferenceFile data_file(fs::OpenMode::read_only());
  data_file.reference() = contents;

  PRINTER_TRACE(printer, "parsing disassembly file" + lst_path);

  std::function<String(const Tokenizer &tokenizer)> find_address
    = [](const Tokenizer &tokenizer) -> String {
    // return first non empy token
    for (const auto &token : tokenizer.list()) {
      if (token.is_empty() == false) {
        return token;
      }
    }
    return String();
  };

  while ((line = data_file.gets()).is_empty() == false) {

    if (line.find(section_start) == 0) {
      size_t col_pos = line.find(":");
      current_section = line.create_sub_string(
        String::Position(section_start.length()),
        String::Length(col_pos - section_start.length()));
    } else if (current_section.is_empty() == false) {

      Tokenizer label_tokens(line, Tokenizer::Delimeters(" <>:\t\n"));

      if (label_tokens.count() == 6) {
        // this is a new label
        if (current_label.is_empty() == false) {
          PRINTER_TRACE(printer, "add label " + current_label);

          // update the size value
          // append the line count value

          labels.append(label);
          current_label.clear();
        }

        label = JsonObject();
        current_label = label_tokens.at(2);
        label.insert("name", JsonString(current_label));
        int demangle_status;

        char *pretty_name = abi::__cxa_demangle(
          current_label.cstring(),
          0,
          0,
          &demangle_status);

        label.insert("prettyName", JsonString(pretty_name));
        free(pretty_name);
        label.insert("address", JsonString(label_tokens.at(0)));
        label.insert("line", JsonString(String().format("%d", line_number)));
        label.insert("section", JsonString(current_section));
        if (first_address.is_empty()) {
          first_address = label_tokens.at(0);
        }
      } else if (line == "\n") {
        int line_count = line_number - label.at(("line")).to_integer();
        int size
          = last_address.to_unsigned_long(String::base_16)
            - label.at("address").to_string().to_unsigned_long(String::base_16);

        label.insert("addressEnd", JsonString(last_address));
        label.insert("lineEnd", JsonString(String().format("%d", line_number)));

        label.insert(("size"), JsonInteger(size));
        label.insert(("lineCount"), JsonInteger(line_count));
      } else if (label_tokens.list().find("...") == label_tokens.count()) {
        last_address = find_address(label_tokens);
      }
    }

    // a new label looks like '08040000 <mcu_core_vector_table>:'
    line_number++;
  }

  JsonObject info;

  String name = File::name(path.argument());
  name = name.create_sub_string(
    String::Position(0),
    String::Length(name.find(".")));

  info.insert(("path"), JsonString(path.argument()));
  info.insert(("name"), JsonString(name));

  if (path.argument().find("_debug") != String::npos) {
    info.insert(("debug"), JsonTrue());
  } else {
    info.insert(("debug"), JsonFalse());
  }

  info.insert(("address"), JsonString(first_address));
  if (path.argument().find("_v7m") != String::npos) {
    info.insert(("arch"), JsonString("v7m"));
  }
  if (path.argument().find("_v7em") != String::npos) {
    info.insert(("arch"), JsonString("v7em"));
  }
  if (path.argument().find("_v7em_f4sh") != String::npos) {
    info.insert(("arch"), JsonString("v7em_f4sh"));
  }
  if (path.argument().find("_v7em_f5sh") != String::npos) {
    info.insert(("arch"), JsonString("v7em_f5sh"));
  }
  if (path.argument().find("_v7em_f5dh") != String::npos) {
    info.insert(("arch"), JsonString("v7em_f5dh"));
  }

  info.insert(("binaryHash"), JsonString(binary_hash));

  if (info.at(("arch")).is_valid()) {
    info.insert(("type"), JsonString("app"));
  } else {

    info.insert(("type"), JsonString("os"));
    const String build_prefix = "/build_";
    size_t build_pos = path.argument().find(build_prefix);
    if (build_pos != String::npos) {
      String build = path.argument();
      size_t build_name_pos = build_pos + build_prefix.length();
      build = build.create_sub_string(
        String::Position(build_pos + build_prefix.length()),
        String::Length(
          path.argument().find("/", String::Position(build_name_pos))
          - build_name_pos));
      info.insert("build", JsonString(build));
    }
  }

  result.insert("info", info);
  result.insert("labels", labels);

  JsonDocument().save(result, fs::File::Path(json_path));

  return result;
}

json::JsonObject Build::lookup_disassembly(
  const json::JsonObject &disassembly,
  u32 device_address) {

  json::JsonArray labels = disassembly.at(("labels")).to_array();

  for (u32 i = 0; i < labels.count(); i++) {

    json::JsonObject label = labels.at(i).to_object();

    u32 start_address
      = label.at("address").to_string().to_unsigned_long(String::base_16);
    u32 size = label.at(("size")).to_integer();

    if (
      (device_address >= start_address)
      && (device_address < start_address + size)) {
      return label;
    }
  }

  return json::JsonObject();
}

mcu_board_config_t Build::load_mcu_board_config(
  fs::File::Path project_path,
  const var::String &project_name,
  Build::Name build_name,
  sys::Printer &printer) {
  // parse the lst file until mcu_board_config is found
  // lookup secret key from mcu_board_config
  mcu_board_config_t result;
  Reference result_reference(result);
  result_reference.fill<u8>(0);
  String build_folder_name;

  build_folder_name = Build().normalize_name(build_name.argument());

  const String base_path
    = project_path.argument() + "/" + build_folder_name + "/" + project_name;

  const String json_file_path = base_path + ".json";

  json::JsonObject symbols_object
    = Build::import_disassembly(fs::File::Path(base_path), build_name, printer);

  if (symbols_object.is_empty()) {
    printer.error("Failed to import symbols for %s", base_path.cstring());
    return result;
  }

  JsonArray labels_array = symbols_object.at(("labels")).to_array();

  u32 start_address = symbols_object.at(("info"))
                        .to_object()
                        .at(("address"))
                        .to_string()
                        .to_unsigned_long(String::base_16);

  u32 mcu_board_config_offset = static_cast<u32>(-1);
  for (u32 i = 0; i < labels_array.count(); i++) {
    JsonObject label = labels_array.at(i).to_object();

    String name = label.at(("name")).to_string();
    if (name == "mcu_board_config") {
      u32 address
        = label.at(("address")).to_string().to_unsigned_long(String::base_16);

      mcu_board_config_offset = address - start_address;
    }
  }

  File binary_image;
  if (binary_image.open(base_path + ".bin", fs::OpenMode::read_only()) < 0) {

    printer.error("failed to open " + base_path + ".bin");

    return result;
  }

  binary_image.read(File::Location(mcu_board_config_offset), result_reference);

  PRINTER_TRACE(
    printer,
    "Found secret key at address "
      + String::number(result.secret_key_address, "%08x"));
  result.secret_key_address -= (start_address + 1);

  return result;
}

var::String Build::normalize_name(const var::StringView build_name) const {
  String result;
  if (build_name.find("build_") == 0) {
    result = build_name;
  } else {
    result = "build_" + build_name;
  }

  if (!application_architecture().is_empty()) {
    bool is_arch_present = false;
    if (result.find("_v7em_f5dh") != String::npos) {
      is_arch_present = true;
    }
    if (!is_arch_present && result.find("_v7em_f5sh") != String::npos) {
      is_arch_present = true;
    }
    if (!is_arch_present && result.find("_v7em_f4sh") != String::npos) {
      is_arch_present = true;
    }
    if (!is_arch_present && result.find("_v7em") != String::npos) {
      is_arch_present = true;
    }
    if (!is_arch_present && result.find("_v7m") != String::npos) {
      is_arch_present = true;
    }
    if (!is_arch_present) {
      result += "_" + application_architecture();
    }
  }

  return result;
}

void Build::migrate_build_info_list_20200518() {
  JsonArray build_list_array = to_object().at("buildList");

  Vector<BuildImageInfo> migrated_list;

  for (u32 i = 0; i < build_list_array.count(); i++) {
    if (build_list_array.at(i).is_string() == false) {
      return;
    }

    migrated_list.push_back(BuildImageInfo()
                              .set_name(build_list_array.at(i).to_string())
                              .set_image("")
                              .set_hash("")
                              .set_secret_key_position(0)
                              .set_secret_key_size(0)
                              .set_secret_key(""));
  }

  set_build_image_list(migrated_list);
}
