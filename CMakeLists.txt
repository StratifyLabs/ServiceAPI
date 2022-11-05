cmake_minimum_required(VERSION 3.12)
cmsdk2_minimum_required(2.1.2)
project(ServiceAPI
	LANGUAGES CXX
	VERSION 1.2.0)
include(CTest)

option(SERVICE_API_IS_TEST "Enable Service API tests" OFF)

add_subdirectory(library library)
if(SERVICE_API_IS_TEST)
	add_subdirectory(tests tests)
endif()

