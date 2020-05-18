#include "service/DatumDocument.hpp"

#include <sapi/sys/Sys.hpp>

using namespace service;

DatumDocument::DatumDocument(const DatumDocumentOptions& options){
	//create the data array
	set_thing(options);

}

DatumDocument::DatumDocument(
		const sys::SysInfo & info
		){
	set_thing(
				DatumDocumentOptions()
				.set_serial_number(info.serial_number().to_string())
				.set_team_id(info.team_id())
				);

}

int DatumDocument::count() const {
	return 0;
}


DatumDocument& DatumDocument::insert(
		size_t position,
		const var::Datum & datum
		){
	data_to_array().insert(position, datum.to_object());
	return *this;
}

DatumDocument& DatumDocument::append(const var::Datum & datum){
	data_to_array().append(datum.to_object());
	return *this;
}

var::Datum DatumDocument::at(u32 idx){
	return var::Datum(data_to_array().at(idx).to_object());
}

//download from cloud
int DatumDocument::download(const var::String & id){
	if( thing().is_empty() ){
		error_message() = "thing has not been assigned";
		return -1;
	}
	return Document::download(
				cloud::DocumentOptions()
				.set_document_id(id)
				);
}

//upload to cloud
var::String DatumDocument::upload(){
	if( thing().is_empty() ){
		error_message() = "thing has not been assigned";
		return var::String();
	}
	return Document::upload(
				DatumDocumentOptions()

				);
}
