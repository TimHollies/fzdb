#include "./entity_manager.h"
#include <spdlog/spdlog.h>
#include <cassert>
#include <set>
#include <sstream>
#include <iostream>
#include "../file_system.h"
#include "./graph_serialiser.h"
#include "../util.h"
#include <algorithm>
#include <boost/algorithm/string.hpp>

#include "../parser.h"
#include "../util.h"
#include "../job_queue.h"

#include "../types/base.h"
#include "../types/value_ref.h"
#include "../types/string.h"
#include "../types/source_ref.h"
#include "../types/entity_ref.h"
#include "../types/int.h"
#include "../types/date.h"
#include "../types/property.h"
#include "../singletons.h"

//#define ENFORCE_ENTITIES_HAVE_TYPES

static const unsigned int ENTITY_TYPE_GENERIC = 0;

EntityManager::EntityManager(Database *db) : _database(db) {
}

EntityManager::~EntityManager() { }

std::shared_ptr<Entity> EntityManager::createEntity(const std::string &type) {
  unsigned int typeID;
  if (_entityTypeNames.has(type)) {
    typeID = _entityTypeNames.get(type);
  } else {
    typeID = _entityTypeNames.add(type);
  }

  _lastHandle++;
  assert(_lastHandle != Entity::INVALID_EHANDLE);

  std::shared_ptr<Entity> e = std::make_shared<Entity>(typeID, _lastHandle);

  // Jonathan: NEW: Set the manager pointer within the entity.
  // This means entities can now query the manager tables for stuff,
  // without going through Singletons (which would be bad because then the
  // job const guarantees would be broken).
  e->_manager = this;

  _entities.insert(std::pair<Entity::EHandle_t, std::shared_ptr<Entity> >(_lastHandle, e));

  return e;
}

// Basic Graph Pattern
// When presented with a sanatised list of triples finds values for variables that satisfy that condition
VariableSet EntityManager::BGP(TriplesBlock triplesBlock, const QuerySettings&& settings) const {
  VariableSet result(triplesBlock.variables);

  //sort by entropy
  triplesBlock.Sort();
  auto conditions = triplesBlock.triples;

  //iterate over conditions
  for (auto conditionsIter = conditions.cbegin(); conditionsIter != conditions.end(); conditionsIter++) {

    //There are 7 valid combinations

    if (conditionsIter->subject.type == model::Subject::Type::VARIABLE) {
      if (conditionsIter->predicate.type == model::Predicate::Type::PROPERTY) {

        // quick out if the property does not exist in the database
        if (!_propertyNames.has(conditionsIter->predicate.value)) continue;

        if (model::Object::IsValue(conditionsIter->object.type)) {
          //option 1 - $a <prop> value
          this->Scan1(std::move(result), result.indexOf(conditionsIter->subject.value), std::move(conditionsIter->predicate), std::move(conditionsIter->object), std::move(conditionsIter->meta_variable), std::move(settings));
        } else {
          //option 2 - $a <prop> $b
          this->Scan2(std::move(result), result.indexOf(conditionsIter->subject.value), std::move(conditionsIter->predicate),
                      result.indexOf(conditionsIter->object.value), std::move(conditionsIter->meta_variable), std::move(settings));
        }
      } else {
        if (model::Object::IsValue(conditionsIter->object.type)) {
          //option3 - $a $b value
          this->Scan3(std::move(result), result.indexOf(conditionsIter->subject.value),
                      result.indexOf(conditionsIter->predicate.value), std::move(conditionsIter->object), std::move(conditionsIter->meta_variable), std::move(settings));
        } else {
          //option 4 - $a $b $c
          this->Scan4(std::move(result), result.indexOf(conditionsIter->subject.value),
                      result.indexOf(conditionsIter->predicate.value), result.indexOf(conditionsIter->object.value), std::move(conditionsIter->meta_variable), std::move(settings));
        }
      }
    } else {
      if (conditionsIter->predicate.type == model::Predicate::Type::PROPERTY) {

        // quick out if the property does not exist in the database
        if (!_propertyNames.has(conditionsIter->predicate.value)) continue;

        if (model::Object::IsValue(conditionsIter->object.type)) {
          //meanlingless unless in a meta block!
          this->ScanEPR(std::move(result), std::move(conditionsIter->subject),
                        std::move(conditionsIter->predicate), std::move(conditionsIter->object), std::move(conditionsIter->meta_variable), std::move(settings));
        } else {
          //option 5 - entity <prop> $c
          this->Scan5(std::move(result), std::move(conditionsIter->subject), std::move(conditionsIter->predicate),
                      result.indexOf(conditionsIter->object.value), std::move(conditionsIter->meta_variable), std::move(settings));
        }
      } else {
        if (model::Object::IsValue(conditionsIter->object.type)) {
          //option 7 - entity $b value
          this->Scan6(std::move(result), std::move(conditionsIter->subject),
                      result.indexOf(conditionsIter->predicate.value), std::move(conditionsIter->object), std::move(conditionsIter->meta_variable), std::move(settings));
        } else {
          //option 8 - entity $b $c
          this->Scan7(std::move(result), std::move(conditionsIter->subject),
                      result.indexOf(conditionsIter->predicate.value), result.indexOf(conditionsIter->object.value), std::move(conditionsIter->meta_variable), std::move(settings));
        }
      }
    }
  }

  return result;
}

