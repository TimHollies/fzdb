#pragma once

#include <map>
#include <user/Permission.h>
#include <user/UserExceptions.h>
#include <user/UserAttributes.h>

class UserFileOperations {
	protected: 
		static void addUser(UserAttributes userAttributes);
		static void removeUser(std::string userName);
		static void updateUser(std::string userName,UserAttributes newAttributes);
		static UserAttributes getUserAttributes(std::string userName);
	private:
		UserFileOperations() {};
		static void loadCacheFromFile();
		static void saveCacheToFile();
		static std::string pathToLoginFile();
		static std::map<std::string, UserAttributes> userFileCache;
};

