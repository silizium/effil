#pragma once

#include "utils.h"
#include "garbage-collector.h"

#include <sol.hpp>

namespace effil {

struct EffilApiMarker{};

class SharedTable;
class Function;
class Channel;
class Thread;

// Represents an interface for lua type stored at C++ code
class StoredObject {
public:
    StoredObject(StoredObject&& other);

    explicit StoredObject(const std::string& str);
    explicit StoredObject(lua_Number num);
    explicit StoredObject(bool b);

    StoredObject(const SharedTable& obj);
    StoredObject(const Function& obj);
    StoredObject(const Channel& obj);
    StoredObject(const Thread& obj);
    StoredObject(sol::lightuserdata_value ud);
    StoredObject(sol::nil_t);
    StoredObject(EffilApiMarker);
    StoredObject();
    ~StoredObject();

    //bool operator<(const StoredObject& other) const;
    bool operator==(const StoredObject& other) const;
    const std::type_info& type();
    sol::object unpack(sol::this_state state) const;
    GCHandle gcHandle() const;
    void releaseStrongReference();
    void holdStrongReference();

    static sol::optional<LUA_INDEX_TYPE> toIndexType(const StoredObject&);

protected:
    union {
        lua_Number  number_;
        bool        bool_;
        char*       string_;
        GCHandle    handle_;
        void*       lightUData_;
    };
    StrongRef strongRef_;

    enum class StoredType {
        Number,
        Boolean,
        String,
        Nil,
        ApiMarker,
        LightUserData,
        SharedTable,
        SharedChannel,
        SharedFunction,
        SharedThread,
    } type_;

    StoredObject(const StoredObject&) = delete;
    friend struct StoredObjectHash;
};

struct StoredObjectHash {
    size_t operator ()(const StoredObject& obj) const;
};

StoredObject createStoredObject(bool);
StoredObject createStoredObject(lua_Number);
StoredObject createStoredObject(lua_Integer);
StoredObject createStoredObject(const std::string&);
StoredObject createStoredObject(const char*);
StoredObject createStoredObject(const sol::object&);
StoredObject createStoredObject(const sol::stack_object&);

/*
sol::optional<bool> storedObjectToBool(const StoredObject&);
sol::optional<double> storedObjectToDouble(const StoredObject&);
sol::optional<std::string> storedObjectToString(const StoredObject&);
*/
} // effil
