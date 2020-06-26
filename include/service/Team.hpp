#ifndef SERVICE_API_SERVICE_TEAM_HPP
#define SERVICE_API_SERVICE_TEAM_HPP

#include <CloudAPI/cloud/Document.hpp>

namespace service {

class TeamUserOptions: public cloud::DocumentOptionsAccess<TeamUserOptions> {
public:
	explicit TeamUserOptions(
			const var::String& team_id
			){
		//document id is the user id
		set_path("teams/" + team_id + "/users");
	}
};

class TeamUser: public cloud::DocumentAccess<TeamUser> {
public:
	JSON_ACCESS_BOOL(TeamUser,create);
	JSON_ACCESS_BOOL(TeamUser,delete);
	JSON_ACCESS_BOOL(TeamUser,read);
	JSON_ACCESS_BOOL(TeamUser,update);
	JSON_ACCESS_BOOL(TeamUser,write);
};

class TeamOptions: public cloud::DocumentOptionsAccess<TeamOptions> {
public:
	TeamOptions(){
		set_path("teams");
	}
};

class Team : public cloud::DocumentAccess<Team> {
public:
	Team();
	JSON_ACCESS_STRING(Team,name);
};


}

#endif // SERVICE_API_SERVICE_TEAM_HPP
