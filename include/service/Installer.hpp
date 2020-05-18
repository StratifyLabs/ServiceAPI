#ifndef SERVICE_API_SERVICE_INSTALLER_HPP
#define SERVICE_API_SERVICE_INSTALLER_HPP

#include <sapi/var/String.hpp>
#include <sapi/sys/Link.hpp>
#include <CloudAPI/cloud/CloudObject.hpp>


#include "Project.hpp"

namespace service {

class InstallerOptions {

	//if id is provided -- download and install
	API_ACCESS_COMPOUND(InstallerOptions,var::String,project_id);
	API_ACCESS_COMPOUND(InstallerOptions,var::String,team_id);
	API_ACCESS_COMPOUND(InstallerOptions,var::String,url);

	//path is a path to a project
	API_ACCESS_COMPOUND(InstallerOptions,var::String,project_path);
	API_ACCESS_COMPOUND(InstallerOptions,var::String,version);
	API_ACCESS_COMPOUND(InstallerOptions,var::String,build_name);

	//path to an image to install
	API_ACCESS_COMPOUND(InstallerOptions,var::String,binary_path);
	API_ACCESS_BOOL(InstallerOptions,application,false);
	API_ACCESS_BOOL(InstallerOptions,os,false);

	API_ACCESS_COMPOUND(InstallerOptions,var::String,build_path);

	//application options
	API_ACCESS_COMPOUND(InstallerOptions,var::String,destination);
	API_ACCESS_COMPOUND(InstallerOptions,var::String,suffix);
	API_ACCESS_COMPOUND(InstallerOptions,var::String,architecture);
	API_ACCESS_BOOL(InstallerOptions,tightly_coupled_data,false);
	API_ACCESS_BOOL(InstallerOptions,tightly_coupled_code,false);
	API_ACCESS_BOOL(InstallerOptions,external_data,false);
	API_ACCESS_BOOL(InstallerOptions,external_code,false);
	API_ACCESS_BOOL(InstallerOptions,clean,false);
	API_ACCESS_BOOL(InstallerOptions,force,false);
	API_ACCESS_BOOL(InstallerOptions,kill,false);
	API_ACCESS_BOOL(InstallerOptions,flash,false);
	API_ACCESS_BOOL(InstallerOptions,startup,false);
	API_ACCESS_BOOL(InstallerOptions,authenticated,false);
	API_ACCESS_FUNDAMENTAL(InstallerOptions,u32,ram_size,0);
	API_ACCESS_FUNDAMENTAL(InstallerOptions,u32,access_mode,0555);

	//OS options
	API_ACCESS_BOOL(InstallerOptions,verify,false);
	API_ACCESS_BOOL(InstallerOptions,append_hash,false);
	API_ACCESS_BOOL(InstallerOptions,reconnect,false);
	API_ACCESS_COMPOUND(InstallerOptions,chrono::MicroTime,delay);
	API_ACCESS_FUNDAMENTAL(InstallerOptions,u32,retry_reconnect_count,50);

	API_ACCESS_BOOL(InstallerOptions,insert_key,false);
	API_ACCESS_COMPOUND(InstallerOptions,var::String,secret_key);

public:
	InstallerOptions(){
		set_delay( chrono::Milliseconds(500) );
	}

};

class Installer : public cloud::CloudObject {
public:
	Installer(sys::Link * connection);

	bool install(const InstallerOptions& options);

	API_ACCESS_FUNDAMENTAL(Installer,sys::Link*,connection,nullptr);

	void print_transfer_info(
			const fs::File & image,
			const chrono::Timer & transfer_timer
			);

private:
	API_ACCESS_COMPOUND(Installer,var::String,project_name);
	API_ACCESS_COMPOUND(Installer,var::String,project_id);
	API_ACCESS_COMPOUND(Installer,var::String,architecture);

	bool install_url(const InstallerOptions& options);
	bool install_id(const InstallerOptions& options);
	bool install_path(const InstallerOptions& options);
	bool install_binary(const InstallerOptions& options);

	bool import_build_from_project_path(
			const InstallerOptions& options
			);

	bool install_build(
			Build& build,
			const InstallerOptions& options
			);

	bool install_application_build(
			Build& build,
			const InstallerOptions& options
			);

	bool install_os_build(
			Build& build,
			const InstallerOptions& options
			);

	bool install_os_image(
			const fs::File& image,
			const InstallerOptions& options
			);

	bool install_application_image(
			const fs::File& image,
			const InstallerOptions& options
			);

	int save_image_locally(
			const fs::File& image,
			const InstallerOptions& options
			);

	int kill_application(int app_pid);
	int clean_application();


};

}

#endif // SERVICE_API_SERVICE_INSTALLER_HPP
