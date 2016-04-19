#include "./unknown.h"

UnknownJob::UnknownJob(std::shared_ptr<ISession> session, std::string name) : Job(session, PermType::ViewDB)
{
  _name = name;
}

QueryResult UnknownJob::executeConst() const
{
    return QueryResult::generateError(QueryResult::ErrorCode::NotImplemented, std::string("Unknown command: ") + _name + std::string("."));
}