#include "../Job.h"

// Demotes an admin back to an editor. Requires admin privileges.
class DemoteAdminJob : public Job {
    public:
        DemoteAdminJob(std::shared_ptr<ISession> session, const std::string &username);
        
        virtual bool constOperation() const override { return false; }
        virtual QueryResult executeNonConst() override;
        
    private:
        std::string _username;
};
