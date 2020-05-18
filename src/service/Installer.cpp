#include <sapi/sys.hpp>
#include <sapi/fs.hpp>
#include <sapi/var.hpp>
#include <sapi/chrono.hpp>

#include "service/Installer.hpp"

using namespace service;

Installer::Installer(sys::Link * link_connection) :
	m_connection(link_connection){

}


bool Installer::install(const InstallerOptions& options){

	if( !options.url().is_empty() ){
		CLOUD_PRINTER_TRACE("install from url " + options.url());
		return install_url(options);
	}

	if( !options.project_id().is_empty() ){
		CLOUD_PRINTER_TRACE("install from id " + options.project_id());
		return install_id(options);
	}

	if( !options.binary_path().is_empty() ){
		CLOUD_PRINTER_TRACE("install from binary path " + options.binary_path());
		return install_binary(options);
	}

	if( !options.project_path().is_empty() ){
		CLOUD_PRINTER_TRACE("install from project path " + options.project_path());
		return install_path(options);
	}

	set_error_message(
				"must set url, project id, binary path, or project path"
				);
	return false;
}

bool Installer::install_url(const InstallerOptions& options){
	Build b;
	if( b.download(options.url()) < 0 ){
		set_error_message( b.error_message() );
		return false;
	}
	return install_build(b, options);
}

bool Installer::install_id(const InstallerOptions& options){
	Project p;

	if( p.download(
				ProjectOptions()
				.set_document_id(options.project_id())
				.set_team_id(options.team_id())
				)< 0 ){
		return false;
	}

	set_project_name( p.get_name() );
	Build b;

	b.set_type( p.get_type() );
	b.set_application_architecture(
				connection()->sys_info().arch()
				);

	//download the build
	if( b.download(
				BuildOptions(p.get_document_id())
				.set_build_name(
					b.normalize_name(options.build_name()))
				) < 0 ){
		return false;
	}

	b.set_project_id(p.get_document_id());

	return install_build(b, options);
}


bool Installer::install_binary(const InstallerOptions& options){

	DataFile image(File::Path(options.binary_path()));
	image.flags() = OpenFlags::read_write();

	if( options.is_binary_application() ){
		AppfsInfo source_image_info = Appfs::get_info(options.binary_path());

		if( source_image_info.is_valid() == false ){
			set_error_message(
						options.binary_path() +
						" is not a valid application binary file"
						);

			set_troubleshoot_message(
						"The `path` was specified as a file. However, "
						"the file specified did not contain a valid "
						"Stratify OS application binary. The application "
						"header is not valid."
						);
			return false;
		}

		return install_application_image(
					image,
					options
					);
	}




	return install_os_image(
				image,
				options
				);
}

bool Installer::install_path(const InstallerOptions& options){
	Build b;

	set_project_name( FileInfo::name(options.project_path()) );
	if( b.import_from_compiled(
				options.project_path()
				) < 0 ){
		return false;
	}

	return install_build(b, options);
}

bool Installer::install_build(
		Build& build,
		const InstallerOptions& options
		){

	if(build.decode_build_type() == Build::type_application ){
		CLOUD_PRINTER_TRACE("installing application build");
		return install_application_build(build, options);
	}

	if( build.decode_build_type() == Build::type_os ){
		CLOUD_PRINTER_TRACE("installing OS build");
		return install_os_build(build, options);
	}

	set_error_message(
				"cannot install build of type `" + build.get_type() + "`"
				);
	return false;
}

bool Installer::install_application_build(
		Build& build,
		const InstallerOptions& options
		){

	build.set_application_architecture(
				connection()->sys_info().arch()
				);

	DataFile image(OpenFlags::read_write());
	image.data() = build.get_image(
				options.build_name()
				);

	if( image.data().size() == 0 ){
		set_error_message(
					"can't install build with name " +
					build.normalize_name(options.build_name())
					);
		return false;
	}

	return install_application_image(
				image,
				options
				);
}

bool Installer::install_os_build(
		Build& build,
		const InstallerOptions& options
		){

	if( connection()->is_bootloader() == false ){
		set_error_message(
					"cannot install os because bootloader is not connected"
					);
		return false;
	}

	//insert secret key
	if( options.is_insert_key() ){
		build.insert_secret_key(
					options.build_name(),
					var::Data::from_string(options.secret_key())
					);
	}

	//append hash
	if( options.is_append_hash() ){
		build.append_hash(
					options.build_name()
					);
	}

	DataFile image(OpenFlags::read_only());
	image.data() = build.get_image(options.build_name());

	return install_os_image(
				image,
				options
				);
}

