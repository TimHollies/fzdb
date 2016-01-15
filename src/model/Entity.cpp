#include "./Entity.h"

const Entity::EHandle_t Entity::INVALID_EHANDLE = 0;

Entity::Entity(unsigned int type) : handle_(Entity::INVALID_EHANDLE), _type(type)
{
}

Entity::Entity(unsigned int type, EHandle_t handle) : handle_(handle), _type(type)
{
}

Entity::~Entity()
{
    deleteAllProperties();
}

bool Entity::isNull() const
{
	return handle_ == INVALID_EHANDLE;
}

int Entity::propertyCount() const
{
	return _propertyTable.size();
}

void Entity::removeProperty(const unsigned int &key)
{
    try
    {
        delete _propertyTable.at(key);
        _propertyTable.erase(key);
    }
    catch (const std::out_of_range &ex)
    {

    }
}

bool Entity::hasProperty(const unsigned int& key)
{
    return _propertyTable.find(key) != _propertyTable.cend();
}

void Entity::clearProperties()
{
    deleteAllProperties();
    _propertyTable.clear();
}

Entity::EHandle_t Entity::getHandle() const
{
	return handle_;
}

void Entity::deleteAllProperties()
{
    // Make a list of pointers - I'm not sure whether deleting things as
    for ( auto it = _propertyTable.begin(); it != _propertyTable.end(); ++it )
    {
        delete it->second;
    }
}
