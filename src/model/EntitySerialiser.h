#ifndef MODEL_ENTITYSERIALISER_H
#define MODEL_ENTITYSERIALISER_H

#include "Serialiser.h"
#include <cstring>

class Entity;

class EntitySerialiser
{
public:
    EntitySerialiser(const Entity* ent);

    std::size_t serialise(Serialiser &serialiser) const;

    // TODO: This is probably unsafe without a length parameter!
    static Entity* unserialise(const char* serialData);

private:
    const Entity*   _entity;
};

#endif  // MODEL_ENTITYSERIALISER_H