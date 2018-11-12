#include "stored-object.h"
#include "channel.h"
#include "threading.h"
#include "shared-table.h"
#include "function.h"

#include <map>
#include <vector>
#include <algorithm>

#include <cassert>

namespace effil {

BaseHolder::BaseHolder(BaseHolder&& other) {
    type_ = other.type_;
    switch (type_) {
        case StoredType::Number:
            number_ = other.number_;
            break;
        case StoredType::Boolean:
            bool_ = other.bool_;
            break;
        case StoredType::String:
            string_ = other.string_;
            other.string_ = nullptr;
            break;
        case StoredType::LightUserData:
            lightUData_ = other.lightUData_;
            other.lightUData_ = nullptr;
            break;
        case StoredType::SharedTable:
        case StoredType::SharedChannel:
        case StoredType::SharedFunction:
        case StoredType::SharedThread:
            handle_ = other.handle_;
            strongRef_ = other.strongRef_;
            other.handle_ = nullptr;
            other.strongRef_.reset();
            break;
        default:
            break;
    }
}

BaseHolder::BaseHolder(const SharedTable& obj)
    : handle_(obj.handle()), type_(StoredType::SharedTable) {
    holdStrongReference();
}

BaseHolder::BaseHolder(const Function& obj)
    : handle_(obj.handle()), type_(StoredType::SharedFunction) {
    holdStrongReference();
}

BaseHolder::BaseHolder(const Channel& obj)
    : handle_(obj.handle()), type_(StoredType::SharedChannel) {
    holdStrongReference();
}

BaseHolder::BaseHolder(const Thread& obj)
    : handle_(obj.handle()), type_(StoredType::SharedThread) {
    holdStrongReference();
}

BaseHolder::BaseHolder(const std::string& str) : type_(StoredType::String) {
    string_ = (char*)malloc(str.size() + 1);
    strcpy(string_, str.c_str());
}

BaseHolder::BaseHolder(lua_Number num) : number_(num), type_(StoredType::Number) {}

BaseHolder::BaseHolder(bool b) : bool_(b), type_(StoredType::Boolean) {}

BaseHolder::BaseHolder(sol::lightuserdata_value ud)
    : lightUData_(ud.value), type_(StoredType::LightUserData) {}

BaseHolder::BaseHolder(sol::nil_t) : bool_(false), type_(StoredType::Nil) {}

BaseHolder::BaseHolder(EffilApiMarker) : type_(StoredType::ApiMarker) {}

BaseHolder::BaseHolder() : type_(StoredType::Nil) {}

BaseHolder::~BaseHolder() {
    if (type_ == StoredType::String)
        free(string_);
}

bool BaseHolder::operator <(const BaseHolder& other) const {
    if (type_ == other.type_)
    {
        switch (type_) {
            case StoredType::Number:
                return number_ < other.number_;
            case StoredType::Boolean:
                return bool_ < other.bool_;
            case StoredType::String:
                return strcmp(other.string_, string_) > 0;
            case StoredType::LightUserData:
                return lightUData_ < other.lightUData_;
            case StoredType::SharedTable:
            case StoredType::SharedChannel:
            case StoredType::SharedFunction:
            case StoredType::SharedThread:
                return handle_ < other.handle_;
            default:
                return false;
        }
    }
    return type_ < other.type_;
}

const std::type_info& BaseHolder::type() {
    return typeid(*this);
}

sol::object BaseHolder::unpack(sol::this_state state) const {
    switch (type_) {
        case StoredType::Number:
            return sol::make_object(state, number_);
        case StoredType::Boolean:
            return sol::make_object(state, bool_);
        case StoredType::String:
            return sol::make_object(state, std::string(string_));
        case StoredType::LightUserData:
            return sol::make_object(state, lightUData_);
        case StoredType::ApiMarker:
        {
            luaopen_effil(state);
            return sol::stack::pop<sol::object>(state);
        }
        case StoredType::SharedTable:
            return sol::make_object(state, GC::instance().get<SharedTable>(handle_));
        case StoredType::SharedChannel:
            return sol::make_object(state, GC::instance().get<Channel>(handle_));
        case StoredType::SharedThread:
            return sol::make_object(state, GC::instance().get<Thread>(handle_));
        case StoredType::SharedFunction:
            return sol::make_object(state, GC::instance().get<Function>(handle_)
                                    .loadFunction(state));
        case StoredType::Nil:
        default:
            return sol::nil;
    }
}

GCHandle BaseHolder::gcHandle() const {
    if (type_ == StoredType::SharedThread ||
            type_ == StoredType::SharedChannel ||
            type_ == StoredType::SharedTable ||
            type_ == StoredType::SharedFunction) {
        return handle_;
    }
    else {
        return GCNull;
    }
}

void BaseHolder::releaseStrongReference() {
    strongRef_.reset();
}

void BaseHolder::holdStrongReference() {
    if (type_ == StoredType::SharedThread ||
            type_ == StoredType::SharedChannel ||
            type_ == StoredType::SharedTable ||
            type_ == StoredType::SharedFunction) {
        strongRef_ = GC::instance().getRef(handle_);
    }
}

namespace {

// This class is used as a storage for visited sol::tables
// TODO: try to use map or unordered map instead of linear search in vector
// TODO: Trick is - sol::object has only operator==:/
typedef std::vector<std::pair<sol::object, GCHandle>> SolTableToShared;

void dumpTable(SharedTable* target, sol::table luaTable, SolTableToShared& visited);

StoredObject makeStoredObject(sol::object luaObject, SolTableToShared& visited) {
    if (luaObject.get_type() == sol::type::table) {
        sol::table luaTable = luaObject;
        auto comparator = [&luaTable](const std::pair<sol::table, GCHandle>& element) {
            return element.first == luaTable;
        };
        auto st = std::find_if(visited.begin(), visited.end(), comparator);

        if (st == std::end(visited)) {
            SharedTable table = GC::instance().create<SharedTable>();
            visited.emplace_back(std::make_pair(luaTable, table.handle()));
            dumpTable(&table, luaTable, visited);
            return std::move(BaseHolder(table));
        } else {
            const auto tbl = GC::instance().get<SharedTable>(st->second);
            return std::move(BaseHolder(tbl));
        }
    } else {
        return createStoredObject(luaObject);
    }
}

void dumpTable(SharedTable* target, sol::table luaTable, SolTableToShared& visited) {
    for (auto& row : luaTable) {
        target->set(makeStoredObject(row.first, visited), makeStoredObject(row.second, visited));
    }
}

template <typename SolObject>
StoredObject fromSolObject(const SolObject& luaObject) {
    switch (luaObject.get_type()) {
        case sol::type::nil:
            return BaseHolder(sol::nil);
        case sol::type::boolean:
            return BaseHolder(luaObject.template as<bool>());
        case sol::type::number:
        {
#if LUA_VERSION_NUM == 503
            sol::stack::push(luaObject.lua_state(), luaObject);
            int isInterger = lua_isinteger(luaObject.lua_state(), -1);
            sol::stack::pop<sol::object>(luaObject.lua_state());
            if (isInterger)
                return std::move(BaseHolder(luaObject.as<lua_Integer>()));
            else
#endif // Lua5.3
                return BaseHolder(luaObject.template as<lua_Number>());
        }
        case sol::type::string:
            return BaseHolder(luaObject.template as<std::string>());
        case sol::type::lightuserdata:
            return BaseHolder(sol::lightuserdata_value(luaObject.template as<void*>()));
        case sol::type::userdata:
            if (luaObject.template is<SharedTable>())
                return BaseHolder(luaObject.template as<SharedTable>());
            else if (luaObject.template is<Channel>())
                return BaseHolder(luaObject.template as<Channel>());
            else if (luaObject.template is<Function>())
                return BaseHolder(luaObject.template as<Function>());
            else if (luaObject.template is<Thread>())
                return BaseHolder(luaObject.template as<Thread>());
            else if (luaObject.template is<EffilApiMarker>())
                return BaseHolder(EffilApiMarker());
            else
                throw Exception() << "Unable to store userdata object";
        case sol::type::function: {
            Function func = GC::instance().create<Function>(luaObject);
            return BaseHolder(func);
        }
        case sol::type::table: {
            sol::table luaTable = luaObject;
            // Tables pool is used to store tables.
            // Right now not defiantly clear how ownership between states works.
            SharedTable table = GC::instance().create<SharedTable>();
            SolTableToShared visited{{luaTable, table.handle()}};

            // Let's dump table and all subtables
            // SolTableToShared is used to prevent from infinity recursion
            // in recursive tables
            dumpTable(&table, luaTable, visited);
            return std::move(BaseHolder(table));
        }
        default:
            throw Exception() << "unable to store object of " << luaTypename(luaObject) << " type";
    }
}

} // namespace

StoredObject createStoredObject(bool value) { return std::move(BaseHolder(value)); }

StoredObject createStoredObject(lua_Number value) { return std::move(BaseHolder(value)); }

StoredObject createStoredObject(lua_Integer value) { return std::move(BaseHolder((lua_Number)value)); }

StoredObject createStoredObject(const std::string& value) {
    return std::move(BaseHolder(value));
}

StoredObject createStoredObject(const char* value) {
    return std::move(BaseHolder(std::string(value)));
}

StoredObject createStoredObject(const sol::object& object) { return fromSolObject(object); }

StoredObject createStoredObject(const sol::stack_object& object) { return fromSolObject(object); }

/*
template <typename DataType>
sol::optional<DataType> getPrimitiveHolderData(const StoredObject& sobj) {
    auto ptr = dynamic_cast<PrimitiveHolder<DataType>*>(sobj.get());
    if (ptr)
        return ptr->getData();
    return sol::optional<DataType>();
}

sol::optional<bool> storedObjectToBool(const StoredObject& sobj) {
    return getPrimitiveHolderData<bool>(sobj);
}

sol::optional<double> storedObjectToDouble(const StoredObject& sobj) { return getPrimitiveHolderData<double>(sobj); }


sol::optional<std::string> storedObjectToString(const StoredObject& sobj) {
    return getPrimitiveHolderData<std::string>(sobj);
}
*/

sol::optional<LUA_INDEX_TYPE> BaseHolder::toIndexType(const BaseHolder& holder) {
    if (holder.type_ == StoredType::Number)
        return holder.number_;
    else
        return sol::nullopt;
}

} // effil