//Inserts new data into the data store
std::map<std::string, Entity::EHandle_t> EntityManager::Insert(TriplesBlock&& block, TriplesBlock&& whereBlock, const QuerySettings&& settings) {
  try {

    VariableSet whereVars = BGP(whereBlock, std::move(settings));

    for (auto newVar : whereBlock.newEntities) {
      whereVars.extend(newVar.first);
    }

    std::map<std::string, Entity::EHandle_t> createdEntities;
    for (auto newVar : whereBlock.newEntities) {
      auto newEntity = createEntity(newVar.second);
      createdEntities.insert(std::make_pair(newVar.first, newEntity->getHandle()));
      auto entityPointer = std::make_shared<model::types::EntityRef>(newEntity->getHandle());
      entityPointer->Init(100);
      whereVars.add(whereVars.indexOf(newVar.first), entityPointer, 0, 0, VariableType::EntityRef, "");
    }

    //sort by entropy
    block.Sort();

    auto iter = block.triples.cbegin();
    auto end = block.triples.cend();
    VariableSet variableSet(block.metaVariables);

    // Keep track of any entity we've modified.
    // We use this later to make sure all entities have a type.
    std::set<const Entity *> modifiedEntities;

    // Iterate over each triple.
    for (; iter != end; iter++) {
      // I know virtually everyone else I've spoken to hates putting beginning braces
      // on new lines, but if there was ever a case for that it would be here.
      // It's so much more readable now!

      auto triple = *iter;

      unsigned char confidence = triple.object.hasCertainty ? triple.object.certainty : 100;
      std::vector<std::shared_ptr<model::types::Base>> newRecords;
      //std::vector<model::types::SubType> newRecordTypes;

      unsigned int authorID;

      switch (triple.object.type) {
      case model::Object::Type::STRING:
        newRecords.push_back(std::make_shared<model::types::String>(triple.object.value, authorID, confidence));
        break;
      case model::Object::Type::ENTITYREF:
        // add sourceref?
		  if (triple.predicate.value == "fuz:source") {
			  newRecords.push_back(std::make_shared<model::types::SourceRef>(triple.object.value));
		  }
		  else {
			  newRecords.push_back(std::make_shared<model::types::EntityRef>(triple.object.value));
		  }        
        break;
      case model::Object::Type::INT:
        newRecords.push_back(std::make_shared<model::types::Int>(triple.object.value, authorID, confidence));
        break;
      case model::Object::Type::DATE:
        newRecords.push_back(std::make_shared<model::types::Date>(model::types::Date::parseDateString(triple.object.value), model::types::Date::Ordering::EqualTo));
        break;
      case model::Object::Type::VARIABLE: {
        auto varId = whereVars.indexOf(triple.object.value);
        for (auto whereVarIter = whereVars.begin(); whereVarIter != whereVars.end(); whereVarIter++) {
          if ((*whereVarIter)[varId].empty()) continue;
          newRecords.push_back((*whereVarIter)[varId].dataPointer()->Clone());
        }
        break;
      }
      }

      for (auto record : newRecords) {
        record->Init(triple.object.hasCertainty ? triple.object.certainty : 100);
      }

      // Get the ID of the property with the given name.
      unsigned int propertyId;
      if (!_propertyNames.has(triple.predicate.value)) {
        propertyId = _propertyNames.add(triple.predicate.value);
        _propertyTypes.insert(std::make_pair(propertyId, newRecords[0]->subtype()));
      } else {
        propertyId = _propertyNames.get(triple.predicate.value);
      }

      // Switch depending on what the subject of the query is.
      switch (triple.subject.type) {
      // EntityRef means we're doing something to an entity in the database
      // that's been explicitly specified by ID.
      case model::Subject::Type::ENTITYREF: {
        // Get the ID.
        auto entity_id = std::stoll(triple.subject.value);

        // Error out if the entity doesn't exist.
        if (_entities.find(entity_id) == _entities.end()) {
          //_entities[entity_id] = std::make_shared<Entity>(ENTITY_TYPE_GENERIC, entity_id);
          throw std::runtime_error("No entity with id " + std::to_string(entity_id) + " was found");
        }

        // Get a pointer to the entity.
        std::shared_ptr<Entity> currentEntity = _entities[entity_id];

        // Keep track that we modified it.
        modifiedEntities.insert(currentEntity.get());

        //note that the OrderingId attribute of a record is assigned as part of insertion
        // For each value we need to insert:
        for (auto newRecord : newRecords) {
          // Add it to the property (cloning).
          currentEntity->insertProperty(propertyId, newRecord->Clone());

          if (triple.meta_variable != "") {
            variableSet.add(variableSet.indexOf(triple.meta_variable),
                            std::make_shared<model::types::ValueRef>(entity_id, propertyId, newRecord->OrderingId()), propertyId, entity_id,
                            model::types::SubType::ValueReference, "");
          }
        }
        break;
      } // END: case model::Subject::Type::ENTITYREF

      // Variable means that it's some entity/entities referred to elsewhere in the query.
      case model::Subject::Type::VARIABLE: {
        if (block.metaVariables.find(triple.subject.value) == block.metaVariables.end()) {
          if (whereVars.contains(triple.subject.value)) {
            if ((whereVars.typeOf(triple.subject.value) & static_cast<unsigned char>(model::types::TypePosition::SUBJECT)) == 0) {
              throw new std::runtime_error("Variables bound in the WHERE clause of an insert statement used in the body of that statement MUST resolve to entity values");
            }

            auto varId = whereVars.indexOf(triple.subject.value);

            for (auto whereVarIter = whereVars.begin(); whereVarIter != whereVars.end(); whereVarIter++) {
              if ((*whereVarIter)[varId].empty()) continue;

              Entity::EHandle_t entityHandle = std::dynamic_pointer_cast<model::types::EntityRef, model::types::Base>((*whereVarIter)[varId].dataPointer())->value();

              std::shared_ptr<Entity> currentEntity = _entities[entityHandle];

              modifiedEntities.insert(currentEntity.get());

              for (auto newRecord : newRecords) {

                auto clonedRecord = newRecord->Clone();
                currentEntity->insertProperty(propertyId, clonedRecord);

                if (triple.meta_variable != "") {
                  variableSet.add(variableSet.indexOf(triple.meta_variable),
                                  std::make_shared<model::types::ValueRef>(entityHandle, propertyId, clonedRecord->OrderingId()), propertyId, entityHandle,
                                  model::types::SubType::ValueReference, "");
                }
              }
            }
          }
        } else {
          auto values = variableSet.getData(variableSet.indexOf(triple.subject.value));

          for (auto val : values) {
			  //TODO: is this right?
            if (val.empty()) continue;
            std::shared_ptr<model::types::ValueRef> valueRef = std::dynamic_pointer_cast<model::types::ValueRef, model::types::Base>(val.dataPointer());
            auto record = dereference(valueRef->entity(), valueRef->prop(), valueRef->value());
            assert(record->_manager == this);
            for (auto newRecord : newRecords) {
				if (triple.predicate.value != "fuz:source") {
					record->insertProperty(propertyId, newRecord->Clone());
				} else {
					record->insertProperty(propertyId, std::make_shared<model::types::SourceRef>(newRecord->toString()));
				}
            }
          }
        }
        break;
      }
      }
    }

    saveToFile(_database->dataFilePath());
    return createdEntities;

  } catch (std::exception& e) {
    loadFromFile(_database->dataFilePath());
    throw; // propagate the error
  }


}

