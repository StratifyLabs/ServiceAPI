#ifndef SERVICE_API_SERVICE_BUILD_HPP
#define SERVICE_API_SERVICE_BUILD_HPP

#include <sapi/sys/Link.hpp>
#include <sapi/calc/Base64.hpp>
#include <CloudAPI/cloud/Document.hpp>

namespace service {

class BuildSecretKeyInfo {
public:

	u32 invalid_position() const {
		return static_cast<u32>(-1);
	}

private:
	API_ACCESS_FUNDAMENTAL(BuildSecretKeyInfo,u32,position,invalid_position());
	API_ACCESS_FUNDAMENTAL(BuildSecretKeyInfo,u32,size,0);
};

class BuildImageInfo : public var::JsonValue {
public:
	BuildImageInfo() : var::JsonValue( var::JsonObject() ){}
	BuildImageInfo(const var::JsonObject& object) : var::JsonValue(object){}

	JSON_ACCESS_STRING(BuildImageInfo,name);
	JSON_ACCESS_STRING(BuildImageInfo,image);
	JSON_ACCESS_STRING(BuildImageInfo,hash);
	JSON_ACCESS_INTEGER(BuildImageInfo,padding);
	JSON_ACCESS_STRING_WITH_KEY(BuildImageInfo,secretKey,secret_key);
	JSON_ACCESS_INTEGER_WITH_KEY(BuildImageInfo,secretKeyPosition,secret_key_position);
	JSON_ACCESS_INTEGER_WITH_KEY(BuildImageInfo,secretKeySize,secret_key_size);

	var::Data get_image_data() const {
		return calc::Base64::decode(get_image());
	}

	BuildImageInfo& set_image_data(const var::Reference& image_reference){
		return set_image( calc::Base64::encode(image_reference) );
	}

};

class BuildOptions : public cloud::DocumentOptionsAccess<BuildOptions> {
public:

	BuildOptions(const var::String& project_id){
		set_project_id(project_id);
		set_path("projects/" + project_id + "/builds");
	}

	var::String create_storage_path() const {
		if( project_id().is_empty() ||
				project_name().is_empty() ||
				build_name().is_empty() ||
				document_id().is_empty()
				){
			//assert here
			return var::String();
		}

		return
				"builds/"
				+ project_id()
				+ "/"
				+ document_id()
				+ "/"
				+ build_name()
				+ "/"
				+ project_name();
	}

private:
	API_ACCESS_BOOL(BuildOptions,dry_run,false);
	API_ACCESS_COMPOUND(BuildOptions,var::String,build_name);
	API_ACCESS_COMPOUND(BuildOptions,var::String,project_id);
	API_ACCESS_COMPOUND(BuildOptions,var::String,project_name);
	API_ACCESS_COMPOUND(BuildOptions,var::String,architecture);
	API_ACCESS_COMPOUND(BuildOptions,var::String,url);
	API_ACCESS_COMPOUND(BuildOptions,var::String,version);

};

/*! \brief Build Class
 * \details The Build class represents a versioned build
 * of a project or just raw file data.
 *
 * Every build can have a dependency list containing
 * other builds that it requires to run properly.
 *
 */
class Build : public cloud::DocumentAccess<Build> {
public:

	using IsIncludeImage = arg::Argument<bool, struct BuildIsIncludeImageTag>;
	using Version = arg::Argument<const var::String &, struct BuildVersionTag>;
	using Url =	arg::Argument<const var::String &, struct BuildUrlTag>;
	using Name = arg::Argument<const var::String &, struct BuildOptionTag>;
	using Architecture =  arg::Argument<const var::String &, struct BuildArchitectureTag>;

	Build();
	Build(const var::JsonObject& object) : DocumentAccess(object){}

	enum types {
		type_unknown,
		type_application,
		type_os,
		type_data
	};

	static const var::String application_type(){ return "app"; }
	static const var::String os_type(){ return "os"; }
	static const var::String data_type(){ return "data"; }
	static const var::String path(){ return "build"; }

	bool is_application() const;
	bool is_os() const;
	bool is_data() const;

	/*! \details Imports a build from a project on the
	 * local filesystem.
	 *
	 */
	int import_from_compiled(const var::String& path);

	JSON_ACCESS_STRING(Build,name);
	JSON_ACCESS_STRING(Build,version);
	JSON_ACCESS_STRING(Build,publisher);
	JSON_ACCESS_STRING(Build,readme);
	JSON_ACCESS_STRING(Build,description);
	JSON_ACCESS_STRING(Build,type);
	JSON_ACCESS_STRING(Build,permissions);
	JSON_ACCESS_STRING(Build,project_id);
	JSON_ACCESS_ARRAY_WITH_KEY(Build,BuildImageInfo,buildList,build_image_list);
	JSON_ACCESS_STRING_WITH_KEY(Build,ramSize,ram_size);
	JSON_ACCESS_BOOL_WITH_KEY(Build,isBuildImage,image_included);

	Build& remove_build_image_data(){
		var::Vector<BuildImageInfo> build_list = build_image_list();
		for(BuildImageInfo& image_info: build_list){
			image_info.set_image("<base64>");
		}
		set_image_included(false);
		return *this;
	}

	BuildSecretKeyInfo get_secret_key(
			const var::String& build_name
			){

		const BuildImageInfo image_info = build_image_info(
					normalize_name(build_name)
					);
		if( image_info.is_valid() ){
			return BuildSecretKeyInfo()
					.set_position(image_info.get_secret_key_position())
					.set_size(image_info.get_secret_key_size());
		}
		return BuildSecretKeyInfo();
	}

	BuildImageInfo build_image_info(
			const var::String& build_name
			){
		var::Vector<BuildImageInfo> list = build_image_list();
		u32 position = list.find(
					BuildImageInfo().set_name(
						normalize_name(build_name)
						),
					[](const BuildImageInfo & a, const BuildImageInfo & b){
			return a.get_name() == b.get_name();
		}
		);
		if( position < list.count() ){
			return list.at(position);
		}
		return BuildImageInfo();
	}

	Build& insert_secret_key(
			const var::String& build_name,
			const var::Reference& secret_key
			);

	Build& append_hash(
			const var::String& build_name
			);

	bool is_build_image_included() const {
		//buildIncludesImage is a legacy support thing
		return is_image_included() ||
				to_object().at(("buildIncludesImage")).to_bool();
	}

	int download(const var::String& url);
	int download(const BuildOptions& options);

	var::Data get_image(const var::String& name) const;
	Build& set_image(const var::String& name, const var::Data& image);

	var::String upload(
			const BuildOptions& options
			);

	static enum types decode_build_type(
			const var::String & type
			);

	static var::String encode_build_type(
			enum types type
			);

	static var::JsonObject import_disassembly(
			fs::File::Path path,
			Build::Name build,
			sys::Printer & printer = null_printer()
			);

	static var::JsonObject lookup_disassembly(
			const var::JsonObject & disassembly,
			u32 device_address
			);

	static mcu_board_config_t load_mcu_board_config(
			fs::File::Path project_path,
			const var::String & project_name,
			Build::Name build_name,
			sys::Printer& printer = null_printer()
			);

	var::String normalize_name(const var::String & build_name) const;

	int decode_build_type() const;

	Build& set_application_architecture(const var::String& value){
		if( decode_build_type() == type_application ){
			m_application_architecture = value;
		}
		return *this;
	}

private:
	API_READ_ACCESS_COMPOUND(Build,var::String,application_architecture);

	var::String build_file_path(
			const var::String & path,
			const var::String & build
			);



};

}

#endif // SERVICE_API_SERVICE_BUILD_HPP
