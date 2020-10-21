#ifndef SERVICE_API_SERVICE_USER_HPP
#define SERVICE_API_SERVICE_USER_HPP

#include "Document.hpp"

namespace service {

class User : public DocumentAccess<User> {
public:
  class AdminProfile : public DocumentAccess<AdminProfile> {
  public:
    AdminProfile(const Id &id)
      : DocumentAccess(Path("users") / id / "profile", Id("admin")) {}

    JSON_ACCESS_STRING(AdminProfile, account);
  };

  class PublicProfile : public DocumentAccess<PublicProfile> {
  public:
    PublicProfile(const Id &id)
      : DocumentAccess(Path("users") / id / "profile", Id("public")) {}

    JSON_ACCESS_STRING(PublicProfile, email);
    JSON_ACCESS_STRING(PublicProfile, interests);
    JSON_ACCESS_STRING(PublicProfile, job);
    JSON_ACCESS_STRING_ARRAY_WITH_KEY(
      PublicProfile,
      programmingLanguages,
      programming_language_list);
  };

  class PrivateProfile : public DocumentAccess<PrivateProfile> {
  public:
    PrivateProfile(const Id &id)
      : DocumentAccess(Path("users") / id / "/profile", Id("private")) {}

    JSON_ACCESS_BOOL_WITH_KEY(PrivateProfile, isEmailPublic, email_public);
    JSON_ACCESS_BOOL_WITH_KEY(
      PrivateProfile,
      isInterestsPublic,
      interests_public);
    JSON_ACCESS_BOOL_WITH_KEY(PrivateProfile, isNamePublic, name_public);
    JSON_ACCESS_BOOL_WITH_KEY(
      PrivateProfile,
      isProgrammingLanguagesPublic,
      programming_language_list_public);
    JSON_ACCESS_BOOL_WITH_KEY(
      PrivateProfile,
      isSlLoginCompleted,
      sl_login_completed);
  };

  User(const Id &id);

  JSON_ACCESS_STRING_WITH_KEY(User, displayName, display_name);
  JSON_ACCESS_STRING(User, email);
  JSON_ACCESS_STRING_WITH_KEY(User, loginTime, login_time);
  JSON_ACCESS_STRING_WITH_KEY(User, phoneNumber, phone_number);
  JSON_ACCESS_STRING_WITH_KEY(User, photoUrl, photo_url);

private:
  API_ACCESS_COMPOUND(User, var::String, user_id);
};

} // namespace service

#endif // SERVICE_API_SERVICE_USER_HPP