// For each variable described in triple block delete it
std::tuple<int, int, int> EntityManager::Delete(std::vector<std::string> selectLine, TriplesBlock&& whereBlock, const QuerySettings&& settings) {
  try {
    using VariableType = model::types::SubType;
    //Records number of deletion in subject, property, and object.
    std::tuple<int, int, int> result = std::make_tuple(0, 0, 0);
    //Get VariableSet from BGP
    VariableSet vs = BGP(whereBlock, std::move(settings));

    std::set<std::string> variables = vs.getVariables();

    std::set<std::string>::iterator varsIter = variables.begin();
    std::set<std::string>::iterator varsEnd =  variables.end();

    std::set<std::string> selectLineVarSet(selectLine.begin(), selectLine.end());

    const bool usingSelectLine = !selectLine.empty();

    if (usingSelectLine) { 
      varsIter=selectLineVarSet.begin();
      varsEnd=selectLineVarSet.end();
    }

    std::vector<VariableSetRow>::iterator rowIter;

    for (; varsIter != varsEnd; ++varsIter) {
      std::string var = *varsIter;
      std::vector<VariableSetRow>::iterator rowIter;

      if (vs.contains(var) && vs.used(var)) {
        int variableSetId = (int)vs.indexOf(var);
        std::vector<VariableSetValue> vals;
        if(usingSelectLine) {
          vals=vs.getData(variableSetId);
          auto intermediate = DeleteFromVariableSetValue(vals);
          std::get<0>(result) = std::get<0>(intermediate) + std::get<0>(result);
          std::get<1>(result) = std::get<1>(intermediate) + std::get<1>(result);
          std::get<2>(result) = std::get<2>(intermediate) + std::get<2>(result);
        } else {
          std::vector<VariableSetRow> column = vs.extractRowsWith(variableSetId);
          std::vector<VariableSetRow>::iterator rowIter;
          for (rowIter = vs.begin(); rowIter != vs.end(); rowIter++) {
            VariableSetRow row = *rowIter;
            std::vector<VariableSetValue> vals= row.values();
            auto intermediate = DeleteFromVariableSetValue(vals);
            std::get<0>(result) = std::get<0>(intermediate) + std::get<0>(result);
            std::get<1>(result) = std::get<1>(intermediate) + std::get<1>(result);
            std::get<2>(result) = std::get<2>(intermediate) + std::get<2>(result);
          }
        }
      } //END if(vs.contains(var) && vs.used(var))
    }//End variable for loop

    saveToFile(_database->dataFilePath());
    return result;
  } catch(std::exception& e) {
    loadFromFile(_database->dataFilePath());
    throw;
  }
}


