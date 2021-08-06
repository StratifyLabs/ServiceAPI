cmake_minimum_required (VERSION 3.12)

if(NOT SOS_SDK_PATH)
	set(SOS_SDK_PATH $ENV{SOS_SDK_PATH})
endif()

set(CMAKE_MODULE_PATH ${SOS_SDK_PATH}/cmake)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_MODULE_PATH}/sos-toolchain.cmake)
include(sos-sdk)

if(NOT DEFINED API_IS_SDK)
	include(CloudAPI)
	include(CryptoAPI)
	include(SosAPI)
	include(SwdAPI)
endif()

set(LOCAL_NAME ServiceAPI)
project(${LOCAL_NAME} CXX)
include(CTest)

option(SERVICE_API_IS_TEST "Enable Service API tests" OFF)

add_subdirectory(library library)
if(SERVICE_API_IS_TEST)
	add_subdirectory(tests tests)
endif()

