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
class BaseHolder {
public:
    BaseHolder(BaseHolder&& other);

    explicit BaseHolder(const std::string& str);
    explicit BaseHolder(lua_Number num);
    explicit BaseHolder(bool b);

    BaseHolder(const SharedTable& obj);
    BaseHolder(const Function& obj);
    BaseHolder(const Channel& obj);
    BaseHolder(const Thread& obj);
    BaseHolder(sol::lightuserdata_value ud);
    BaseHolder(sol::nil_t);
    BaseHolder(EffilApiMarker);
    BaseHolder();
    ~BaseHolder();

    bool operator<(const BaseHolder& other) const;
    const std::type_info& type();
    sol::object unpack(sol::this_state state) const;
    GCHandle gcHandle() const;
    void releaseStrongReference();
    void holdStrongReference();

    static sol::optional<LUA_INDEX_TYPE> toIndexType(const BaseHolder&);

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

    BaseHolder(const BaseHolder&) = delete;
};

typedef BaseHolder StoredObject;

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
