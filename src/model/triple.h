#ifndef FUZZY_MODEL_TRIPLE
#define FUZZY_MODEL_TRIPLE

#include <string>
#include <vector>

namespace model {

enum class TripleComponentType {
  EntityRef,
  String,
  Integer,
  Property,
  Variable
};

struct Subject {
 public:

  enum class Type {
    ENTITYREF,
    VARIABLE
  };

  Type type;
  std::string value;

  Subject() { }

  Subject(Type t, std::string val) : value(val) {
    type = t;
  }
};

struct Predicate {
 public:

  enum class Type {
    PROPERTY,
    VARIABLE
  };

  Type type;
  std::string value;

  Predicate() { }

  Predicate(Type t, std::string val) : value(val) {
    type = t;
  }
};

struct Object {
 public:

  enum class Type {
    VARIABLE = 0b0001,
    ENTITYREF = 0b1000,
    STRING = 0b1001,
    INT = 0b1010,
    DATE = 0b1011
  };

  const static int VALUE_MASK = 0b1000;

  unsigned char certainty;
  bool hasCertainty;

  static inline bool IsValue(Type t) {
    return ((int)t & model::Object::VALUE_MASK) != 0;
  }

  Type type;
  std::string value;

  Object() : hasCertainty(false) { }

  Object(Type t, std::string val) : value(val), hasCertainty(false) {
    type = t;
  }

  Object(Type t, std::string val, unsigned char cert) : value(val), certainty(cert), hasCertainty(true) {
    type = t;
  }
};

// Class representing a triple.
// Holds subject/predicate/object and entropy
// value corresponding to how many of these are variables.
struct Triple {
 public:
  Subject subject;
  Predicate predicate;
  Object object;

  std::string meta_variable;

  std::vector<std::string> variables();

  Triple(Subject sub, Predicate pred, Object obj) : subject(sub), predicate(pred), object(obj) {}
  Triple(Subject sub, Predicate pred, Object obj, std::string meta_var) : subject(sub), predicate(pred), object(obj), meta_variable(meta_var) {}

  unsigned char Entropy() {
    unsigned char entropy = 0;
    if (subject.type == Subject::Type::VARIABLE) entropy++;
    if (predicate.type == Predicate::Type::VARIABLE) entropy++;
    if (object.type == Object::Type::VARIABLE) entropy++;
    return entropy;
  }
};

}

#endif