//Delete, given BGP returned VariableSetValue
std::tuple<int, int, int> EntityManager::DeleteFromVariableSetValue(std::vector<VariableSetValue> vals) {
  using VariableType = model::types::SubType;
  //Records number of deletion in subject, property, and object.
  std::tuple<int, int, int> result = std::make_tuple(0, 0, 0);
  for (auto valueIter = vals.begin(); valueIter != vals.end(); valueIter++) {
    VariableSetValue value = *valueIter;
    if (value.empty()) continue;
    VariableType type = value.dataPointer()->subtype();
    unsigned long long entityId = value.entity();
    unsigned long long propertyId = value.property();
    if (type == VariableType::EntityRef) {
      //Remove all properties that are link to the entity getting deleted, by constructing a query and recursively call delete.
      const std::string e_name = std::string("entity:") + std::to_string(entityId);
      const std::string p_name = "$p";
      const std::string o_name = "$o";
      TriplesBlock tb;
      tb.variables.insert(p_name);
      tb.variables.insert(o_name);
      model::Triple trip(model::Subject(model::Subject::Type::ENTITYREF, e_name),
          model::Predicate(model::Predicate::Type::VARIABLE, p_name),
          model::Object(model::Object::Type::VARIABLE, o_name));
      tb.Add(std::move(trip));

      std::vector<std::string> newSelect(tb.variables.begin(), tb.variables.end());

      QuerySettings nqs;

      auto intermediate = Delete(std::move(newSelect), std::move(tb), std::move(nqs));
      std::get<0>(result) = std::get<0>(intermediate) + std::get<0>(result);
      std::get<1>(result) = std::get<1>(intermediate) + std::get<1>(result);
      std::get<2>(result) = std::get<2>(intermediate) + std::get<2>(result);

      assert(entityId != 0);
      assert(EntityExists(entityId));
      //Check if the entity is linked.
      if (_links.find(entityId) != _links.end()) {
        //This entity has linkage, let's not delete it.
        throw std::runtime_error("This entity currently has linkage with another entity, unlink them first.");
      }
      //Erasing the entity
      spdlog::get("main")->info("Removing entityId {}", entityId);
      _entities.erase(entityId);
      std::get<0>(result) = std::get<0>(result) + 1;
    } else if (type == VariableType::PropertyReference) {
      //The item that we want to delete is property
      //The deletion will cause deleting this property on all entities
      //BGP will return all entities that have this property
      //We will first iterate through BGP VariableSet to get all entities with this property
      //Then access the std::shared_ptr<Entity>
      //Then access its property owner
      //Then delete the property.

      //XXX A few tests that was done returned propertyId as the entityId swapped
      //Proceed with caution.
      propertyId = value.entity(); //XXX See above caution.
      entityId = value.property(); //XXX See above caution.
      assert(entityId != 0);
      assert(propertyId != 0);
      assert(EntityExists(entityId));
      std::shared_ptr<Entity> e = _entities.at(entityId);
      assert(e->hasProperty(propertyId, MatchState::None));
      e->removeProperty(propertyId);
      //Remove PropertyName and propertyType
      spdlog::get("main")->info("Removing property of name {}", _propertyNames.get(propertyId));
      _propertyNames.remove(propertyId);
      _propertyTypes.erase(propertyId);
      std::get<1>(result) = std::get<1>(result) + 1;
    } else if (type == VariableType::Int32 || type == VariableType::String || type == VariableType::Date) {
      std::shared_ptr<model::types::Base> val_ptr = value.dataPointer();
      int orderingId = val_ptr->OrderingId();
      assert(entityId != 0);
      assert(propertyId != 0);
      assert(EntityExists(entityId));
      std::shared_ptr<Entity> e = _entities.at(entityId);
      assert(e->hasProperty(propertyId, MatchState::None));
      std::shared_ptr<EntityProperty> ep = e->getProperty(propertyId);
      ep->remove(orderingId);
      //If EntityProperty is now empty, delete it too.
      if (ep->isEmpty()) {
        e->removeProperty(propertyId);
      }
      std::get<2>(result) = std::get<2>(result) + 1;
    } else if (type == VariableType::ValueReference) { //To my understanding ValueReference should not appear here?
      throw std::runtime_error("Cannot delete type Valuereference.");
    } else if (type == VariableType::Undefined) {
      throw std::runtime_error("Cannot delete type undefined.");
    } else {
      //There is a new variable type that has been used in query but not implemented delete method.
      throw std::runtime_error("Delete query found variable that was used, but delete method did not implement for the specific type.");
    }

  }
  return result;
}

