
local luno = require("luno")
local ctx = luno.getcontext(1)

local _uno = {}
for k, v in pairs(luno) do _uno[k] = v end

----------------------------------------
-- Generates types from __unotypes field
local function _getTypes(obj)
    local _obj = obj
    local checked = {}
    local typenames = {}
    local unotypes
    while not checked[_obj] do
        checked[_obj] = true
        
        if type(_obj) == "table" then
            unotypes = rawget(_obj, "__unotypes")
            _obj = getmetatable(_obj)
        elseif type(_obj) == "funtion" then
            unotypes = _obj(obj, "__unotypes")
            _obj = nil
        end
        if type(unotypes) == "table" then
            for _, v in ipairs(unotypes) do typenames[v] = true end
        end
        
        if type(_obj) == "table" and _obj.__index then _obj = _obj.__index
        else break end
    end
    typenames["com.sun.star.lang.XTypeProvider"] = true
    local types = {}
    for k, _ in pairs(typenames) do
        table.insert(types, luno.Type(k)) -- ToDo
    end
    types = luno.Seq("[]type", types)
    return types
end

local function getTypes(obj)
    local types = rawget(obj, "__unotypes2")
    if not types then
        local status
        status, types = pcall(_getTypes, obj)
        if status then
            rawset(obj, "__unotypes2", types)
        else
            types = {}
            -- ToDo error?
        end
    end
    return types
end

local function getImplementationId(obj)
    local id = rawget(obj, "__unoid")
    if id == nil then
        id = luno.uuid()
        rawset(obj, "__unoid", id)
    end
    return id
end

--- Provides com.sun.star.lang.XTypeProvider interface.
-- metatable should be have __unotypes field that keeps 
-- table of supported interface names. __unotypes2 and 
-- __unoid fields are filled to keep values.
-- @param metatable table that gets a few methods
-- @return passed metatable
_uno.addtypeinfo = function (metatable)
    metatable.getTypes = getTypes
    metatable.getImplementationId = getImplementationId
    return metatable
end

----------------------------------------
-- com.sun.star.lang.XServiceInfo
local function getImplementationName(self) return self.__implename end

local function supportsService(self, name)
    for _, _name in ipairs(self.__servicenames) do
        if name == _name then return true end
    end
    return false
end

local function getSupportedServiceNames(self) return self.__servicenames end

--- Provides com.sun.star.lang.XServiceInfo interface.
-- metatable should be have __implename field that keeps implementation 
-- name of the component and __servicenames field keeps table of 
-- supported service names. XServiceInfo name is introduced to 
-- __unotypes field. Use this with uno.addtypeinfo function.
-- @param metatable table that gets a few methods
-- @return passed metatable
_uno.addserviceinfo = function (metatable)
    metatable.getImplementationName = getImplementationName
    metatable.supportsService = supportsService
    metatable.getSupportedServiceNames = getSupportedServiceNames
    if type(metatable.__unotypes) == "table" then
        -- ToDo add if not found
        table.insert(metatable.__unotypes, "com.sun.star.lang.XServiceInfo")
    end
    return metatable
end


return _uno
