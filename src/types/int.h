#ifndef FUZZY_MODEL_TYPES_INT
#define FUZZY_MODEL_TYPES_INT

#include <string>

#include "./ordered_type.h"
#include <iostream>

namespace model {
namespace types {

// Stores an integer value.
class Int : public OrderedType {
 private:
  friend class TypeSerialiser;
  int32_t _value;
  MemberSerialiser _memberSerialiser;

  void initMemberSerialiser() {
    _memberSerialiser.addPrimitive(&_value, sizeof(_value));

    _memberSerialiser.setInitialised();
  }

 public:
  Int() : OrderedType(), _value(0) {
  }

  Int(int32_t value, unsigned int author, unsigned char confidence = 100, const std::string &comment = std::string()) :
    OrderedType(), _value(value) {
  }

  Int(const std::string &value, unsigned int author, unsigned char confidence = 100, const std::string &comment = std::string()) :
    Int(std::atoi(value.c_str()), author, confidence, comment) {
    // Already initialised
  }

  virtual bool valuesEqualOnly(const Base *other) const {
    const Int* i = dynamic_cast<const Int*>(other);
    assert(i);

    // If the subtypes are not the same then the base implementation
    // will return false and the statement will short-circuit, meaning
    // we should avoid dereferencing the pointer if it's null!
    return Base::valuesEqualOnly(other)
           && _value == i->_value;
  }

  virtual ~Int() {}

  int32_t value() const {
    return _value;
  }

  virtual SubType subtype() const {
    return SubType::Int32;
  }

  virtual std::shared_ptr<Base> Clone() override {
    auto cloned = std::make_shared<Int>();
    cloned->_value = _value;
    copyValues(cloned);
    return cloned;
  }

  virtual std::string logString(const Database* db = NULL) const override {
    return std::string("Int(") + std::to_string(_value) + std::string(", ")
           + std::to_string(confidence()) + std::string(")");
  }

  // Inherited via Base
  virtual unsigned char Equals(const std::string &val) const override {
    return _value == std::stoi(val) ? 100 : 0;
  }

  virtual std::string toString() const override {
    return std::to_string(_value);
  }

  virtual bool memberwiseEqual(const Base* other) const {
    const Int* cOther = dynamic_cast<const Int*>(other);
    return Base::memberwiseEqual(other) && cOther &&
           _value == cOther->_value;
  }

  bool greaterThan(const std::string rhs) override {
    return _value > std::stoi(rhs);
  }

  bool lessThan(const std::string rhs) override {
    return _value < std::stoi(rhs);
  }

 protected:
  virtual std::size_t serialiseSubclass(Serialiser &serialiser) {
    if (!_memberSerialiser.initialised())initMemberSerialiser();
    return Base::serialiseSubclass(serialiser) + _memberSerialiser.serialiseAll(serialiser);
  }

  Int(const char* &serialisedData, std::size_t length) : OrderedType(serialisedData, length) {
    initMemberSerialiser();
    serialisedData += _memberSerialiser.unserialiseAll(serialisedData, length);
  }
};
}
}


#endif // !FUZZY_MODEL_TYPES_INT
