#ifndef FUZZY_VARIABLESET
#define FUZZY_VARIABLESET

#include <vector>
#include <string>
#include <map>
#include <typeinfo>


class VariableSet {

public:

	enum class VariableType {
		STRING,
		INT,
		ENTITYREF
	};

	VariableSet() { }
	
	void add(std::string&& var, std::string&& value, VariableType&& type) {
		if (_data.find(var) == _data.cend()) {
			_data[var] = std::pair<std::vector<std::string>, VariableType>(std::vector<std::string>{ value }, type);
		}
		else {
			if (type != _data[var].second) {
				throw new std::runtime_error("Attempted to mix variable types!");
			}
			_data[var].first.push_back(value);
		}
	}

private:
	std::map<std::string, std::pair<std::vector<std::string>, VariableType>> _data;
};
#endif // !FUZZY_VARIABLESET
