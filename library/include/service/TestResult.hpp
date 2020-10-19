#ifndef SERVICE_API_SERVICE_TESTRESULT_HPP
#define SERVICE_API_SERVICE_TESTRESULT_HPP

#include <CloudAPI/cloud/Document.hpp>

namespace service {

class TestResult : public Document {
public:
	TestResult();

	static const var::String path(){ return "test"; }

};

}

#endif // SERVICE_API_SERVICE_TESTRESULT_HPP
