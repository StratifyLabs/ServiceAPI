
#include <sapi/fmt.hpp>
#include <sapi/var.hpp>
#include <sapi/sys.hpp>
#include <sapi/calc.hpp>
#include <sapi/chrono.hpp>

#include "service/Project.hpp"
#include "service/Build.hpp"

using namespace service;

Project::Project(){}

var::JsonArray Project::list(){
	return Document::list(
				"projects",
				"mask.fieldPaths=name"
				"&mask.fieldPaths=description"
				"&mask.fieldPaths=version"
				"&mask.fieldPaths=github"
				"&mask.fieldPaths=documentId"
				"&mask.fieldPaths=permissions"
				"&mask.fieldPaths=type"
				"&mask.fieldPaths=tags"
				);
}

int Project::publish_build(
		const ProjectOptions& options
		){

	//does the current version already exist
	VersionString version;
	String project_document_id = get_document_id();
	version.string() = get_version();
	set_user_id( cloud().credentials().get_uid() );

	//check the current version against the versions in the build list
	if( is_build_version_valid(version) == false ){
		printer().error("version must be greater than latest published version");
		return -1;
	}

	String permissions = get_permissions();

	if( (permissions != "public") &&
			(permissions != "private") &&
			(permissions != "searchable")
			){
		printer().error("permissions must be <public|private|searchable>");
		return -1;
	}

	CLOUD_PRINTER_TRACE("check for valid project id " + project_document_id);
	//if project hasn't been published before, upload it now and add the document id
	if( (project_document_id == "<invalid>") ||
			project_document_id.is_empty() ){
		CLOUD_PRINTER_TRACE("upload project and assign id");
		project_document_id = upload();
		if( project_document_id.is_empty() ){
			printer().error("failed to pre-publish project");
			return -1;
		}
		CLOUD_PRINTER_TRACE("assign " + project_document_id + " as project id");

		set_document_id(project_document_id);
		printer().key("id", project_document_id);

		int type = Build::decode_build_type( get_type() );
		String type_command;
		if( type == Build::type_os){
			type_command = "os";
		} else {
			type_command = "app";
		}
		printer().key(
					"tip",
					"build project with `sl %s.build:path=%s` then use `sl %s.publish:path=%s,changes=<change description>`",
					type_command.cstring(),
					options.file_path().cstring(),
					type_command.cstring(),
					options.file_path().cstring()
					);
		return 0;
	}

	Project existing_project;

	if( existing_project.download(
				ProjectOptions()
				.set_document_id(project_document_id)
				.set_team_id(get_team_id())
				) < 0 ){
		return -1;
	}

	if( existing_project.get_user_id() != cloud().credentials().get_uid() ){
		printer().error("project permissions error (not owner)");
		return -1;
	}

	//import the build and upload it
	Build build;
	CLOUD_PRINTER_TRACE(
				"import build from " +
				options.file_path()
				);
	if( build.import_from_compiled(options.file_path()) < 0 ){
		printer().error("Failed to import build");
		return -1;
	}

	//add the README if it is available
	fs::File readme;
	String readme_path = options.file_path() + "/README.md";
	String base64_readme;

	if( readme.open(
				readme_path,
				fs::OpenFlags::read_only()
				) >= 0 ){

		printer().key("readme", readme_path);
		CLOUD_PRINTER_TRACE(
					"adding README file " +
					readme_path
					);

		fs::DataFile readme_datafile(
					fs::OpenFlags::append_read_write()
					);
		readme_datafile.write(
					readme,
					fs::File::PageSize(512)
					);
		base64_readme = Base64::encode(readme_datafile.data());
	} else {
		CLOUD_PRINTER_TRACE("failed to open readme file at " + readme_path);
	}

	CLOUD_PRINTER_TRACE("loaded readme " + base64_readme);
	set_readme(base64_readme);
	build.set_readme(base64_readme);
	build.set_description(options.change_description());
	build.set_version(version.string());


	CLOUD_PRINTER_TRACE("assigned readme to build");
	String build_id;
	//printer().open_array("build.upload");
	CLOUD_PRINTER_TRACE("uploading build");
	build_id = build.upload(
				BuildOptions(project_document_id)
				.set_dry_run(options.is_dry_run())
				.set_team_id(get_team_id())
				);
	if( build_id.is_empty() ){
		//failed to upload the build
		printer().error("failed to upload build information");
		return -1;
	}

	build.remove_build_image_data();
	printer().object("buildUpload", build.to_object());

	CLOUD_PRINTER_TRACE("update project build list");

	ProjectBuildList project_build_list = get_build_list();
	project_build_list.push_back(
				ProjectBuildItem(build_id)
				.set_version(version.string())
				);

	set_build_list( project_build_list );

	CLOUD_PRINTER_TRACE(
				"publish build changes to project " +
				project_document_id
				);

	if( !options.is_dry_run() ){
		bool upload_result = upload(project_document_id).is_empty();

		if( upload_result == true ){
			CLOUD_PRINTER_TRACE(
						"failed with message " +
						error_message()
						);

			printer().error(
						"failed to update project"
						);

			return -1;
		}
	}

	printer().object("projectUpload", to_object());
	remove_readme();

	return 0;
}

