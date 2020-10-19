#include <sapi/var.hpp>
#include <sapi/chrono.hpp>
#include <sapi/fs.hpp>
#include <sapi/crypto.hpp>
#include <sapi/calc.hpp>

#include "service/Job.hpp"

using namespace service;


Job::Job(){}


json::JsonValue Job::publish(const JobOptions & options){

	JobDocument job_document;

	if( job_document.download(JobDocumentOptions().set_document_id(options.id())) < 0 ){
		CLOUD_PRINTER_TRACE("failed to download job document");
		return JsonNull();
	}

	const String object_path = "jobs/" + job_document.get_id();

	//does the job exists
	JobObject job_object = cloud().get_database_object(
				object_path,
				cloud::Cloud::IsRequestShallow(true));

	if( job_object.is_valid() ){
		Timer timeout_timer;
		//need to wait for the job to complete

		var::Data crypto_key = Base64::decode(job_document.get_key());

		JobIOValue input_value("", crypto_key, options.input());

		timeout_timer.restart();
		String input_id = cloud().create_database_object(
					object_path + "/input",
					input_value.get_value()
					);
		if( input_id.is_empty() ){
			CLOUD_PRINTER_TRACE("Failed to create object " + cloud().database_traffic());
			return JsonNull();
		}

		json::JsonObject object;

		//wait for result to post
		do {

			object = cloud().get_database_object(object_path + "/output");
			if( object.at(input_id).is_valid() ){
				//job is complete -- delete the output
				cloud().remove_database_object(object_path + "/output/" + input_id);
				return JobIOValue("", object.at(input_id)).decrypt_value(crypto_key);
			}

		} while( (options.timeout().seconds() == 0
							|| timeout_timer < options.timeout())
						 && !is_stop()
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
		if( cloud().remove_database_object("jobs/" + m_id) < 0 ){
			CLOUD_PRINTER_TRACE("failed to delete job " + cloud().database_traffic());
		}
	}

	if( m_document_id.is_empty() == false ){
		if( cloud().remove_document("jobs/" + document_id()) < 0 ){
			CLOUD_PRINTER_TRACE("failed to delete job document " + cloud().document_traffic());
		}
	}
}


var::String JobServer::create(const JobOptions & options){
	if( m_id.is_empty() == false ){
		//delete the job
		cloud().remove_database_object("jobs/" + m_id);
		m_id = String();
	}

	m_id = cloud().create_database_object(
				"jobs", JobObject().set_type(type())
				);

	if( m_id.is_empty() ){
		CLOUD_PRINTER_TRACE(cloud().database_traffic());
		set_error_message("failed to create job");
		return String();
	}

	var::Data key = Random::get_data(256/8);
	set_crypto_key(key);

	m_document_id = JobDocument()
			.set_permissions(options.permissions())
			.set_id(m_id)
			.set_team_id(options.team())
			.set_key(Base64::encode(key))
			.upload(JobDocumentOptions().set_create(true));

	if( m_document_id.is_empty() ){
		CLOUD_PRINTER_TRACE(cloud().document_traffic());
	}

	return m_document_id;
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

JobIOValue& JobIOValue::encrypt_value(
		const json::JsonValue & value,
		const var::Data & crypto_key){

	var::String string_value = JsonDocument()
			.set_flags(JsonDocument::option_compact)
			.stringify(value);

	Aes::CbcCipherData cipher_data = Aes::get_cbc_cipher_data(crypto_key, string_value);

	set_initialization_vector(var::Blob(cipher_data.initialization_vector()).to_string());
	set_blob(Base64::encode(cipher_data.data()));

	return *this;
}

json::JsonValue JobIOValue::decrypt_value(const var::Data & crypto_key){


	//iv is 16 bytes -- 32 characters
	String iv_string = get_initialization_vector();
	String data_string = get_blob();

	Aes::CbcCipherData cipher_data;

	InitializationVector iv;
	Data iv_data = Data::from_string(iv_string);
	if( iv_data.size() != 16 ){
		return JsonNull();
	}
	for(u32 i=0; i < iv.count(); i++){
		iv.at(i) = iv_data.to_const_u8()[i];
	}
	cipher_data.set_initialization_vector( iv );
	cipher_data.set_data( Base64::decode(data_string) );

	String json_string = String(Aes::get_plain_data(crypto_key, cipher_data));

	return JsonDocument().load(json_string);
}

bool JobServer::listen_callback(const var::String & event, const json::JsonValue & data){
	MCU_UNUSED_ARGUMENT(event);
	if( data.is_valid() && data.is_object() ){
		const String path = "jobs/" + id();

		JobObject object = cloud().get_database_object(
					path
					);

		json::JsonKeyValueList<JobIOValue> input_list = object.get_input();

		var::StringList key_list = input_list.get_key_list();

		for(const String& key: key_list){

			if( callback() && (object.input().at(key).is_valid()) ){

				JsonValue output = callback()(
							context(),
							object.get_type(),
							input_list.at(key).decrypt_value(crypto_key())
							);

				cloud().create_database_object(
							path + "/output",
							JobIOValue("", crypto_key(), output).get_value(),
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