std::shared_ptr<model::types::Base> EntityManager::dereference(Entity::EHandle_t entity, unsigned int prop, unsigned int val) const {
  if (prop == 0) {
    throw std::runtime_error("deference error");
  }
  if (_entities.at(entity)->getProperty(prop)->count() <= val) {
    throw std::runtime_error("deference error");
  }
  return _entities.at(entity)->getProperty(prop)->baseValue(val);
}

std::vector<std::shared_ptr<Entity>> EntityManager::entityList() const {
  std::vector<std::shared_ptr<Entity>> list;

  for ( auto it = _entities.cbegin(); it != _entities.cend(); ++it ) {
    list.push_back(it->second);
  }

  return list;
}

std::shared_ptr<Entity> EntityManager::getEntity(Entity::EHandle_t entity) const {
  return _entities.at(entity);
}

// TODO: Do we need some sort of handle renumber function too?
void EntityManager::insertEntity(std::shared_ptr<Entity> ent) {
  assert(ent->getHandle() != Entity::INVALID_EHANDLE);
  assert(_entities.find(ent->getHandle()) == _entities.end());

  if ( _lastHandle < ent->getHandle() )
    _lastHandle = ent->getHandle();

  _entities.insert(std::pair<Entity::EHandle_t, std::shared_ptr<Entity>>(ent->getHandle(), ent));
}

