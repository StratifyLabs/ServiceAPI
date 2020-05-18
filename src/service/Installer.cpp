﻿#include <sapi/sys.hpp>
#include <sapi/fs.hpp>
#include <sapi/var.hpp>
#include <sapi/chrono.hpp>

#include "service/Installer.hpp"

using namespace service;

Installer::Installer(sys::Link * link_connection) :
	m_connection(link_connection){

}


bool Installer::install(const InstallerOptions& options){

	if( connection()->is_connected_and_is_not_bootloader() ){
		set_architecture( connection()->sys_info().arch() );
	} else {
		set_architecture( options.architecture() );
	}

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

	CLOUD_PRINTER_TRACE("setting project name " + p.get_name());
	set_project_name( p.get_name() );
	CLOUD_PRINTER_TRACE("setting project id " + p.get_document_id());
	set_project_id( p.get_document_id() );

	Build b;

	CLOUD_PRINTER_TRACE("setting build type " + p.get_type());
	b.set_type( p.get_type() );
	b.set_application_architecture( architecture() );

	CLOUD_PRINTER_TRACE("setting architecture " + b.application_architecture());


	String cloud_storage_path =
			p.get_storage_path(
				ProjectOptions()
				.set_version( options.version() )
				.set_build_name( options.build_name() )
				.set_architecture( b.application_architecture() )
				);
	CLOUD_PRINTER_TRACE("getting cloud storage " + cloud_storage_path);

	String build_document_id =
			p.get_build_id(options.version());

	CLOUD_PRINTER_TRACE("build id is " + build_document_id);

	//download the build
	if( b.download(
				BuildOptions(p.get_document_id())
				.set_document_id(build_document_id)
				.set_build_name(
					b.normalize_name(options.build_name())
					)
				.set_storage_path(cloud_storage_path)
				) < 0 ){
		CLOUD_PRINTER_TRACE("failed to download the build image");
		set_error_message(b.error_message());
		return false;
	}

	//now download the actual binary image


	return install_build(b, options);
}


bool Installer::install_binary(const InstallerOptions& options){

	if( FileInfo::suffix(options.binary_path()) == "json" ){
		Build b = Build().load(options.binary_path());
		set_project_name(b.get_name());
		return install_build(b,options);
	}

	DataFile image(File::Path(options.binary_path()));
	image.flags() = OpenFlags::read_write();

	if( options.is_application() ){
		AppfsInfo source_image_info = Appfs::get_info(options.binary_path());

		if( source_image_info.is_valid() == false ){
			set_error_message(
						"The `path` was specified as a file. However, "
						"the file specified did not contain a valid "
						"Stratify OS application binary. The application "
						"header is not valid."
						);
			return false;
		}

		CLOUD_PRINTER_TRACE("binary image id is " + source_image_info.id());

		set_project_name( source_image_info.name() );
		set_project_id( source_image_info.id() );

		return install_application_image(
					image,
					options
					);
	}

	if( options.is_os() ){
		return install_os_image(
					image,
					options
					);
	}

	set_error_message("set os or application for the binary type");
	return false;

}

bool Installer::install_path(const InstallerOptions& options){
	Build b;

	set_project_name( FileInfo::name(options.project_path()) );
	if( b.import_from_compiled(
				options.project_path()
				) < 0 ){
		return false;
	}

	if( b.decode_build_type() == Build::type_application ){
		if( options.is_application() == false ){
			set_error_message("project path is an application but an"
												"application install was not specified");
			return false;
		}
	}

	if( b.decode_build_type() == Build::type_os ){
		if( options.is_os() == false ){
			set_error_message("project path is an os but an"
												"os install was not specified");
			return false;
		}
	}

	return install_build(b, options);
}

