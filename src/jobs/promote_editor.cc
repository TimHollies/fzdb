#include "./promote_editor.h"
#include "../user/user_attributes.h"
#include "../user/hashing.h"
#include "../user/user_operation.h"
#include <stdexcept>

using namespace jobs;

PromoteEditor::PromoteEditor(std::shared_ptr<ISession> session, const std::string &username):
  Job(session, PermType::UserOp) {
  _username = username;
}

QueryResult PromoteEditor::executeNonConst() {
  try {
    Permission::UserGroup group = _database->users().getUserGroup(_username); //Throws user not exist exception
    if (group != Permission::UserGroup::EDITOR) {
      return QueryResult::generateError(QueryResult::ErrorCode::UserDataError, "User to promote is not an editor.");
    }
  } catch (const std::exception &ex) {
    return QueryResult::generateError(QueryResult::ErrorCode::UserDataError, ex.what());
  }

  _database->users().changeUserGroup(_username, Permission::UserGroup::ADMIN);

  QueryResult result;
  result.setResultDataText(std::string("User ") + _username + std::string(" promoted to admin."));
  return result;
}