void EntityManager::clearAll() {
  _entities.clear();
  _lastHandle = Entity::INVALID_EHANDLE;
  _entityTypeNames.clear();
  _propertyNames.clear();
  _propertyTypes.clear();
  _links.clear();

  _entityTypeNames.add("source");
  unsigned int propertyIdName = _propertyNames.add("name");
  _propertyTypes.insert(std::pair<unsigned int, model::types::SubType>(propertyIdName, model::types::SubType::String));

  auto unknownSourceEntity = createEntity("source");
  auto newRecord = std::make_shared<model::types::String>("Unknown Source", 0);
  newRecord->Init(100);
  unknownSourceEntity->insertProperty(propertyIdName, newRecord);
  unknownSourceEntity->lock();

  _propertyTypes.insert(std::make_pair(_propertyNames.add(ReservedProperties::TYPE), model::types::SubType::String));

  _propertyTypes.insert(std::make_pair(_propertyNames.add(ReservedProperties::ORDER_SUBSET_OF), model::types::SubType::EntityRef));

  _propertyTypes.insert(std::make_pair(_propertyNames.add(ReservedProperties::ORDER_SUPERSET_OF), model::types::SubType::EntityRef));

  _propertyTypes.insert(std::make_pair(_propertyNames.add("fuz:author"), model::types::SubType::UInt32));

  _propertyTypes.insert(std::make_pair(_propertyNames.add("fuz:source"), model::types::SubType::EntityRef));

  _propertyTypes.insert(std::make_pair(_propertyNames.add("fuz:created"), model::types::SubType::TimeStamp));

  _propertyTypes.insert(std::make_pair(_propertyNames.add("fuz:certainty"), model::types::SubType::UInt32));

  saveToFile(_database->dataFilePath());
}

std::size_t EntityManager::entityCount() const {
  return _entities.size();
}

std::string EntityManager::dumpContents() const {
  std::stringstream str;
  str << "Number of entities: " << _entities.size() << "\n";

  for ( auto it = _entities.cbegin(); it != _entities.cend(); ++it ) {
    const std::shared_ptr<Entity> ent = it->second;
    str << ent->logString(_database) << "\n";
    if ( ent->propertyCount() > 0 ) {
      str << "{\n";

      const std::map<unsigned int, std::shared_ptr<EntityProperty>> &propMap = ent->properties();
      for ( auto it2 = propMap.cbegin(); it2 != propMap.cend(); ++it2 ) {
        std::shared_ptr<EntityProperty> prop = it2->second;
        str << "  " << prop->logString(_database) << "\n";

        if ( prop->count() > 0 ) {
          str << "  {\n";

          for (std::size_t i = 0; i < prop->count(); i++ ) {
            BasePointer val = prop->baseValue(i);
            str << "    " << val->logString(_database) << "\n";
          }

          str << "  }\n";
        }
      }

      str << "}\n";
    }
  }

  return str.str();
}

bool EntityManager::saveToFile(const std::string &filename) {
  Serialiser serialiser;
  GraphSerialiser gSer(this);
  gSer.serialise(serialiser);

  bool success = true;
  try {
    FileSystem::writeFile(filename, serialiser);
  } catch (const std::exception &) {
    success = false;
  }
  return success;
}

bool EntityManager::loadFromFile(const std::string &filename) {
  std::size_t size = FileSystem::dataLength(filename);
  if ( size < 1 )
    return false;

  char* buffer = new char[size];

  bool success = true;
  try {
    FileSystem::readFile(filename, buffer, size);
  } catch (const std::exception &) {
    success = false;
  }

  if ( !success ) {
    delete[] buffer;
    return false;
  }

  _entities.clear();
  _lastHandle = Entity::INVALID_EHANDLE;
  _entityTypeNames.clear();
  _propertyNames.clear();
  _propertyTypes.clear();
  _links.clear();

  GraphSerialiser gSer(this);
  gSer.unserialise(buffer, size);
  delete[] buffer;

  return true;
}

unsigned int EntityManager::getTypeID(const std::string &str) {
  return _entityTypeNames.get(str);
}

unsigned int EntityManager::getTypeID(const std::string &str) const {
  return _entityTypeNames.get(str);
}

#pragma region linkingandmerging

