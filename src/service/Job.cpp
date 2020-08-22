#include <sapi/var.hpp>
#include <sapi/chrono.hpp>
#include <sapi/fs.hpp>
#include <errno.h>

#include "service/Job.hpp"

using namespace service;


Job::Job(){}


var::String Job::publish(const JobOptions & options){

	String object_path = "jobs/" + options.id();
	//does the job exists
	JobObject job_object = cloud().get_database_object("jobs/" + options.id());

	if( job_object.is_valid() ){
		Timer timeout_timer;
		//need to wait for the job to complete

		job_object.set_input(options.input());
		job_object.set_report(String());

		if( cloud().patch_database_object(object_path, job_object) < 0 ){
			return String();
		}

		//wait for result to post

	}

	return String();
}


JobServer::~JobServer(){
	//if job exists -- delete it from the database
	if( m_id.is_empty() == false ){
		//delete job
		printf("delete job id %s\n", m_id.cstring());
	}
}


var::String JobServer::create(){
	if( m_id.is_empty() == false ){
		//delete the job

		m_id = String();
	}

	m_id = cloud().create_database_object(
				"jobs", JobObject().set_type(type()).set_input(JsonString(""))
				);

	if( m_id.is_empty() ){
		set_error_message("failed to create job");
	}

	return m_id;
}


bool JobServer::listen(){
	if( m_id.is_empty() ){
		return false;
	}


	const String job_path = "jobs/" + id();

	m_timeout_timer.restart();
	if( cloud().listen_database_stream(
				job_path,
				listen_callback_function,
				this
				) < 0 ){
		CLOUD_PRINTER_TRACE("failed to listen to database stream");
		return false;
	}

	return true;
}

bool JobServer::listen_callback(const var::String & event, const var::JsonValue & data){
	bool result = true;
	MCU_UNUSED_ARGUMENT(event);
	if( data.is_valid() && data.is_object() ){
		const String path = "jobs/" + id();
		JobObject object = cloud().get_database_object(
					path
					);



		if( (object.get_input().to_object().is_empty() == false) && callback() ){
			String report_id = callback()(context(), object.set_document_id(id()));

			cloud().patch_database_object(
						path, JobObject().set_document_id(id()).set_type(type()).set_input(JsonString("")).set_report(report_id)
						);


		}
	}
	return result;
}
