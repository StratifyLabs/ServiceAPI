

set(DEPENDENCIES ServiceAPI TestAPI FsAPI)

stratify_api_add_test_executable(ServiceAPI 32768 "${DEPENDENCIES}")



