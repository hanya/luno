
local luno = require("luno")
luno.getcontext()

local _uno = {}

for k, v in pairs(luno) do
    _uno[k] = v
end


-- This is still under development.
------------------------------
-- Provides com.sun.star.lang.XTypeProvider interface
-- 
-- Pass metatable to uno.prepare function, it adds 
-- getTypes and getImplementataionId methods.
--
--  local Meta = {}
--  Meta.__index = Meta
--  Meta.__unotypes = {
--      "com.sun.star.awt.XActionListener", 
--  }
--  uno.asuno(Meta)
--  uno.prepare(Meta)
--
--  getTypes, getImplementataionId methods are added 
--  and __unotypes2 and __unoid fields are filled.

-- Generates types from __unotypes field
_uno.getTypes = function (obj)
    local types = obj.__unotypes2
    if not types then
        local _obj = obj
        local insert = table.insert
        local alltypes = {}
        local checked = {}
        local _types, meta
        repeat
            if not checked[_obj] then
                checked[_obj] = true
                _types = _obj.__unotypes
                if type(_types) == "table" then
                    for _, name in ipairs(_types) do
                        if type(name) == "string" then
                            insert(alltypes, luno.Type(name))
                        end
                    end
                end
                _obj = getmetatable(_obj)
                meta = getmetatable(_obj)
                if _obj and meta then _obj = meta.__index 
                else _obj = nil end
            else
                _obj = nil
            end
        until not _obj
        -- ToDo make identical
        insert(alltypes, luno.Type("com.sun.star.lang.XTypeProvider"))
        types = luno.Seq("[]type", alltypes)
        obj.__unotypes2 = types
    end
    return types
end


_uno.getImplementationId = function (obj)
    local id = obj.__unoid
    if id == nil then
        id = luno.uuid()
        obj.__unoid = id
    end
    return id
end


_uno.prepare = function (metatable)
    metatable.getTypes = _uno.getTypes
    metatable.getImplementationId = _uno.getImplementationId
    return metatable
end


return _uno
