#ifndef SERVICE_API_SERVICE_JOB_HPP
#define SERVICE_API_SERVICE_JOB_HPP

#include <sapi/var/Json.hpp>
#include <sapi/var/String.hpp>
#include <CloudAPI/cloud/CloudObject.hpp>

#include "Report.hpp"

namespace service {

class JobOptions {
	API_AC(JobOptions,var::String,id);
	API_AC(JobOptions,var::JsonObject,input);
	API_AC(JobOptions,chrono::MicroTime,timeout);
};

class JobInputValue : public var::JsonKeyValue {
public:
	JobInputValue(const var::String & key, const var::JsonValue & value) :
		JsonKeyValue(key, value){}
};

class JobOutputValue : public var::JsonKeyValue {
public:
	JobOutputValue(const var::String & key, const var::JsonValue & value) :
		JsonKeyValue(key, value){}
};

class JobObject : public var::JsonObject {
public:

	JobObject(){}
	JobObject(const var::JsonObject & object) : var::JsonObject(object){}

	JSON_ACCESS_STRING_WITH_KEY(JobObject,documentId,document_id);
	JSON_ACCESS_STRING(JobObject,type);
	JSON_ACCESS_OBJECT_LIST(JobObject,JobInputValue,input);
	JSON_ACCESS_OBJECT_LIST(JobObject,JobOutputValue,output);

	bool is_valid() const {
		return get_type().is_empty() == false;
	}
};


class Job : public cloud::CloudAccess {
public:
	Job();

	bool ping(const JobOptions & options);
	var::JsonValue publish(const JobOptions & options);

private:
	API_AC(Job,chrono::MicroTime,timeout);

};

class JobServer : public cloud::CloudAccess {
public:
	JobServer(){}
	~JobServer();
	typedef var::JsonValue (*callback_t)(void * context, const var::String & type, const var::JsonValue & input_value);

	var::String create();
	bool listen();

private:
	API_AC(JobServer,var::String,type);
	API_AB(JobServer,stop,false);
	API_AC(JobServer,chrono::MicroTime,timeout);
	API_AF(JobServer,callback_t,callback,nullptr);
	API_AF(JobServer,void*,context,nullptr);
	API_RAC(JobServer,var::String,id);

	chrono::Timer m_timeout_timer;

	static bool listen_callback_function(void * context, const var::String & event, const var::JsonValue & data){
		return reinterpret_cast<JobServer*>(context)->listen_callback(event, data);
	}

	bool listen_callback(const var::String & event, const var::JsonValue & data);

};

}

#endif // SERVICE_API_SERVICE_JOB_HPP
