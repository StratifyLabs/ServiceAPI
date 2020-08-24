#ifndef SERVICE_API_SERVICE_JOB_HPP
#define SERVICE_API_SERVICE_JOB_HPP

#include <sapi/var/Json.hpp>
#include <sapi/var/String.hpp>
#include <CloudAPI/cloud/CloudObject.hpp>

#include "Report.hpp"

namespace service {

class JobOptions {
	API_AC(JobOptions,var::String,id);
	API_AC(JobOptions,var::String,team);
	API_AC(JobOptions,var::Data,key);
	API_AC(JobOptions,var::String,permissions);
	API_AC(JobOptions,var::JsonValue,input);
	API_AC(JobOptions,chrono::MicroTime,timeout);
public:
	JobOptions(){
		set_permissions("private");
	}
};

class JobIOValue : public var::JsonKeyValue, public cloud::CloudAccess {
public:
	JobIOValue(const var::String & key, const var::JsonValue & value) :
		JsonKeyValue(key, value){

	}

	JobIOValue(const var::String & key, const var::Data & crypto_key, const var::JsonValue & value) :
		JsonKeyValue(key, var::JsonObject()){
		encrypt_value(value, crypto_key);
	}

	JSON_ACCESS_STRING_WITH_KEY(JobIOValue,iv,initialization_vector);
	JSON_ACCESS_STRING(JobIOValue,blob);


	JobIOValue& encrypt_value(const var::JsonValue & value, const var::Data & crypto_key);
	var::JsonValue decrypt_value(const var::Data & crypto_key);

};


class JobObject : public var::JsonObject {
public:

	JobObject(){}
	JobObject(const var::JsonObject & object) : var::JsonObject(object){}

	JSON_ACCESS_STRING_WITH_KEY(JobObject,documentId,document_id);
	JSON_ACCESS_STRING(JobObject,type);
	JSON_ACCESS_OBJECT_LIST(JobObject,JobIOValue,input);
	JSON_ACCESS_OBJECT_LIST(JobObject,JobIOValue,output);

	bool is_valid() const {
		return get_type().is_empty() == false;
	}
};

class JobDocumentOptions : public cloud::DocumentOptionsAccess<JobDocumentOptions> {
public:
	JobDocumentOptions(){
		set_path("jobs");
	}
};

class JobDocument : public cloud::DocumentAccess<JobDocument> {
public:
	JobDocument(){}
	JobDocument(const var::JsonObject & object) : DocumentAccess(object){}
	JSON_ACCESS_STRING(JobDocument,key);
	JSON_ACCESS_STRING(JobDocument,id);
	JSON_ACCESS_STRING(JobDocument,type);
};


class Job : public cloud::CloudAccess {
public:
	Job();

	bool ping(const JobOptions & options);
	var::JsonValue publish(const JobOptions & options);

private:
	API_AC(Job,chrono::MicroTime,timeout);
	API_AB(Job,stop,false);

};

class JobServer : public cloud::CloudAccess {
public:
	JobServer(){}
	~JobServer();
	typedef var::JsonValue (*callback_t)(void * context, const var::String & type, const var::JsonValue & input_value);

	var::String create(const JobOptions & options);
	bool listen();

private:
	API_AC(JobServer,var::String,type);
	API_AB(JobServer,stop,false);
	API_AC(JobServer,chrono::MicroTime,timeout);
	API_AF(JobServer,callback_t,callback,nullptr);
	API_AF(JobServer,void*,context,nullptr);
	API_RAC(JobServer,var::String,id);
	API_RAC(JobServer,var::String,document_id);
	API_AC(JobServer,var::Data,crypto_key);

	chrono::Timer m_timeout_timer;

	static bool listen_callback_function(void * context, const var::String & event, const var::JsonValue & data){
		return reinterpret_cast<JobServer*>(context)->listen_callback(event, data);
	}

	bool listen_callback(const var::String & event, const var::JsonValue & data);

};

}

#endif // SERVICE_API_SERVICE_JOB_HPP
