#pragma once
#include <cstdint>

class Location;

enum class ObjectType : uint8_t
{
    None,
    Artefact,
    Asteroid,
    ResearchUnlock,
    MAX_OBJECT_TYPE
};

class Object
{
public:
    int id;                // unique identifier for the object
    ObjectType type;       // the type of the object
    Location *location;    // the location where the object is found
    int quantity;          // the quantity of the object
    int resource_id;       // the resource type of the object
    int research_topic_id; // the research unlock associated with the object

    const char *description() const;
};