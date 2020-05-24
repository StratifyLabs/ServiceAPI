#ifndef SERVICE_API_SERVICE_BUILD_HPP
#define SERVICE_API_SERVICE_BUILD_HPP

#include <sapi/sys/Link.hpp>
#include <sapi/calc/Base64.hpp>
#include <sapi/crypto/Sha256.hpp>
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

class BuildHashInfo {
	API_ACCESS_COMPOUND(BuildHashInfo,var::String,value);
	API_ACCESS_FUNDAMENTAL(BuildHashInfo,u32,size,0);
	API_ACCESS_FUNDAMENTAL(BuildHashInfo,u32,padding,0);

public:

	static BuildHashInfo calculate(var::Data& image){
		crypto::Sha256 hash;

		if( hash.initialize() < 0 ){
			return BuildHashInfo();
		}

		u32 image_size = image.size();

		u32 padding_length = hash.length() - image_size % hash.length();
		if( padding_length == hash.length() ){
			padding_length = 0;
		}

		if( padding_length > 0 ){
			var::Data padding_block = var::Data(padding_length);
			padding_block.fill<u8>(0xff);
			image.append(padding_block);
		}
		//calculate the hash for block
		hash.start();
		hash << image;
		hash.finish();

		return BuildHashInfo()
				.set_value(hash.to_string())
				.set_padding(padding_length)
				.set_size(image.size());
	}
};

class BuildSectionImageInfo : public var::JsonKeyValue {
public:
	BuildSectionImageInfo(const var::String& key) :
		JsonKeyValue(key, var::JsonObject()){
	}

	BuildSectionImageInfo(const var::String& key, const var::JsonObject& object) :
		JsonKeyValue(key, object){
	}

	JSON_ACCESS_STRING(BuildSectionImageInfo,image);
	JSON_ACCESS_STRING(BuildSectionImageInfo,hash);
	JSON_ACCESS_INTEGER(BuildSectionImageInfo,padding);

	var::Data get_image_data() const {
		return calc::Base64::decode(get_image())
				.append(var::Data::from_string(get_hash()));
	}

	BuildSectionImageInfo& set_image_data(const var::Reference& image_reference){
		return set_image( calc::Base64::encode(image_reference) );
	}

	BuildSectionImageInfo& calculate_hash(){
		var::Data image = get_image_data();
		BuildHashInfo hash_info = BuildHashInfo::calculate(image);
		set_hash(hash_info.value());
		set_padding(hash_info.padding());
		if( hash_info.padding() ){
			set_image_data(image);
		}
		return *this;
	}
};

class BuildImageInfo : public var::JsonValue {
public:
	JSON_ACCESS_CONSTRUCT_OBJECT(BuildImageInfo);

	JSON_ACCESS_STRING(BuildImageInfo,name);
	JSON_ACCESS_STRING(BuildImageInfo,image);
	JSON_ACCESS_STRING(BuildImageInfo,hash);
	JSON_ACCESS_INTEGER(BuildImageInfo,padding);
	JSON_ACCESS_STRING_WITH_KEY(BuildImageInfo,secretKey,secret_key);
	JSON_ACCESS_INTEGER_WITH_KEY(BuildImageInfo,secretKeyPosition,secret_key_position);
	JSON_ACCESS_INTEGER_WITH_KEY(BuildImageInfo,secretKeySize,secret_key_size);
	JSON_ACCESS_OBJECT_LIST_WITH_KEY(BuildImageInfo,BuildSectionImageInfo,sections,section_list);

	var::Data get_image_data() const {
		return calc::Base64::decode(get_image())
				.append(var::Data::from_string(get_hash()));
	}

	BuildImageInfo& set_image_data(const var::Reference& image_reference){
		return set_image( calc::Base64::encode(image_reference) );
	}

	BuildImageInfo& calculate_hash(){
		var::Data image = get_image_data();
		BuildHashInfo hash_info = BuildHashInfo::calculate(image);
		set_hash(hash_info.value());
		set_padding(hash_info.padding());
		if( hash_info.padding() ){
			set_image_data(image);
		}
		return *this;
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
	API_ACCESS_COMPOUND(BuildOptions,var::String,storage_path);

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
	JSON_ACCESS_STRING_WITH_KEY(Build,projectId,project_id);
	JSON_ACCESS_ARRAY_WITH_KEY(Build,BuildImageInfo,buildList,build_image_list);
	JSON_ACCESS_STRING_ARRAY_WITH_KEY(Build,buildList,build_image_list_20200518); //migration list
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

	Build& remove_other_build_images(const var::String& keep_name){
		var::Vector<BuildImageInfo> build_list = build_image_list();
		var::Vector<BuildImageInfo> updated_build_list;
		for(BuildImageInfo& image_info: build_list){
			if( image_info.get_name() == normalize_name(keep_name) ){
				updated_build_list.push_back(image_info);
			}
		}
		set_build_image_list(updated_build_list);
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
			) const {
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

	var::String normalize_name(const var::String & build_name) const;

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


	int decode_build_type() const;

	Build& set_application_architecture(const var::String& value){
		if( decode_build_type() == type_application ){
			m_application_architecture = value;
		}
		return *this;
	}

private:
	API_READ_ACCESS_COMPOUND(Build,var::String,application_architecture);

	class SectionPathInfo {
		API_ACCESS_COMPOUND(SectionPathInfo,var::String,name);
		API_ACCESS_COMPOUND(SectionPathInfo,var::String,path);
	};


	var::String get_build_file_path(
			const var::String & path,
			const var::String & build
			);

	var::Vector<SectionPathInfo> get_section_image_path_list(
			const var::String & path,
			const var::String & build
			);


	var::String calculate_hash(var::Data& image);
	void migrate_build_info_list_20200518();



};

}

#endif // SERVICE_API_SERVICE_BUILD_HPP