std::set<Entity::EHandle_t> EntityManager::getLinkGraph(const Entity::EHandle_t start, std::set<Entity::EHandle_t>&& visited) const {
  if (_links.find(start) == _links.cend()) {
    //Bad
  }

  std::set<Entity::EHandle_t> results;
  std::set_difference(_links.at(start).begin(), _links.at(start).end(),
                      visited.begin(), visited.end(),
                      std::inserter(results, results.begin()));

  visited.insert(start);

  if (results.size() == 0)return visited;

  for (auto id : results) {
    this->getLinkGraph(id, std::move(visited));
  }

  return visited;
}

const std::string EntityManager::getPropertyName(unsigned int propertyId) const {
  return _propertyNames.get(propertyId);
}

void EntityManager::linkEntities(const Entity::EHandle_t entityId, const Entity::EHandle_t entityId2) {
  try {
    if (_entities.find(entityId) == _entities.end() || _entities.find(entityId2) == _entities.end()) {
      throw std::runtime_error("Attempting to link non-existant entity");
    }

    //create link sets if they do not exists
    if (_links.find(entityId) == _links.end()) _links[entityId] = std::set<Entity::EHandle_t>();
    if (_links.find(entityId2) == _links.end()) _links[entityId2] = std::set<Entity::EHandle_t>();

    //add the new link
    _links[entityId].insert(entityId2);
    _links[entityId2].insert(entityId);

    //get the full graph of linked entities
    auto allLinked = getLinkGraph(entityId, std::set<Entity::EHandle_t>());
    Entity::EHandle_t lowestId = *std::min_element(allLinked.begin(), allLinked.end());

    //set and/or reset link status values
    for (auto id : allLinked) {
      _entities[id]->linkStatus(Entity::LinkStatus::Slave);
      if (id == lowestId) _entities[id]->linkStatus(Entity::LinkStatus::Master);
    }

    saveToFile(_database->dataFilePath());
  } catch (std::exception& e) {
    loadFromFile(_database->dataFilePath());
  }
}

void EntityManager::unlinkEntities(Entity::EHandle_t entityId, Entity::EHandle_t entityId2) {
  try {
    if (_entities.find(entityId) == _entities.end() || _entities.find(entityId2) == _entities.end()) {
      throw std::runtime_error("Attempting to unlink non-existant entity");
    }

    //if they are not actually linked, return
    if (_links.find(entityId) == _links.end() || _links.find(entityId2) == _links.end() ||
        _links.find(entityId)->second.find(entityId2) == _links.find(entityId)->second.end() ||
        _links.find(entityId2)->second.find(entityId) == _links.find(entityId2)->second.end()) {
      return;
    }

    //remove the old link
    _links[entityId].erase(entityId2);
    _links[entityId2].erase(entityId);

    //get the new link graphs
    auto allLinked = getLinkGraph(entityId, std::set<Entity::EHandle_t>());
    auto allLinked2 = getLinkGraph(entityId2, std::set<Entity::EHandle_t>());

    //check if entities are now not linked at all
    if (allLinked.size() == 1) {
      _links.erase(entityId);
      _entities[entityId]->linkStatus(Entity::LinkStatus::None);
    } else {
      Entity::EHandle_t lowestId = *std::min_element(allLinked.begin(), allLinked.end());

      //set and/or reset link status values
      for (auto id : allLinked) {
        _entities[id]->linkStatus(Entity::LinkStatus::Slave);
        if (id == lowestId) _entities[id]->linkStatus(Entity::LinkStatus::Master);
      }
    }

    if (allLinked2.size() == 1) {
      _links.erase(entityId2);
      _entities[entityId2]->linkStatus(Entity::LinkStatus::None);
    } else {
      Entity::EHandle_t lowestId = *std::min_element(allLinked2.begin(), allLinked2.end());

      //set and/or reset link status values
      for (auto id : allLinked2) {
        _entities[id]->linkStatus(Entity::LinkStatus::Slave);
        if (id == lowestId) _entities[id]->linkStatus(Entity::LinkStatus::Master);
      }
    }

    saveToFile(_database->dataFilePath());
  } catch (std::exception& e) {
    loadFromFile(_database->dataFilePath());
  }
}

