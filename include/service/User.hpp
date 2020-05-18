#ifndef SERVICE_API_SERVICE_USER_HPP
#define SERVICE_API_SERVICE_USER_HPP

#include <CloudAPI/cloud/Document.hpp>

namespace service {

class UserAdminProfileOptions : public cloud::DocumentOptionsAccess<UserAdminProfileOptions> {
public:
	UserAdminProfileOptions(const var::String& user_id){
		set_path("users/" + user_id + "/admin");
	}
};

class UserAdminProfile : public cloud::DocumentAccess<UserAdminProfile> {
public:
	JSON_ACCESS_STRING(UserAdminProfile,account);
};

class UserPublicProfileOptions : public cloud::DocumentOptionsAccess<UserPublicProfileOptions> {
public:
	UserPublicProfileOptions(const var::String& user_id){
		set_path("users/" + user_id + "/public");
	}
};

class UserPublicProfile : public cloud::DocumentAccess<UserPublicProfile> {
public:
	JSON_ACCESS_STRING(UserPublicProfile,email);
	JSON_ACCESS_STRING(UserPublicProfile,interests);
	JSON_ACCESS_STRING(UserPublicProfile,job);
	JSON_ACCESS_STRING_ARRAY_WITH_KEY(
			UserPublicProfile,
			programmingLanguages,
			programming_language_list);
};

class UserPrivateProfileOptions : public cloud::DocumentOptionsAccess<UserPrivateProfileOptions> {
public:
	UserPrivateProfileOptions(const var::String& user_id){
		set_path("users/" + user_id + "/private");
	}
};

class UserPrivateProfile : public cloud::DocumentAccess<UserPrivateProfile> {
public:
	JSON_ACCESS_BOOL_WITH_KEY(UserPrivateProfile,isEmailPublic,email_public);
	JSON_ACCESS_BOOL_WITH_KEY(UserPrivateProfile,isInterestsPublic,interests_public);
	JSON_ACCESS_BOOL_WITH_KEY(UserPrivateProfile,isNamePublic,name_public);
	JSON_ACCESS_BOOL_WITH_KEY(UserPrivateProfile,isProgrammingLanguagesPublic,programming_language_list_public);
	JSON_ACCESS_BOOL_WITH_KEY(UserPrivateProfile,isSlLoginCompleted,sl_login_completed);

};

class UserOptions : public cloud::DocumentOptionsAccess<UserOptions> {
public:
	UserOptions(){
		set_path("users");
	}

};

class User : public cloud::DocumentAccess<User> {
public:
	User();
	int download(const var::String & id){
		return Document::download(
					UserOptions()
					.set_document_id(id)
					);
	}

	JSON_ACCESS_STRING_WITH_KEY(User,displayName,display_name);
	JSON_ACCESS_STRING(User,email);
	JSON_ACCESS_STRING_WITH_KEY(User,loginTime,login_time);
	JSON_ACCESS_STRING_WITH_KEY(User,phoneNumber,phone_number);
	JSON_ACCESS_STRING_WITH_KEY(User,photoUrl,photo_url);

private:
	API_ACCESS_COMPOUND(User,var::String,user_id);

};

}

#endif // SERVICE_API_SERVICE_USER_HPP
