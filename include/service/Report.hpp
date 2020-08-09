#ifndef SERVICE_API_SERVICE_REPORT_HPP
#define SERVICE_API_SERVICE_REPORT_HPP

#include <sapi/fs/File.hpp>
#include <CloudAPI/cloud/Document.hpp>

namespace service {

class ReportOptions: public cloud::DocumentOptionsAccess<ReportOptions> {
public:
	ReportOptions(){
		set_path("reports");
	}

	API_AF(ReportOptions,const fs::File*,file,nullptr);

};

class Report : public cloud::DocumentAccess<Report> {
public:
	Report();

	JSON_ACCESS_STRING(Report,name);

	//document ID, team ID, user ID, and timestamp is inherited from Document

	//available if a thing is associatd with this report
	JSON_ACCESS_STRING_WITH_KEY(Report,thingId,thing_id);
	//if a project is associated with this report
	JSON_ACCESS_STRING_WITH_KEY(Report,projectId,project_id);
	//comma separated list of tags
	JSON_ACCESS_STRING(Report,tags);

	var::String upload(const ReportOptions& options);

	int download_contents(const fs::File& destination);

	var::String get_storage_path() const {
		API_ASSERT(!get_document_id().is_empty());
		return "reports/" + get_document_id() + ".md";
	}

	var::String get_document_path() const {
		if( get_team_id().is_empty() ){
			return "reports/" + get_user_id() + "/" + get_document_id();
		}

		return "reports/" + get_team_id() + "/" + get_document_id();
	}

};


}

#endif // SERVICE_API_SERVICE_REPORT_HPP
