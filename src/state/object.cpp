#include "state/object.h"

const char *Object::description() const
{
    switch (type)
    {
    case ObjectType::Artefact:
        return "Artefact";
    case ObjectType::Asteroid:
        return "Asteroid";
    default:
        return "Unknown Object";
    }
}