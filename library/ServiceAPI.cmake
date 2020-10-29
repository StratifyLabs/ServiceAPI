
if(NOT DEFINED IS_SDK)
	include(CryptoAPI)
	include(CloudAPI)
	include(SosAPI)

	sos_sdk_include_target(ServiceAPI "${STRATIFYAPI_CONFIG_LIST}")
endif()
