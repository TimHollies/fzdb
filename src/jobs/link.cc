#include "./link.h"
#include "../singletons.h"
#include "../model/entity_manager.h"

#include "../exceptions.h"

using namespace jobs;

Link::Link(std::shared_ptr<ISession> session, Entity::EHandle_t entity1, Entity::EHandle_t entity2)
  : Job(session, PermType::ModifyDB), _entity1(entity1), _entity2(entity2) {
}

QueryResult Link::executeNonConst() {

  try {
    _database->entityManager().linkEntities(_entity1, _entity2);
  } catch (std::runtime_error ex) {
    return QueryResult::generateError(QueryResult::ErrorCode::GenericError, ex.what());
  }

  QueryResult result;
  result.setResultDataText(std::string("Entities ") + std::to_string(_entity1) + std::string(" and ") + std::to_string(_entity2) + std::string(" linked successfully."));
  return result;
}

Unlink::Unlink(std::shared_ptr<ISession> session, Entity::EHandle_t entity1, Entity::EHandle_t entity2)
  : Link(session, entity1, entity2) {
}

QueryResult Unlink::executeNonConst() {
  try {
    _database->entityManager().unlinkEntities(_entity1, _entity2);
  } catch (std::runtime_error ex) {
    return QueryResult::generateError(QueryResult::ErrorCode::GenericError, ex.what());
  }
  QueryResult result;
  result.setResultDataText(std::string("Entities ") + std::to_string(_entity1) + std::string(" and ") + std::to_string(_entity2) + std::string(" unlinked successfully."));
  return result;
}

Merge::Merge(std::shared_ptr<ISession> session, Entity::EHandle_t entity1, Entity::EHandle_t entity2)
  : Link(session, entity1, entity2) {
}

QueryResult Merge::executeNonConst() {
  try {
    _database->entityManager().mergeEntities(_entity1, _entity2);
  } catch (std::runtime_error ex) {
    return QueryResult::generateError(QueryResult::ErrorCode::GenericError, ex.what());
  }
  QueryResult result;
  result.setResultDataText(std::string("Entities ") + std::to_string(_entity1) + std::string(" and ") + std::to_string(_entity2) + std::string(" merged successfully."));
  return result;
}
