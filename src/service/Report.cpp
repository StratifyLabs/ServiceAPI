#include <sapi/var.hpp>
#include "service/Report.hpp"

using namespace service;

Report::Report(){}


var::String Report::upload(const ReportOptions & options){

	//publish the report to storage
	API_ASSERT(options.file() != nullptr);

	API_ASSERT(!get_name().is_empty());


	String report_id = Document::upload(options);
	if( report_id.is_empty() ){
		return String();
	}

	if( cloud().create_storage_object(
				*options.file(),
				get_storage_path()
				) < 0 ){
		set_error_message( cloud().storage_error() );
		//need to remove the docoument
		return String();
	}

	return report_id;
}


int Report::download_contents(const fs::File & destination){

	CLOUD_PRINTER_TRACE("download report at " + get_storage_path());

	return cloud().get_storage_object(
				get_storage_path(),
				destination);

}
