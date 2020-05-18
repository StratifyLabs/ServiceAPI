#ifndef SERVICE_API_SERVICE_DATUMDOCUMENT_HPP
#define SERVICE_API_SERVICE_DATUMDOCUMENT_HPP

#include <sapi/sys/Sys.hpp>
#include <sapi/var/String.hpp>
#include <sapi/var/Json.hpp>
#include <sapi/var/Datum.hpp>

#include <CloudAPI/cloud/Document.hpp>


namespace service {

class DatumDocumentOptions : public cloud::DocumentOptionsAccess<DatumDocumentOptions> {
public:
	DatumDocumentOptions(){
		set_path("/data");
	}

private:
	API_ACCESS_COMPOUND(DatumDocumentOptions,var::String,serial_number);

};

class DatumDocument : public cloud::DocumentAccess<DatumDocument> {
public:
	DatumDocument(
		const DatumDocumentOptions& options
			);

	DatumDocument(
			const sys::SysInfo & info
			);

	//download from cloud
	int download(const var::String & id);

	//upload to cloud
	var::String upload();

	JSON_ACCESS_ARRAY(DatumDocument,var::Datum,data);

	int count() const;
	DatumDocument& insert(size_t position, const var::Datum & datum);
	DatumDocument& append(const var::Datum & datum);
	var::Datum at(u32 idx);

	void set_thing(const DatumDocumentOptions& options){
		//m_thing = options.serial_number();
		m_team = options.team_id();
	}

private:

	API_READ_ACCESS_COMPOUND(DatumDocument,var::String,thing);
	API_READ_ACCESS_COMPOUND(DatumDocument,var::String,team);

};

}



#endif // SERVICE_API_SERVICE_DATUMDOCUMENT_HPP
