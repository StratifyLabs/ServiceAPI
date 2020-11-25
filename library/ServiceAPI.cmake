
if(NOT DEFINED IS_SDK)
	include(CryptoAPI)
	include(CloudAPI)
	include(SosAPI)
	include(SwdAPI)
	sos_sdk_include_target(ServiceAPI "${API_CONFIG_LIST}")
endif()
