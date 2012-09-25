
local luno = require("luno")
local ctx = luno.getcontext(1)

local _uno = {}
for k, v in pairs(luno) do _uno[k] = v end

----------------------------------------
-- Generates types from __unotypes field
local function _gettypes(obj)
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
        status, types = pcall(_gettypes, obj)
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
    -- ToDo it should be cached to metatable
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
--_uno.addtypeinfo = function (metatable)
function _uno.addtypeinfo(metatable)
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
--_uno.addserviceinfo = function (metatable)
function _uno.addserviceinfo(metatable)
    metatable.getImplementationName = getImplementationName
    metatable.supportsService = supportsService
    metatable.getSupportedServiceNames = getSupportedServiceNames
    if type(metatable.__unotypes) == "table" then
        -- ToDo add if not found
        table.insert(metatable.__unotypes, "com.sun.star.lang.XServiceInfo")
    end
    return metatable
end


----------------------------------------
--- Returns next element by index access.
-- Index in index container starts with 0.
-- @param obj proxy supports com.sun.star.container.XIndexAccess
-- @param n next index
-- @return index and element
local function indexiter(obj, n)
    -- Index of index container starts with 0
    local f = obj.getByIndex
    if f then
        n = n + 1
        local status, ret = pcall(f, obj, n)
        if status then return n, ret end
    end
    return nil, nil
end

--- Iterates elements in index container.
-- @param obj proxy supports com.sun.star.container.XIndexAccess
-- @return iterator, object and -1
function _uno.ipairs(obj)
    if luno.hasinterface(obj, "com.sun.star.container.XIndexAccess") then
        return indexiter, obj, -1
    end
    return ipairs(obj)
end

--- Returns next element by name access.
-- @param obj proxy supports com.sun.star.container.XNameAccess
-- @param n index
-- @return name and element
local function nameiter(obj, n)
    local f = obj.getByName
    if f then
        local status, ret
        local names = obj:getElementNames()
        for _, name in ipairs(names) do
            if obj:hasByName(name) then
                status, ret = pcall(f, obj, name)
                if status then coroutine.yield(name, ret) end
            end
        end
    end
    return nil, nil
end

--- Iterates elements in name container.
-- @param obj proxy supports com.sun.star.container.XNameAccess
-- @return function returns next, object and nil
function _uno.pairs(obj)
    if luno.hasinterface(obj, "com.sun.star.container.XNameAccess") then
        return coroutine.wrap(nameiter), obj, nil
    end
    return pairs(obj)
end

--- Returns next element by enumeration access.
-- @param obj proxy supports com.sun.star.container.XEnumerationAccess or XEnumeration
-- @param _dummy not used
-- @return dummy index and element
local function enumiter(obj, _dummy)
    local e
    if luno.hasinterface(obj, "com.sun.star.container.XEnumerationAccess") then
        e = obj:createEnumeration()
    elseif luno.hasinterface(obj, "com.sun.star.container.XEnumeration") then
        e = obj
    end
    if e then
        local f = e.nextElement
        if f then
            local status, ret
            local n = 0
            while e:hasMoreElements() do
                status, ret = pcall(f, e)
                if status then coroutine.yield(n, ret) end
                n = n + 1
            end
        end
    end
    return nil, nil
end

-- Iterates elements in enumeration container.
-- Index return value is dummy numerical value.
-- @param obj proxy supports com.sun.star.container.XEnumerationAccess or XEnumeration
-- @return function returns next, object and 0
function _uno.iterate(obj)
    if luno.hasinterface(obj, "com.sun.star.container.XEnumerationAccess") or 
       luno.hasinterface(obj, "com.sun.star.container.XEnumeration") then
        return coroutine.wrap(enumiter), obj, 0
    end
    return nil, nil, nil -- error?
end


return _uno
