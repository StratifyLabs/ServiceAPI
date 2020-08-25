#ifndef SERVICE_API_SERVICE_HARDWARE_HPP
#define SERVICE_API_SERVICE_HARDWARE_HPP

#include <sapi/sys/Sys.hpp>
#include <CloudAPI/cloud/Document.hpp>

namespace service {

class HardwareOptions : public cloud::DocumentOptionsAccess<HardwareOptions> {
public:

	HardwareOptions(){
		set_path("hardware");
	}

private:

};

class HardwareFeature : public var::JsonValue {
public:
	JSON_ACCESS_CONSTRUCT_OBJECT(HardwareFeature);
	JSON_ACCESS_STRING(HardwareFeature,key);
	JSON_ACCESS_STRING(HardwareFeature,value);
};

/*!
 * \brief Thing class
 * \details The Thing class refers to the Things
 * of Internet of Things. A Thing is product
 * that deploys Stratify OS.
 *
 */
class Hardware : public cloud::DocumentAccess<Hardware> {
public:
	Hardware();
	JSON_ACCESS_STRING_WITH_KEY(Hardware, imageUrl, image_url);
	JSON_ACCESS_STRING_WITH_KEY(Hardware, mbedDriveName, mbed_drive_name);
	JSON_ACCESS_STRING_WITH_KEY(Hardware, sessionTicket, session_ticket);
	JSON_ACCESS_ARRAY_WITH_KEY(
		Hardware,
		HardwareFeature,
		features,
		feature_list);

private:




};

}

#endif // SERVICE_API_SERVICE_HARDWARE_HPP