bool Installer::install_build(
		Build& build,
		const InstallerOptions& options
		){

	if( build.decode_build_type() == Build::type_application ){
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

	build.set_application_architecture( architecture() );

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

	printer().key("build", build.normalize_name(options.build_name()));
	printer().key("version", build.get_version());
	return install_application_image(
				image,
				InstallerOptions(options)
				.set_version(build.get_version())
				);
}

bool Installer::install_os_build(
		Build& build,
		const InstallerOptions& options
		){

	//insert secret key
	if( options.is_insert_key() ){
		CLOUD_PRINTER_TRACE("insert secret key");
		build.insert_secret_key(
					options.build_name(),
					var::Data::from_string(options.secret_key())
					);

		String secret_key =
				build.build_image_info(options.build_name()).get_secret_key();
		PrinterObject po(printer(), "secretKey");
		printer().key("key256", secret_key);
		printer().key(
					"key128",
					secret_key.create_sub_string(
						String::Position(0),
						String::Length(secret_key.length()/2)
						)
					);
	}

	//append hash
	if( options.is_append_hash() ){
		CLOUD_PRINTER_TRACE("append hash");
		build.append_hash(
					options.build_name()
					);
	}

	if( FileInfo::suffix(options.destination()) == "json" ){
		CLOUD_PRINTER_TRACE("save build as json" + options.destination());
		LinkPath link_path(options.destination(), connection()->driver());

		build.remove_other_build_images(options.build_name());

		if( link_path.is_host_path() ){
			int result = build.save(
						link_path.path()
						);

			if( result < 0 ){
				set_error_message( build.error_message() );
				return false;
			}

			printer().key("destination", link_path.path_description());
			return true;
		} else {
			set_error_message(
						"cannot save JSON export to device (use `host@` prefix)"
						);
			return false;
		}
	}

	DataFile image(OpenFlags::read_only());
	CLOUD_PRINTER_TRACE(
				"get build image " +
				build.normalize_name(options.build_name())
				);

	image.data() = build.get_image(options.build_name());

	CLOUD_PRINTER_TRACE(
				"build image size " +
				String::number(image.data().size())
				);

	if( image.data().size() == 0 ){
		set_error_message(
					"build `" + build.normalize_name(options.build_name()) +
					"` does not exist for " + project_name()
					);
		return false;
	}


	printer().key("build", build.normalize_name(options.build_name()));
	printer().key("version", build.get_version());
	return install_os_image(
				image,
				options
				);
}

bool Installer::install_application_image(
		const fs::File& image,
		const InstallerOptions& options
		){

	{
		int result;

		result = save_image_locally(
					image,
					InstallerOptions(options)
					.set_application()
					);

		if( result >= 0 ){
			return result;
		}
	}

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
			set_error_message(
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
			.set_id( project_id() )
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

	{
		PrinterObject po(printer(), "appfsAttributes");
		printer().key("name", attributes.name());
		printer().key("version", version.string());
		printer().key("id", attributes.id());
		printer().key("flash", attributes.is_flash());
		printer().key("startup", attributes.is_startup());
		printer().key("externalcode", attributes.is_code_external());
		printer().key("externaldata", attributes.is_data_external());
		printer().key("tightlycoupledcode", attributes.is_code_tightly_coupled() != 0);
		printer().key("tightlycoupleddata", attributes.is_data_tightly_coupled() != 0);
		printer().key("authenticated", attributes.is_authenticated());
		if( attributes.ram_size() == 0 ){
			printer().key("ramsize", String("<default>"));
		} else {
			printer().key("ramsize", String::number(attributes.ram_size()));
		}
	}

	if( attributes.apply(image) < 0 ){
		set_error_message("failed to apply file attributes to image");
		return -1;
	}

	if( connection()->is_connected() ){
		CLOUD_PRINTER_TRACE("Is connected");
	} else {
		set_error_message("Not connected");
		return false;
	}

	String destination =
			options.destination().is_empty() ?
				String("/app") :
				options.destination();

	Timer transfer_timer;
	printer().progress_key() = "installing";
	transfer_timer.start();
	int result = connection()->install_app(
				image,
				fs::File::Path(destination),
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


	result = save_image_locally(
				image,
				InstallerOptions(options)
				.set_os()
				);

	if( result >= 0 ){
		return result;
	}

	if( !connection()->is_bootloader() ){

		//bootloader must be invoked
		CLOUD_PRINTER_TRACE("invoking bootloader");
		result = connection()->reset_bootloader();
		if( result < 0 ){
			set_error_message("Failed to invoke the bootloader");
			set_error_message(
						"Failed to invoke bootloader with connnection"
						" error message " +
						connection()->error_message()
						);

			return false;
		}

		CLOUD_PRINTER_TRACE(
					"waiting " +
					String::number(options.delay().milliseconds()) +
					"ms"
					);
		options.delay().wait();

		//now reconnect to the device
		CLOUD_PRINTER_TRACE("reconnect on " + connection()->info().port());
		CLOUD_PRINTER_TRACE("retry is " + String::number(options.retry_reconnect_count()));
		if( connection()->reconnect(
					sys::Link::RetryCount(options.retry_reconnect_count()),
					sys::Link::RetryDelay(options.delay())
					) < 0 ){
			set_error_message("failed to connect to bootloader");
			set_error_message(
						"Failed to connect to bootloader with connnection"
						" error message " +
						connection()->error_message()
						);
			return false;
		}
	}

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

	if( connection()->reset() < 0 ){
		set_error_message(
					"Failed to reset the OS with connection error "
					"message: " +
					connection()->error_message()
					);
		return false;
	}

	if( options.is_reconnect() ){
		CLOUD_PRINTER_TRACE(
					String().format(
						"reconnect %d retries at %dms intervals",
						options.retry_reconnect_count(),
						options.delay().milliseconds()
						)
					);

		printer().progress_key() = "reconnecting";
		for(u32 i=0; i < options.retry_reconnect_count(); i++){
			if( connection()->reconnect(
						sys::Link::RetryCount(1),
						sys::Link::RetryDelay(options.delay())
						) == 0 ){
				break;
			}
			printer().update_progress(
						static_cast<int>(i),
						ProgressCallback::indeterminate_progress_total()
						);
		}
		if( connection()->is_connected() == false ){
			set_error_message("failed to reconnect: " + connection()->error_message());
			return false;
		}

		printer().update_progress(0, 0);
		printer().progress_key() = "progress";
	}

	return true;
}

int Installer::save_image_locally(
		const fs::File& image,
		const InstallerOptions& options
		){
	if( !options.destination().is_empty() ){
		CLOUD_PRINTER_TRACE("saving image to " + options.destination());
		String destination;
		LinkPath link_path(options.destination(), connection()->driver());



		if( link_path.is_host_path() ){
			if( link_path.path().is_empty() ){
				destination =
						project_name() +
						"_" +
						Build()
						.set_type( options.is_os() ? Build::os_type() : Build::application_type() )
						.set_application_architecture( architecture() )
						.normalize_name(options.build_name()) +
						(options.is_os() ?
							 ".bin" :
							 "");
			} else {
				destination = link_path.path();
			}

			File destination_file;
			if( destination_file.create(
						destination,
						File::IsOverwrite(true)) < 0 ){
				set_error_message("failed to create destination"
													"file " + destination);
				return 0;
			}

			destination_file.write(image);
			printer().key("destination", "host@" + destination);
			return 1;
		} else {
			if( options.is_os() ){
				set_error_message(
							"cannot write the os directly to the"
							"device filesystem at " + link_path.path_description()
							);
				return 0;
			}

			return -1;
		}
	}
	return -1;
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


