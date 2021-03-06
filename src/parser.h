#ifndef FUZZY_PARSER
#define FUZZY_PARSER

#include <iterator>
#include <vector>
#include <set>
#include <string>
#include <map>

#include "./model/triple.h"
#include "./filters/ifilter.h"
#include "./model/query_settings.h"

// Values that don't collide with masks range from 0x0 to 0xf.
#define TOKEN_SPLITTER_MASK 0b010000    // This token is a splitter.
#define TOKEN_KEYWORD_MASK 0b100000     // This token is a keyword.

// Specifiers for tokens.
enum class ParsedTokenType {
  VARIABLE          = 0x0,       // Variable: $abc
  STRING            = 0x1,       // String: "dog"
  INT               = 0x2,       // Integer: 146
  NOTIMPLEMENTED    = 0x3,       // Placeholder. Something that's not implemented yet.
  PROPERTY          = 0x4,       // Property: <forename>
  ENTITYREF         = 0x5,       // Entity handle: entity:1
  CONFIDENCE_RATING = 0x8,    // Confidence for a value: [80]
  DATE = 0xb,

  //structural
  OPEN_CURLBRACE  = 0x6,      // {
  CLOSE_CURLBRACE = 0x7,      // }
  FILTER          = 0x9,               // FILTER: For filtering values.
  NEWENTITY       = 0xa,            // NEWENTITY: For creating new entities

  //keywords
  KEYWORD_SELECT   = TOKEN_KEYWORD_MASK | 0x00,
  KEYWORD_WHERE    = TOKEN_KEYWORD_MASK | 0x01,
  KEYWORD_INSERT   = TOKEN_KEYWORD_MASK | 0x02,
  KEYWORD_DELETE   = TOKEN_KEYWORD_MASK | 0x03,
  KEYWORD_PING     = TOKEN_KEYWORD_MASK | 0x04,
  KEYWORD_ECHO     = TOKEN_KEYWORD_MASK | 0x05,
  KEYWORD_META     = TOKEN_KEYWORD_MASK | 0x06,
  KEYWORD_DATA     = TOKEN_KEYWORD_MASK | 0x07,
  KEYWORD_DEBUG    = TOKEN_KEYWORD_MASK | 0x08,
  KEYWORD_LOAD     = TOKEN_KEYWORD_MASK | 0x09,
  KEYWORD_SAVE     = TOKEN_KEYWORD_MASK | 0x0A,
  KEYWORD_LINK     = TOKEN_KEYWORD_MASK | 0x0B,
  KEYWORD_UNLINK   = TOKEN_KEYWORD_MASK | 0x0C,
  KEYWORD_FINAL    = TOKEN_KEYWORD_MASK | 0x0D,
  KEYWORD_FLUSH    = TOKEN_KEYWORD_MASK | 0x0E,
  KEYWORD_CANON    = TOKEN_KEYWORD_MASK | 0x0F,
  KEYWORD_USER     = TOKEN_KEYWORD_MASK | 0x10,
  KEYWORD_ADD      = TOKEN_KEYWORD_MASK | 0x11,
  KEYWORD_PASSWORD = TOKEN_KEYWORD_MASK | 0x12,
  KEYWORD_PROMOTE  = TOKEN_KEYWORD_MASK | 0x13,
  KEYWORD_DEMOTE   = TOKEN_KEYWORD_MASK | 0x14,
  KEYWORD_LOGIN    = TOKEN_KEYWORD_MASK | 0x15,
  KEYWORD_LOGOUT   = TOKEN_KEYWORD_MASK | 0x16,
  KEYWORD_LEVEL    = TOKEN_KEYWORD_MASK | 0x17,
  KEYWORD_RESET    = TOKEN_KEYWORD_MASK | 0x18,


  SPLITTER1 = TOKEN_SPLITTER_MASK | 0x0,
  SPLITTER2 = TOKEN_SPLITTER_MASK | 0x1,
  SPLITTER3 = TOKEN_SPLITTER_MASK | 0x2,
};

// Represents a specific token.
struct TokenInfo {
 public:
  const ParsedTokenType type;
  const unsigned int lineNumber;
  const unsigned int charPosition;
  const std::string data0;

  TokenInfo(ParsedTokenType ptt, unsigned int lineNo, unsigned int charPos, std::string dat0) :
    type(ptt), lineNumber(lineNo), charPosition(charPos), data0(dat0) { }
};

