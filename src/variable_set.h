#ifndef FUZZY_VARIABLESET
#define FUZZY_VARIABLESET

#include <vector>
#include <set>
#include <string>
#include <map>
#include <typeinfo>
#include <stdexcept>
#include <iterator>
#include <algorithm>

#include "./model/entity.h"
#include "./types/base.h"
#include "./util/name_manager.h"

using VariableType = model::types::SubType;

/*
  VariableSet is essentially a table of values
  Each column represents a variable in the query
  Each row represents a 'result'
  It is intended to be the data-interchange between the scan
  functions and so has been developed in a rather hacky fashion
  and really needs tidying up a bit
*/

// VariableSetValue is a simple wrapper around the values stored in the
// VariableSet
class VariableSetValue {
 private:
  // these should really be const, but setting them to const causes all kinds of strange issues
  std::shared_ptr<model::types::Base> _ptr;
  unsigned int _propertyId;
  uint64_t _entityId;

  unsigned int _metaRef;

 public:
  VariableSetValue(std::shared_ptr<model::types::Base> ptr, unsigned int propertyId, uint64_t entityId) :
    _ptr(ptr), _propertyId(propertyId), _entityId(entityId), _metaRef(0) {}

  VariableSetValue() :
    _ptr(), _propertyId(0), _entityId(0), _metaRef(0) {}

  void reset() {
    _ptr.reset();
    _propertyId = 0;
    _entityId = 0;
    _metaRef = 0;
  }

  void reset(std::shared_ptr<model::types::Base> newDataPtr, Entity::EHandle_t entityId, unsigned int propertyId) {
    _ptr = newDataPtr;
    _propertyId = propertyId;
    _entityId = entityId;
    _metaRef = 0;
  }

  std::shared_ptr<model::types::Base> dataPointer() const {
    return _ptr;
  }
  unsigned int property() const {
    return _propertyId;
  }
  uint64_t entity() const {
    return _entityId;
  }

  void metaRef(unsigned int metaRef) {
    _metaRef = metaRef;
  }

  unsigned int metaRef() const {
    return _metaRef;
  }

  bool empty() const {
    return _ptr ==  nullptr;
  }
};

class VariableSetRow {
 private:
  const std::size_t _size;
  std::vector<VariableSetValue> _values;
  int _ranking;

 public:
  explicit VariableSetRow(const std::size_t size) : _size(size), _values(_size), _ranking(0) {}
  VariableSetRow(const std::size_t size, const std::vector<VariableSetValue>&& values) : _size(size), _values(values), _ranking(0) {}

  VariableSetRow(const VariableSetRow& rhs) : _size(rhs._size), _values(rhs._values), _ranking(rhs._ranking) {}
  VariableSetRow(VariableSetRow&& rhs) : _size(rhs._size), _values(rhs._values), _ranking(rhs._ranking) {}

  VariableSetValue& operator[](const std::size_t index) {
    return _values[index];
  }

  VariableSetValue at(const std::size_t index) const {
    return _values[index];
  }

  std::size_t size() const {
    return _size;
  }

  std::vector<VariableSetValue>::iterator begin() {
    return _values.begin();
  }
  std::vector<VariableSetValue>::iterator end() {
    return _values.end();
  }

  std::vector<VariableSetValue>::const_iterator cbegin() const {
    return _values.cbegin();
  }

  std::vector<VariableSetValue>::const_iterator cend() const {
    return _values.cend();
  }

  std::vector<VariableSetValue> values() {
    return _values;
  }

  VariableSetRow& operator=(const VariableSetRow& row) {
    // moooove
    if (row._size != _size) throw std::runtime_error("Attempt to assign a row of a different size!");
    _ranking = row._ranking;
    _values = row._values;
    return *this;
  }

  int ranking() const {
    return _ranking;
  }

  void ranking(const int ranking) {
    _ranking = ranking;
  }
};

class VariableSet {
 public:
  explicit VariableSet(const std::set<std::string> &variableNames = std::set<std::string>());

  void extend(std::string variableName);

  unsigned int add(const VariableSetRow&& row);

  unsigned int add(const unsigned int var, const std::shared_ptr<model::types::Base>&& value,
                   const unsigned int propertyId, const Entity::EHandle_t entityId, const VariableType&& type, const std::string&& metaVar);

  void add(const unsigned int var, const std::shared_ptr<model::types::Base>&& value,
           const unsigned int propertyId, const Entity::EHandle_t entityId, const VariableType&& type, const std::string&& metaVar, unsigned int row);

  std::vector<unsigned int> find(const unsigned int varId, const std::string value);

  // std::vector<VariableSetRow>* getData();
  std::vector<VariableSetRow>::iterator begin() {
    return _values.begin();
  }
  std::vector<VariableSetRow>::iterator end() {
    return _values.end();
  }

  std::vector<VariableSetRow>::const_iterator cbegin() const {
    return _values.cbegin();
  }
  std::vector<VariableSetRow>::const_iterator cend() const {
    return _values.cend();
  }

  std::vector<VariableSetRow>::iterator erase(std::vector<VariableSetRow>::iterator iter);

  std::vector<VariableSetValue> getData(const unsigned int varId) const;

  const bool contains(const std::string name) const;
  const bool contains(const unsigned int id) const;


  const bool used(const std::string name) const;
  const bool used(unsigned int id) const;

  const unsigned char typeOf(const std::string name) const;
  const unsigned char typeOf(const std::size_t id) const;

  void enforcePosition(const unsigned int id, const model::types::TypePosition pos);

  const std::size_t indexOf(const std::string name) const;

  const unsigned int getMetaRef();

  void removeMetaRefs(unsigned int metaRef);

  void addToMetaRefRow(unsigned int metaRef, std::size_t position, const std::shared_ptr<model::types::Base>&& value,
                       const unsigned int propertyId, const Entity::EHandle_t entityId);

  // this doesn't seem to work
  void trimEmptyRows();

  std::vector<VariableSetRow> extractRowsWith(const unsigned int variable, const std::string value) const;

  std::vector<VariableSetRow> extractRowsWith(const unsigned int variable) const;

  void removeRowsWith(const unsigned int variable);

  std::size_t width() const {
    return _size;
  }

  std::size_t height() const {
    return _values.size();
  }

  void sort();

  std::set<std::string> getVariables() const;

  VariableSet split() const;
  void join(const VariableSet&& other);

 private:
  NameManager _nameMap;
  std::vector<std::string> _vars;
  std::vector<unsigned char> _typeMap;
  std::vector<VariableSetRow> _values;
  std::vector<bool> _variablesUsed;
  std::size_t _size;
  unsigned int _nextMetaRef;
};
#endif  // !FUZZY_VARIABLESET
