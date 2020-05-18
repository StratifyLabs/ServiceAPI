#ifndef SERVICE_API_SERVICE_THING_HPP
#define SERVICE_API_SERVICE_THING_HPP

#include <sapi/sys/Sys.hpp>
#include <sapi/var/Datum.hpp>
#include <CloudAPI/cloud/Document.hpp>

namespace service {

class ThingOptions : public cloud::DocumentOptionsAccess<ThingOptions> {
public:

	ThingOptions(){
		set_path("things");
	}

private:

};

class ThingSystemInformation : public var::JsonValue {
public:
	JSON_ACCESS_CONSTRUCT_OBJECT(ThingSystemInformation);

	ThingSystemInformation(const sys::SysInfo & info){
		set_application_signature(var::String::number(info.application_signature(), "0x%08X"));
		set_architecture(info.cpu_architecture());
		set_bsp_git_hash(info.bsp_git_hash());
		set_cpu_frequency(info.cpu_frequency());
		set_hardware_id(var::String::number(info.hardware_id(), "0x%08X"));
		set_mcu_git_hash(info.mcu_git_hash());
		set_name(info.name());
		set_project_id(info.id());
		set_serial_number(info.serial_number().to_string());
		set_sos_git_hash(info.sos_git_hash());
		set_team_id(info.team_id());
		set_version(info.system_version());
	}

	JSON_ACCESS_STRING_WITH_KEY(ThingSystemInformation,applicationSignature,application_signature);
	JSON_ACCESS_STRING(ThingSystemInformation,architecture);
	JSON_ACCESS_STRING_WITH_KEY(ThingSystemInformation,bspGitHash,bsp_git_hash);
	JSON_ACCESS_INTEGER_WITH_KEY(ThingSystemInformation,cpuFrequency,cpu_frequency);
	JSON_ACCESS_STRING_WITH_KEY(ThingSystemInformation,hardwareId,hardware_id);
	JSON_ACCESS_STRING_WITH_KEY(ThingSystemInformation,mcuGitHash,mcu_git_hash);
	JSON_ACCESS_STRING(ThingSystemInformation,name);
	JSON_ACCESS_STRING_WITH_KEY(ThingSystemInformation,projectId,project_id);
	JSON_ACCESS_STRING_WITH_KEY(ThingSystemInformation,serialNumber,serial_number);
	JSON_ACCESS_STRING_WITH_KEY(ThingSystemInformation,sosGitHash,sos_git_hash);
	JSON_ACCESS_STRING_WITH_KEY(ThingSystemInformation,teamId,team_id);
	JSON_ACCESS_STRING(ThingSystemInformation,version);

};

/*!
 * \brief Thing class
 * \details The Thing class refers to the Things
 * of Internet of Things. A Thing is product
 * that deploys Stratify OS.
 *
 */
class Thing : public cloud::DocumentAccess<Thing> {
public:
	Thing();
	Thing(const sys::SysInfo & info);

	JSON_ACCESS_STRING_WITH_KEY(Thing,secretKey,secret_key);
	JSON_ACCESS_STRING(Thing,permissions);
	JSON_ACCESS_OBJECT_WITH_KEY(Thing,ThingSystemInformation,systemInformation,system_information);

	Thing & set_secret_key(
			const var::Reference & secret_key
			){
		set_secret_key(secret_key.to_string());
		return *this;
	}

	int download(const ThingOptions& options){
		return Document::download(options);
	}

	int download(const sys::SysInfo & info){
		return Document::download(
					ThingOptions()
					.set_document_id(info.serial_number().to_string())
					.set_team_id(info.team_id())
					);
	}

	var::String upload(
			IsCreate is_create
			);

	var::String update(){
		return upload(
					IsCreate(false)
					);
	}

private:




};

}

#endif // SERVICE_API_SERVICE_THING_HPP