// Jonathan - Reserved property strings.
// These are for things like entity types, where properties should not be allowed to use this name.
namespace ReservedProperties {
static const std::string TYPE("type");
static const std::string ORDER_SUPERSET_OF("supersetOf");
static const std::string ORDER_SUBSET_OF("subsetOf");
}

//TODO: this whole file could do with tidying up

using StringMap = std::map<std::string, std::string>;
using TokenItem = std::pair<TokenInfo, std::string>;
using TokenList = std::vector<TokenItem>;
using TokenIterator = TokenList::iterator;

//General exception class for parsing
//TODO: extend to show line and column
class ParseException : public std::exception {

 private:

  std::string _message;

 public:

  ParseException(std::string msg) {
    _message = msg;
  }

  virtual const char* what() const throw() {
    return _message.c_str();
  }
};

//Represents a block of triples
struct TriplesBlock {
 public:
  std::vector<model::Triple> triples;
  std::vector<IFilter*> filters;
  std::set<std::pair<std::string, std::string>> newEntities;
  std::set<std::string> variables;
  std::set<std::string> metaVariables;

  TriplesBlock(std::vector<model::Triple> trip) {
    triples = trip;
  }

  TriplesBlock() {}

  void Add(const model::Triple&& triple) {
    triples.emplace_back(triple);
  }

  void Add(IFilter* filter) {
    filters.push_back(filter);
  }

  void Add(std::string newEntity, std::string type) {
    newEntities.insert(std::make_pair(newEntity, type));
  }

  const bool HasNewEntities() {
    return newEntities.size() > 0;
  }

  void Sort() {
    std::sort(triples.begin(), triples.end(), [this](model::Triple t1, model::Triple t2) {
      if (t1.subject.type == model::Subject::Type::VARIABLE && metaVariables.find(t1.subject.value) != metaVariables.cend()) {
        return false;
      }
      if (t2.subject.type == model::Subject::Type::VARIABLE && metaVariables.find(t2.subject.value) != metaVariables.cend()) {
        return true;
      }
      return t1.Entropy() < t2.Entropy();
    });
  }
};

//Types of query
//ECHO is called DEBUGECHO to avoid a namespace collision on linux
enum class QueryType {
  SELECT,
  INSERT,
  DELETECMD,
  PING,
  DEBUGECHO,  // ECHO
  DEBUGOTHER, // DEBUG
  LOAD,
  SAVE,
  LINK,
  UNLINK,
  MERGE,
  FLUSH,
  USER_ADD,
  USER_DELETE,
  USER_PROMOTE,
  USER_DEMOTE,
  USER_PASSWORD,
  USER_LOGIN,
  USER_LOGOUT,
  USER_LEVEL,
  USER_RESET
};

//Aggregate query object - this should contain all info required to do a query
struct Query {
 public:

  QueryType type;
  StringMap sources;
  TriplesBlock conditions;
  TriplesBlock whereClause;
  std::string data0;
  std::string data1;
  std::vector<std::string> selectLine;
  std::vector<long long int> entities;
  QuerySettings settings;

  Query(QueryType type_, StringMap s, TriplesBlock cond, TriplesBlock wh, std::string dat0, std::string dat1, std::vector<std::string> selectline, std::vector<long long int> ents, QuerySettings settings_) {
    type        = type_;
    sources     = s;
    conditions  = cond;
    whereClause = wh;
    data0       = dat0;
    data1       = dat1;
    selectLine  = selectline;
    entities    = ents;
    settings    = settings_;
  }
};

// Actual parser class. Takes a string and returns all the data required to execute its corresponding query.
// To do so, call Tokenize() on a string and then ParseAll() on the returned tokens.
class FSparqlParser {
 private:
  static std::string parseConfidenceRating(TokenIterator&& iter, TokenIterator end);
  static IFilter* parseFilter(const TokenInfo&& filterInfo, const std::string&& filterDescription);
 public:
  static TokenItem identifyToken(std::string str, unsigned int line, unsigned int chr);
  static TokenList Tokenize(std::string str);
  static TriplesBlock ParseTriples(TokenIterator&& iter, TokenIterator end);
  static TriplesBlock ParseInsert(TokenIterator&& iter, TokenIterator end);
  static StringMap ParseSources(TokenIterator&& iter, TokenIterator end);
  static std::vector<std::string> ParseSelectLine(TokenIterator&& iter, TokenIterator end);
  static Query ParseAll(TokenList tokens);
};


#endif
