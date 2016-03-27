#ifndef FUZZY_JOBS_FLUSH
#define FUZZY_JOBS_FLUSH

#include "../Job.h"
#include "../model/Triple.h"
#include "../Parser.h"
#include "../QueryResult.h"

#include <string>

class Flush : public Job
{
public:

  Flush(std::shared_ptr<ISession> session);

    virtual bool constOperation() const override { return false; }
    virtual QueryResult executeNonConst() override;
};


#endif
