#ifndef SERVICE_API_SERVICE_PROJECT_HPP
#define SERVICE_API_SERVICE_PROJECT_HPP

#include <sapi/var/String.hpp>
#include <sapi/var/VersionString.hpp>
#include <sapi/sys/Link.hpp>
#include <CloudAPI/cloud/Document.hpp>

#include "Build.hpp"

namespace service {

class ProjectBuildItem : public var::JsonKeyValue {
public:
	explicit ProjectBuildItem(const var::String& key, const var::JsonValue& value = var::JsonString())
		:JsonKeyValue(key, value){}

	JSON_ACCESS_KEY_VALUE_PAIR_STRING(ProjectBuildItem,id,version);

};

using ProjectBuildList = var::JsonKeyValueList<ProjectBuildItem>;


class ProjectBuildOptions {
public:

private:
	API_ACCESS_COMPOUND(ProjectBuildOptions,var::String,type);
	API_ACCESS_COMPOUND(ProjectBuildOptions,var::String,make_options);
	API_ACCESS_COMPOUND(ProjectBuildOptions,var::String,architecture);
	API_ACCESS_COMPOUND(ProjectBuildOptions,var::String,clean);
	API_ACCESS_COMPOUND(ProjectBuildOptions,var::String,generator);
	API_ACCESS_COMPOUND(ProjectBuildOptions,var::String,cmake_options);
	API_ACCESS_COMPOUND(ProjectBuildOptions,var::String,path);
	API_ACCESS_COMPOUND(ProjectBuildOptions,var::String,name);
	API_ACCESS_COMPOUND(ProjectBuildOptions,var::String,target);
	API_ACCESS_COMPOUND(ProjectBuildOptions,var::String,touch_path);
	API_ACCESS_BOOL(ProjectBuildOptions,dry_run,false);
};

class ProjectOptions : public cloud::DocumentOptionsAccess<ProjectOptions> {
public:
	ProjectOptions(){
		set_path("projects");
	}

private:
	API_ACCESS_BOOL(ProjectOptions,dry_run,false);
	API_ACCESS_COMPOUND(ProjectOptions,var::String,change_description);
	API_ACCESS_COMPOUND(ProjectOptions,var::String,file_path);
	API_ACCESS_COMPOUND(ProjectOptions,var::String,architecture);
	API_ACCESS_COMPOUND(ProjectOptions,var::String,version);
	API_ACCESS_COMPOUND(ProjectOptions,var::String,build_name);

};

/*!
 * \brief The Project class
 * \details A project can either be
 * an application, board support package or data.
 *
 */
class Project : public cloud::DocumentAccess<Project> {
public:

	Project();
	explicit Project(const var::JsonObject& object) : DocumentAccess(object){}

	static var::String file_name(){
		return "sl_settings.json";
	}

	//project has top-level description (including change history)
	//project has a list of builds (versioned-builds)
	//can each build contain the git hash data?
	//a project can also have a list of dependencies (versioned-builds)
	//dependencies can be applications or data

	var::String upload(
			const var::String& id = var::String()
			){
		return Document::upload(
					ProjectOptions()
					.set_document_id(id)
					.set_team_id(get_team_id())
					);
	}

	int publish_build(
			const ProjectOptions& options
			);

	static const var::String application_type(){
		return Build::application_type();
	}

	static const var::String os_type(){ return Build::os_type(); }
	static const var::String data_type(){ return Build::data_type(); }

	JSON_ACCESS_STRING(Project,name);
	JSON_ACCESS_STRING(Project,version);
	JSON_ACCESS_STRING(Project,type);
	JSON_ACCESS_STRING(Project,github);
	JSON_ACCESS_STRING(Project,description);
	JSON_ACCESS_STRING_WITH_KEY(Project,hardwareId,hardware_id);
	JSON_ACCESS_STRING(Project,publisher);
	JSON_ACCESS_STRING(Project,permissions);
	JSON_ACCESS_STRING(Project,readme);
	JSON_ACCESS_STRING_WITH_KEY(Project,ramSize,ram_size);
	JSON_ACCESS_STRING_WITH_KEY(Project,teamId,team_id);
	JSON_ACCESS_OBJECT_LIST_WITH_KEY(Project,ProjectBuildItem,buildList,build_list);

	var::String get_storage_path(
			const ProjectOptions& options
			) const;

	Build download_build(
			const var::String& version
			) const;


	bool is_build_version_valid(
			const var::VersionString & build_version
			) const;

	var::String get_build_id(
			const var::String& version
			) const;

	bool is_update_available(const var::String& current_version);

	bool operator < (const var::VersionString & version) const {
		return compare(version) < 0;
	}

	bool operator > (const var::VersionString & version) const {
		return compare(version) > 0;
	}

	bool operator >= (const var::VersionString & version) const {
		return compare(version) >= 0;
	}

	bool operator <= (const var::VersionString & version) const {
		return compare(version) <= 0;
	}

	bool operator == (const var::VersionString & version) const {
		return compare(version) == 0;
	}

	bool operator != (const var::VersionString & version) const {
		return compare(version) != 0;
	}

	var::JsonArray list();


private:


	int compare(const var::VersionString & version) const;

};

}

#endif // SERVICE_API_SERVICE_PROJECT_HPP