bool Project::is_build_version_valid(
		const var::VersionString & build_version
		) const {

	JsonKeyValueList<ProjectBuildItem> list = build_list();
	if( list.count() == 0 ){
		return true;
	}

	for(const ProjectBuildItem& item: list){
		VersionString version;
		version.string() = item.get_version();

		if( build_version <= version ){
			printer().warning(
						"current is not greater than latest %s <= %s",
						build_version.string().cstring(),
						version.string().cstring());
			return false;
		}
	}

	return true;
}

var::String Project::get_storage_path(
		const ProjectOptions& options
		) const {

	String build_id = get_build_id( options.version() );
	if( build_id.is_empty() ){
		return String();
	}

	Build build;
	build.set_type( get_type() );
	build.set_application_architecture( options.architecture() );
	String normalized_build_name = build.normalize_name( options.build_name() );

	String result;
	result =
			"builds/"
			+ get_document_id()
			+ "/"
			+ build_id
			+ "/"
			+ normalized_build_name
			+ "/"
			+ get_name();

	return result;
}


Build Project::download_build(
		const var::String& version
		) const {

	String build_id = get_build_id(version);

	Build result;

	result.download(
				BuildOptions(get_document_id())
				.set_document_id(build_id)
				.set_team_id(get_team_id())
				);

	return result;
}

var::String Project::get_build_id(
		const var::String& version
		) const {

	VersionString latest_version;
	JsonKeyValueList<ProjectBuildItem> list = build_list();

	String build_id;

	latest_version.string() = "0.0";
	VersionString version_string;
	version_string.string() = version;


	PRINTER_TRACE(printer(), "Looking for version " + version_string.string());

	for(const ProjectBuildItem& item: list){
		VersionString version;
		version.string() = item.get_version();

		if( version_string.string().is_empty() ){
			//grab the latest
			PRINTER_TRACE(printer(), "Checking for the latest version");
			if( version > latest_version ){
				latest_version = version;
				build_id = item.key();
			}

		} else if( version == version_string ){
			PRINTER_TRACE(printer(), "Found version " + version_string.string());
			build_id	= item.key();
			break;
		}
	}

	return build_id;
}

int Project::compare(const var::VersionString & version) const {
	ProjectBuildList build_list = this->get_build_list();
	VersionString latest_version;

	for(const ProjectBuildItem& item: build_list){
		VersionString build_version;
		build_version.string() = item.get_version();
		if( build_version > latest_version ){
			latest_version = build_version;
		}
	}

	return VersionString::compare(latest_version, version);
}

