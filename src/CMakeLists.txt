
if( ${SOS_BUILD_CONFIG} STREQUAL link )
	sos_sdk_add_subdirectory(SOURCELIST service)
endif()


set(SOURCES ${SOURCELIST} PARENT_SCOPE)
