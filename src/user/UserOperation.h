#pragma once

#include <user/UserFileOperations.h>
#include <user/UserExceptions.h>
#include <user/UserAttributes.h>
#include <user/UserExceptions.h>
#include <user/Hashing.h>

class UserOperation : public UserFileOperations { 
	public : 
		static UserGroup login(std::string userName, std::string password);
        static void addUser(std::string userName, std::string password, UserGroup userGroup);
        static void removeUser(std::string userName);
        static void changeUserGroup(std::string userName, UserGroup newUserGroup);
		static UserGroup getUserGroup(std::string userName);
	private:
		typedef UserFileOperations super;
};

