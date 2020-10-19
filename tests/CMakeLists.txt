

set(DEPENDENCIES TestAPI FsAPI JsonAPI InetAPI)

stratify_api_add_test_executable(ServiceAPI 32768 "${DEPENDENCIES}")



