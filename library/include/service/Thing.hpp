#ifndef SERVICE_API_SERVICE_THING_HPP
#define SERVICE_API_SERVICE_THING_HPP

#include <sos/Sys.hpp>

#include "Document.hpp"

namespace service {

class ThingOptions : public DocumentOptionsAccess<ThingOptions> {
public:
  ThingOptions() { set_path("things"); }

private:
};


/*!
 * \brief Thing class
 * \details The Thing class refers to the Things
 * of Internet of Things. A Thing is product
 * that deploys Stratify OS.
 *
 */
class Thing : public DocumentAccess<Thing> {
public:
  class SystemInformation : public json::JsonValue {
  public:
    JSON_ACCESS_CONSTRUCT_OBJECT(SystemInformation);

    explicit SystemInformation(const sys::SysInfo &info)
      : JsonValue(json::JsonObject()) {
      set_application_signature(
        var::String::number(info.application_signature(), "0x%08X"));
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

    JSON_ACCESS_STRING_WITH_KEY(
      SystemInformation,
      applicationSignature,
      application_signature);
    JSON_ACCESS_STRING(SystemInformation, architecture);
    JSON_ACCESS_STRING_WITH_KEY(SystemInformation, bspGitHash, bsp_git_hash);
    JSON_ACCESS_INTEGER_WITH_KEY(
      SystemInformation,
      cpuFrequency,
      cpu_frequency);
    JSON_ACCESS_STRING_WITH_KEY(SystemInformation, hardwareId, hardware_id);
    JSON_ACCESS_STRING_WITH_KEY(SystemInformation, mcuGitHash, mcu_git_hash);
    JSON_ACCESS_STRING(SystemInformation, name);
    JSON_ACCESS_STRING_WITH_KEY(SystemInformation, projectId, project_id);
    JSON_ACCESS_STRING_WITH_KEY(SystemInformation, serialNumber, serial_number);
    JSON_ACCESS_STRING_WITH_KEY(SystemInformation, sosGitHash, sos_git_hash);
    JSON_ACCESS_STRING_WITH_KEY(SystemInformation, team, team_id);
    JSON_ACCESS_STRING(SystemInformation, version);
  };

  explicit Thing(const Id &id = Id());
  Thing(const sos::Sys::Info &info);

  Thing &set_system_info(const sos::Sys::Info &info);

  JSON_ACCESS_STRING_WITH_KEY(Thing, secretKey, secret_key);
  JSON_ACCESS_OBJECT_WITH_KEY(
    Thing,
    SystemInformation,
    systemInformation,
    system_information);

private:
};

} // namespace service

#endif // SERVICE_API_SERVICE_THING_HPP