bool Installer::install_application_image(
		const fs::File& image,
		const InstallerOptions& options
		){

	int app_pid = connection()->get_pid(project_name());
	if( options.is_kill() ){
		if( app_pid > 0 ){
			printer().key(
						"kill",
						project_name()
						);
			if( kill_application(app_pid) < 0 ){
				return false;
			}
		} else {
			CLOUD_PRINTER_TRACE(project_name() + " is not running");
		}
	} else {
		if( app_pid > 0 && !options.is_force() ){
			set_error_message(
						project_name() +" is currently running"
						);
			set_troubleshoot_message(
						"`sl` cannot install an application "
						"if the application is currently executing. "
						"If you specifiy `kill=true` when installing, "
						"the application will be sent a kill signal "
						"as part of the installation."
						);
			return false;
		}
	}

	if( options.is_clean() ){
		clean_application();
	}

	bool is_flash_available;

	if( options.is_flash() ){
		is_flash_available = Appfs::is_flash_available(
					fs::File::LinkDriver(connection()->driver())
					);
	} else {
		is_flash_available = false;
	}

	VersionString version;
	version.string() = options.version();

	AppfsFileAttributes attributes;
	attributes
			.set_name(project_name() + options.suffix())
			.set_startup(options.is_startup())
			.set_flash(is_flash_available)
			.set_code_external(options.is_external_code())
			.set_data_external(options.is_external_data())
			.set_code_tightly_coupled(options.is_tightly_coupled_code())
			.set_data_tightly_coupled(options.is_tightly_coupled_data())
			.set_ram_size(options.ram_size())
			.set_authenticated(options.is_authenticated())
			.set_access_mode(options.access_mode())
			.set_version(version.to_bcd16());

	printer().open_object("appfsAttributes");
	printer().key("name", attributes.name());
	printer().key("version", attributes.version());
	printer().key("id", attributes.id());
	printer().key("ram", !attributes.is_flash());
	printer().key("flash", attributes.is_flash());
	printer().key("startup", attributes.is_startup());
	printer().key("externalcode", attributes.is_code_external());
	printer().key("externaldata", attributes.is_data_external());
	printer().key("tightlycoupledcode", attributes.is_code_tightly_coupled() != 0);
	printer().key("tightlycoupleddata", attributes.is_data_tightly_coupled() != 0);
	printer().key("authenticated", attributes.is_authenticated());
	if( attributes.ram_size() ){
		printer().key("ramsize", String("<default>"));
	} else {
		printer().key("ramsize", String::number(attributes.ram_size()));
	}
	printer().close_object();

	if( attributes.apply(image) < 0 ){
		printer().error("failed to apply file attributes to image");
		return -1;
	}

	if( connection()->is_connected() ){
		CLOUD_PRINTER_TRACE("Is connected");
	} else {
		set_error_message("Not connected");
		return false;
	}

	Timer transfer_timer;
	printer().progress_key() = "installing";
	transfer_timer.start();
	int result = connection()->install_app(
				image,
				fs::File::Path(options.destination()),
				sys::Link::ApplicationName(attributes.name()),
				printer().progress_callback()
				);
	transfer_timer.stop();
	printer().progress_key() = "progress";

	if( result < 0 ){
		set_error_message(
					"Failed to install application " +
					connection()->error_message()
					);
		return false;
	}
	print_transfer_info(image, transfer_timer);
	return true;
}

bool Installer::install_os_image(
		const File &image,
		const InstallerOptions& options){
	int result;
	CLOUD_PRINTER_TRACE("Installing OS");
	Timer transfer_timer;
	printer().progress_key() = "installing";
	transfer_timer.start();
	result = connection()->update_os(
				image,
				sys::Link::IsVerify(options.is_verify()),
				printer(),
				sys::Link::BootloaderRetryCount(options.retry_reconnect_count())
				);
	transfer_timer.stop();
	printer().progress_key() = "progress";
	if( result < 0 ){
		CLOUD_PRINTER_TRACE(
					String().format(
						"failed to install with return value %d -> %s",
						result,
						connection()->error_message().cstring()
						)
					);
		return false;
	}
	print_transfer_info(image, transfer_timer);
	return true;
}


int Installer::kill_application(int app_pid){

	if( app_pid <= 0 ){
		return 0;
	}

	CLOUD_PRINTER_TRACE(
				String().format("killing %s:%d", project_name().cstring(), app_pid)
				);
	if( connection()->kill_pid(app_pid, LINK_SIGINT) < 0 ){
		return -1;
	}

	//give a little time for the program to shut down
	CLOUD_PRINTER_TRACE("Wait for killed program to stop");
	int retries = 0;
	while( ((app_pid = connection()->get_pid(project_name())) > 0) && (retries < 5) ){
		retries++;
		chrono::wait(chrono::Milliseconds(100));
	}

	return 0;
}

int Installer::clean_application(){
	String unlink_path;
	int count = 0;
	printer().key("clean", project_name());
	unlink_path = String("/app/flash/") + project_name();
	while( File::remove(
					 unlink_path,
					 fs::File::LinkDriver(connection()->driver())
					 ) >= 0 ){
		//delete all versions
		count++;
	}

	unlink_path = String("/app/ram/") + project_name();
	while( File::remove(
					 unlink_path,
					 fs::File::LinkDriver(connection()->driver())
					 ) >= 0 ){
		count++;
	}
	return count;
}

void Installer::print_transfer_info(
		const fs::File & image,
		const chrono::Timer & transfer_timer
		){


	const u32 size = image.size();
	PrinterObject po(printer(), "transfer");
	printer().key(
				"size",
				"%d",
				size
				);

	printer().key(
				"duration",
				"%0.3fs",
				transfer_timer.microseconds() * 1.0f / 1000000.0f
				);

	printer().key(
				"rate",
				"%0.3fKB/s",
				size *1.0f / transfer_timer.microseconds() * 1000000.0f / 1024.0f
				);

}


