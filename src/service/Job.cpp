#include <sapi/var.hpp>
#include <sapi/chrono.hpp>
#include <sapi/fs.hpp>
#include <errno.h>

#include "service/Job.hpp"

using namespace service;


Job::Job(){}


var::JsonValue Job::publish(const JobOptions & options){

	const String object_path = "jobs/" + options.id();
	//does the job exists
	JobObject job_object = cloud().get_database_object(
				object_path,
				cloud::Cloud::IsRequestShallow(true));

	if( job_object.is_valid() ){
		Timer timeout_timer;
		//need to wait for the job to complete

		timeout_timer.restart();
		String input_id = cloud().create_database_object(object_path + "/input", options.input());
		if( input_id.is_empty() ){
			return JsonNull();
		}

		var::JsonObject object;

		//wait for result to post
		do {

			object = cloud().get_database_object(object_path + "/output");
			if( object.at(input_id).is_valid() ){
				//job is complete -- delete the output
				cloud().remove_database_object(object_path + "/output/" + input_id);
				return object.at(input_id);
			}

		} while( (options.timeout().seconds() == 0
							|| timeout_timer < options.timeout())
						 );

	}

	return JsonNull();
}

bool Job::ping(const JobOptions & options){

	String path = "jobs/" + options.id();
	int result = cloud().get_database_object(
				path,
				NullFile(),
				cloud::Cloud::IsRequestShallow(true));

	return result >= 0;
}



JobServer::~JobServer(){
	//if job exists -- delete it from the database
	if( m_id.is_empty() == false ){
		//delete job
		printf("delete job id %s\n", m_id.cstring());
		if( cloud().remove_database_object("jobs/" + m_id) < 0 ){
			CLOUD_PRINTER_TRACE("failed to delete " + cloud().database_traffic());
		}
	}
}


var::String JobServer::create(){
	if( m_id.is_empty() == false ){
		//delete the job
		CLOUD_PRINTER_TRACE("remove old");
		cloud().remove_database_object("jobs/" + m_id);
		m_id = String();
	}

	CLOUD_PRINTER_TRACE("create object");
	m_id = cloud().create_database_object(
				"jobs", JobObject().set_type(type())
				);

	if( m_id.is_empty() ){
		set_error_message("failed to create job");
	}
	CLOUD_PRINTER_TRACE("created " + m_id);

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
	MCU_UNUSED_ARGUMENT(event);
	if( data.is_valid() && data.is_object() ){
		const String path = "jobs/" + id();

		JobObject object = cloud().get_database_object(
					path
					);

		var::JsonKeyValueList<JobInputValue> input_list = object.get_input();
		var::JsonObject output_object;

		var::StringList key_list = input_list.get_key_list();

		for(const String& key: key_list){

			if( callback() && (object.input().at(key).is_valid()) ){
				JsonValue output = callback()(
							context(),
							object.get_type(),
							input_list.at(key)
							);

				output_object.insert(key, output);
				cloud().create_database_object(
							path + "/output",
							output,
							key
							);

				cloud().remove_database_object(
							path + "/input/" + key);
			}
		}
	} else {
		if( callback() ){
			callback()(context(), "", JsonNull());
		}
	}
	return is_stop() == false;
}