// Merges the entities with the given Ids. The entity with the higher Id number is deleted.
void EntityManager::mergeEntities(Entity::EHandle_t entityId, Entity::EHandle_t entityId2) {
  try {
    if (_entities.find(entityId) == _entities.end() || _entities.find(entityId2) == _entities.end()) {
      throw std::runtime_error("Attempting to link final non-existant entity");
    }

    //we will keep the entity with the lower id
    Entity::EHandle_t keep, lose;
    auto sorted = std::minmax(entityId, entityId2);
    keep = sorted.first;
    lose = sorted.second;

    //get the entities
    std::shared_ptr<Entity> keepEntity = _entities[keep],
                            loseEntity = _entities[lose];

    //check we are merging entities of the same type
    if (keepEntity->getType() != loseEntity->getType()) {
      throw std::runtime_error("Attempted to merge entities of different types");
    }

    //copy the properties of loseEntity to keepEntity
    for (auto prop : loseEntity->properties()) {
      if(prop.second->type() != EntityProperty::Type::LOCKED)
        keepEntity->insertProperty(prop.second);
    }

    //delete EntityManager's ownership of loseEntity (will be automatically deleted)
    _entities.erase(lose);

    saveToFile(_database->dataFilePath());
  } catch (std::exception& e) {
    loadFromFile(_database->dataFilePath());
  }
}

#pragma endregion linkingandmerging

unsigned int EntityManager::getPropertyName(const std::string &str, model::types::SubType type) const {
  unsigned int propId = _propertyNames.get(str);

  model::types::SubType retrievedType = _propertyTypes.at(propId);
  if (type != model::types::SubType::Undefined && retrievedType != type) {
    throw MismatchedTypeException(std::string("Mismatched types when obtaining index for property '" + str
                                  + "'. Requested type '" + model::types::getSubString(type) + "' but got '"
                                  + model::types::getSubString(retrievedType) + "'.").c_str());
  }

  return propId;
}

const NameManager& EntityManager::propertyNameMap() const {
  return _propertyNames;
}

template<typename T, typename A, typename B>
bool comparePrimitiveMaps(const T &mapA, const T &mapB) {
  if ( mapA.size() != mapB.size() )
    return false;

  for ( auto it = mapA.begin(); it != mapA.end(); ++it ) {
    const B &thisVal = it->second;

    auto otherIt = mapB.find(it->first);
    if ( otherIt == mapB.end() )
      return false;

    const B &otherVal = otherIt->second;
    if ( thisVal != otherVal )
      return false;
  }

  return true;
}

template<typename T, typename A, typename B>
void printPrimitiveMaps(const T &mapA, const T &mapB) {
  std::cout << "Map A has " << mapA.size() << " items." << std::endl;
  int i = 0;
  for ( auto it = mapA.begin(); it != mapA.end(); ++it ) {
    std::cout << "Item " << i << ": " << it->first << " -> " << it->second << std::endl;
    i++;
  }

  std::cout << "Map B has " << mapB.size() << " items." << std::endl;
  i = 0;
  for ( auto it = mapB.begin(); it != mapB.end(); ++it ) {
    std::cout << "Item " << i << ": " << it->first << " -> " << it->second << std::endl;
    i++;
  }
}

bool EntityManager::memberwiseEqual(const EntityManager &other) const {
  if ( _entities.size() != other._entities.size() )
    return false;

  for ( auto it = _entities.begin(); it != _entities.end(); ++it ) {
    const Entity* thisEnt = it->second.get();

    auto otherEntIt = other._entities.find(it->first);
    if ( otherEntIt == other._entities.end() )
      return false;

    const Entity* otherEnt = otherEntIt->second.get();
    if ( !thisEnt->memberwiseEqual(otherEnt) )
      return false;
  }

  if ( !comparePrimitiveMaps<std::map<Entity::EHandle_t, std::set<Entity::EHandle_t> >,
       Entity::EHandle_t, std::set<Entity::EHandle_t> >(_links, other._links) )
    return false;

  //We assume that if the left map is equal, so is the right.
  if (!(_entityTypeNames == other._entityTypeNames)) return false;

  if (!(_propertyNames == other._propertyNames)) return false;

  if ( !comparePrimitiveMaps<std::map<unsigned int, model::types::SubType>,
       unsigned int, model::types::SubType>(_propertyTypes, other._propertyTypes) )
    return false;

  return true;
}
