
#include <cstdio>

#include <chrono.hpp>
#include <fs.hpp>
#include <json.hpp>
#include <printer.hpp>
#include <test/Test.hpp>
#include <var.hpp>

#include "service.hpp"

class UnitTest : public test::Test {
public:
  UnitTest(var::StringView name)
    : test::Test(name),
      m_cloud("cloudapitest-2ec81", "AIzaSyCFaqqhpCAQIOXQbtmvcXTvuerk2tPE6tI") {
  }

  bool execute_class_api_case() {
    Document::set_default_cloud(m_cloud);

    TEST_ASSERT_RESULT(login_test());
    printf("%s():%d\n", __FUNCTION__, __LINE__);
    TEST_ASSERT_RESULT(document_test());
    printf("%s():%d\n", __FUNCTION__, __LINE__);

    TEST_ASSERT_RESULT(report_test());
    TEST_ASSERT_RESULT(project_test());
    TEST_ASSERT_RESULT(build_test());
    TEST_ASSERT_RESULT(installer_test());
    TEST_ASSERT_RESULT(thing_test());

    return true;
  }

  bool installer_test() { return true; }
  bool build_test() { return true; }
  bool project_test() { return true; }
  bool job_test() { return true; }
  bool report_test() { return true; }
  bool thing_test() { return true; }
  bool user_test() { return true; }
  bool team_test() { return true; }
  bool hardware_test() { return true; }
  bool document_test() {

    class Generic : public DocumentAccess<Generic> {
    public:
      Generic(const Id &id = "") : DocumentAccess<Generic>("generic", id) {}
    };

    Generic::Id id;
    {
      Generic doc;
      TEST_ASSERT(is_success());
      doc.set_permissions(Generic::Permissions::public_).save();
      TEST_ASSERT(is_success());

      id = doc.id();

      TEST_ASSERT(doc.export_file(File(File::IsOverwrite::yes, "generic.json"))
                    .is_success());
    }

    {
      Generic doc;
      TEST_ASSERT(doc.import_file(File("generic.json")).is_success());
      JsonObject object = JsonDocument().load(File("generic.json"));
      TEST_ASSERT(object.at("permissions").to_string_view() == "public");
      TEST_ASSERT(object.at("documentId").to_string_view() == id.string_view());

      doc.set_permissions(Generic::Permissions::private_);
      TEST_ASSERT(doc.save().is_success());
    }

    {
      Generic doc(id);
      TEST_ASSERT(is_success());
      TEST_ASSERT(doc.get_permissions() == "private");
    }

    return true;
  }

  bool login_test() {
    TEST_ASSERT(
      m_cloud.login("test@stratifylabs.co", "testing-user").is_success());
    return true;
  }

private:
  cloud::Cloud m_cloud;
};
