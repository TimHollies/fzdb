#include "./IUserAdminJobs.h"

class AddUserJob : public IUserAdminJobs {
	public:
		AddUserJob(ISession* session, std::string username, std::string password);
		virtual QueryResult execute() override;
	private:
		std::string _username;
		std::string _password;
};
