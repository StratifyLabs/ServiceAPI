
#include <cstdio>

#include <chrono.hpp>
#include <crypto.hpp>
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
#if 0
    TEST_ASSERT_RESULT(document_test());
    TEST_ASSERT_RESULT(hardware_test());
    TEST_ASSERT_RESULT(team_test());
    TEST_ASSERT_RESULT(user_test());
#endif

    TEST_ASSERT_RESULT(report_test());
    TEST_ASSERT_RESULT(thing_test());
    TEST_ASSERT_RESULT(project_test());
    TEST_ASSERT_RESULT(build_test());
    TEST_ASSERT_RESULT(installer_test());

    return true;
  }

  bool installer_test() { return true; }
  bool build_test() { return true; }
  bool project_test() { return true; }
  bool job_test() { return true; }
  bool thing_test() { return true; }

  bool report_test() {

    Report::Id id;
    {
      Report report;

      JsonDocument().save(
        JsonObject()
          .insert("result", JsonString("pass"))
          .insert("time", JsonString("fast"))
          .insert("color", JsonString("blue")),
        File(File::IsOverwrite::yes, "report.json"));

      report.set_permissions(Report::Permissions::private_)
        .save(File("report.json"));

      TEST_ASSERT(is_success());
      id = report.id();
    }

    {

      Report report(id, File(fs::File::IsOverwrite::yes, "download.json"));
      TEST_ASSERT(is_success());

      TEST_ASSERT(
        DataFile().write(File("report.json")).data()
        == DataFile().write(File("download.json")).data());
    }

    return true;
  }

  bool user_test() {

    const StringView user_id = m_cloud.credentials().get_uid();
    User user;
    user.set_permissions(User::Permissions::public_).set_id(user_id).save();
    API_RESET_ERROR();

    User::AdminProfile user_admin_profile(user_id);
    API_RESET_ERROR();
    user_admin_profile.set_permissions(User::Permissions::private_)
      .set_account("free")
      .save();
    TEST_ASSERT(is_success());

    User::PrivateProfile user_private_profile(user_id);
    API_RESET_ERROR();
    user_private_profile.set_permissions(User::Permissions::private_)
      .set_email_public(false)
      .save();
    TEST_ASSERT(is_success());

    User::PublicProfile user_public_profile(user_id);
    API_RESET_ERROR();
    user_public_profile.set_permissions(User::Permissions::public_)
      .set_email("Test@email.com")
      .save();
    TEST_ASSERT(is_success());
    return true;
  }
  bool team_test() {

    Team::Id id;
    {
      Team team;
      TEST_ASSERT(team.set_name("TestTeam")
                    .set_permissions(Team::Permissions::private_)
                    .save()
                    .is_success());
      id = team.get_document_id();
      TEST_ASSERT(id.is_empty() == false);
    }
    {
      Team team(id);
      TEST_ASSERT(is_success());
      TEST_ASSERT(team.get_name() == "TestTeam");

      Team::User user(id);
      user.set_admin(false)
        .set_permissions(Team::Permissions::private_)
        .set_create(true)
        .set_read(true)
        .set_write(false)
        .set_id(m_cloud.credentials().get_uid())
        .save();
    }

    return true;
  }
  bool hardware_test() {

    Array<u8, 32> buffer;
    Random().seed().randomize(buffer);
    const String hardware_id = View(buffer).to_string();
    {
      Hardware hardware;
      hardware.set_permissions(Hardware::Permissions::public_)
        .set_feature_list(
          Vector<Hardware::Feature>()
            .push_back(Hardware::Feature().set_key("flash").set_value("256KB"))
            .push_back(Hardware::Feature().set_key("ram").set_value("32KB")));

      TEST_ASSERT(hardware.set_id(hardware_id).save().is_success());
    }

    {
      Hardware hardware(hardware_id.string_view());
      TEST_ASSERT(is_success());
      TEST_ASSERT(hardware.get_permissions() == "public");
      const auto feature_list = hardware.get_feature_list();
      TEST_ASSERT(feature_list.count() == 2);
      TEST_ASSERT(feature_list.at(0).get_key() == "flash");
      TEST_ASSERT(feature_list.at(0).get_value() == "256KB");
    }

    return true;
  }

  bool document_test() {

    class Generic : public DocumentAccess<Generic> {
    public:
      Generic(const Id &id = "") : DocumentAccess<Generic>("generic", id) {}
    };

    Generic::Id id;
    {
      Generic doc;
      TEST_ASSERT(doc.path() == "generic");
      TEST_ASSERT(doc.id().is_empty());
      TEST_ASSERT(is_success());
      doc.set_permissions(Generic::Permissions::public_).save();
      TEST_ASSERT(is_success());

      id = doc.id();
      TEST_ASSERT(doc.id() == doc.get_document_id());

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
