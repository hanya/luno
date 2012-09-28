--- Component loader for Lua
----------------------------------------
--- Logging error or something.
-- LUNO_LOG environmental variable is checked at loading time, 
-- and it can be NOTSET, DEBUG, INFO or ERROR.
-- If LUNO_LOG_FILE specifies a file to write, log messages are 
-- stored into it.
local logger = {}
function logger:log(level, message)
    level = level or 0
    if level >= self.level then
        local _message = string.format(
                    "%s: %s\n", os.date("%H:%M:%S"), message)
        if self.path then
            local f = io.open(self.path, "a+")
            if f then f:write(_message); f:flush(); f:close(); return end
        end
        print(_message)
    end
end
function logger:debug(message) self:log(1, message) end
function logger:info (message) self:log(2, message) end
function logger:error(message) self:log(3, message) end
do
    local _level = os.getenv("LUNO_LOG")
    local level = _level and (
        {NOTSET = 0, DEBUG = 1, INFO = 2, ERROR = 3})[_level:upper()] or 4
    logger.path = os.getenv("LUNO_LOG_FILE")
    logger.level = level
end
logger:debug("LuaLoader, executing lualoader.lua")

----------------------------------------
local uno
local ext
if select("#", ...) == 2 then
    local path = tostring(select(1, ...))
    ext = tostring(select(2, ...))
    -- Load luno and uno
    logger:debug("LuaLoader, loading luno, ext: "..ext..", path: "..path)
    local openluno = package.loadlib(path.."luno"..ext, "luaopen_luno")
    if not openluno then
        logger:error("LuaLoader, failed to load luno")
        error("Failed to load luno")
    end
    
    local status, luno = pcall(openluno)
    if not status then
        logger:error("LuaLoader, failed to execute luno: "..tostring(luno))
        error("Failed to execute luno")
    end
    package.loaded["luno"] = luno
    
    local openuno, message = loadfile(path.."uno.lua", "t")
    if not openuno then
        logger:error("LuaLoader, failed to load uno: "..tostring(message))
        error("Failed to load uno")
    end
    
    status, uno = pcall(openuno)
    if not status then
        logger:error("LuaLoader, failed to execute uno: "..tostring(uno))
    end
    package.loaded["uno"] = uno
    uno = luno -- luno is enough
    
    -- ToDo LUA_PATH and LUA_CPATH?
    package.path = ""
    package.cpath = ""
else
    logger:error("LuaLoader, illegal number of arguments for lualoader.lua")
    error("lunoloader: illegal number of arguments")
end

----------------------------------------
--- Addes traceback message to error message.
-- @param message error message passed by xpcall
-- @return error message with traceback
local function tracebackmsgh(message)
    return debug.traceback(message)
    -- ToDo uno exception?
end

----------------------------------------
-- Custom require function
-- ToDo in C with upvalue and ctor function to create require function
local _require = {}
local __require = require

local function copytable(source, dest)
    for k, v in pairs(source) do dest[k] = v end
    return dest
end

--- Creates new custom require function.
-- This function helps local import.
-- @param pwd 
-- @return new callable table
_require.__call = function (self, name)
    -- Check already loaded
    local m = self.loaded[name]
    if m then return m end
    
    -- Find loader
    local loader, env, filepath, message
    loader = package.preload[name]
    if not loader then
        -- Find from local
        local _message
        filepath, _message = package.searchpath(name, self.path) -- 5.2/luajit
        if filepath then
            -- copytable(_G, env)
            -- env._G = env
            -- loader, _message = loadfile(filepath, "t", env) -- 5.2
            loader, _message = loadfile(filepath, "t")
            if not loader then error(_message) end
        else
            message = _message
            filepath, _message = package.searchpath(name, self.cpath)
            if filepath then
                loader, _message = package.loadlib(filepath, "luaopen_"..name)
                if not loader then error(_message) end
            else message = message .. "\n" .. _message end
        end
    end
    if loader then
        -- Set env from function which call this
        env = getfenv(2)
        setfenv(loader, env) -- 5.1/luajit
        
        local status, ret = pcall(loader, name, filepath)
        if status then self.loaded[name] = ret or true
        else error(ret) end
    else
        -- Try with default require function
        local status, ret = pcall(__require, name)
        if status then return ret
        else error(ret .. "\n" .. message) end
    end
    return self.loaded[name]
end

--- Creates new custom require function.
-- This function helps to import from local.
-- Search order is:  first pwd/lua/? and then pwd/?.
-- @param pwd current directory
-- @return new callable table
local function _require_new(pwd)
    local ps = package.config:sub(1, 1)
    local sep = package.config:sub(3, 3)
    local ph = package.config:sub(5, 5)
    pwd = pwd:sub(-1) == ps and pwd or pwd .. ps
    
    local path  = pwd.."lua"..ps..ph..".lua"..sep..
                  pwd.."lua"..ps..ph..ps.."init.lua"..sep..
                  pwd..ph..".lua"..sep..
                  pwd..ph..ps.."init.lua"
    local cpath = pwd.."lua"..ps..ph..ext..sep..
                  pwd.."lua"..ps..ph..ps.."loadall"..ext..sep..
                  pwd..ph..ext..sep..
                  pwd..ph..ps.."loadall"..ext
    local obj = {pwd = pwd, path = path, cpath = cpath, loaded = {}}
    logger:debug("LuaLoader, new require funciton, pwd: "..pwd)
    return setmetatable(obj, _require)
end

----------------------------------------
--- Registered component factories.
local LUNO_REGISTERED = {}

--- Get factory.
-- @param ctx component context
-- @param implename implementation name
-- @param url component location
-- @return data with factory
function LUNO_REGISTERED:getimple(ctx, implename, url)
    local info = self:find(implename, url, true)
    if not info then
        local infos, f = self:load(ctx, url)
        self[url] = {infos, f}
        info = self:find(implename, url)
    end
    return info
end

--- Find factory data.
-- @param implename implementation name
-- @param url component location
-- @return data with factory if found
function LUNO_REGISTERED:find(implename, url, firsttime)
    local info, infos
    local data = self[url]
    if data and #data == 2 then infos = data[1] end
    if infos then
        for _, i in ipairs(infos) do
            if type(i) == "table" and 
               i[2] == implename or i.implename == implename then
                info = i; break
            end
        end
    end
    if not firsttime and not info then
        logger:debug("No implementation data found: "..implename..", "..url)
    end
    return info
end

--- Load component from file.
-- @param ctx component context
-- @param url component location
-- @return data with factory
function LUNO_REGISTERED:load(ctx, url)
    local infos
    local path = self:getpath(ctx, url)
    local dirpath = uno.tosystempath(url:match("^(.*/)[^/]*$"))
    --local env = copytable(_G, {}) -- 5.2
    --env._G = env
    --env.require = _require_new(dirpath)
    logger:debug("LuaLoader, loading path: "..path)
    --local f, message = loadfile(path, "bt", env)
    local f, message = loadfile(path) -- 5.1/luajit
    if f then
        local status
        local env = copytable(getfenv(0), {}) -- 5.1/luajit
        env.require = _require_new(dirpath)
        setfenv(f, env)
        
        status, ret = xpcall(f, tracebackmsgh) -- 5.2/luajit
        if status then
            -- maybe ret is nil but it is not used anymore
            local data = env.IMPLEMENTATIONS
            if data and type(data) == "table" then
                infos = data
            else
                -- No implementation data specified
                logger:error("IMPLEMENTATIONS table is invalid or not found "..url)
            end
        else
            -- Execution error
            logger:error("Error on first execution: "..url..", "..tostring(ret))
            ret = nil
        end
    else
        -- Syntax error or memory error
        logger:error("Failed to load: "..url..", "..tostring(message))
    end
    return infos, f
end

--- Expand macros.
-- @param ctx component context
-- @param url component location
-- @return expanded url
function LUNO_REGISTERED:getpath(ctx, url)
    if url:sub(1, 20) == "vnd.sun.star.expand:" then
        url = ctx:getValueByName(
            "/singletons/com.sun.star.util.theMacroExpander"):expandMacros(url)
        url = url:sub(21)
    end
    return uno.tosystempath(url)
end

----------------------------------------
-- Factory wrapper for each component factory.
local SingleComponentFactory = {__unoid = uno.uuid()}
SingleComponentFactory.__index = SingleComponentFactory
uno.asuno(SingleComponentFactory)

function SingleComponentFactory.new(implename, url)
    logger:debug("factory created for: "..implename..", "..url)
    -- 1: implementation name, 2: url, 
    -- 3: status, 4: service names, 5: ctor
    return setmetatable({implename, url}, SingleComponentFactory)
end

-- com.sun.star.lang.XTypeProvider
function SingleComponentFactory:getTypes()
    return uno.Seq("[]type", {
        uno.Type("com.sun.star.lang.XSingleComponentFactory"), 
        uno.Type("com.sun.star.lang.XServiceInfo"), 
        uno.Type("com.sun.star.lang.XTypeProvider")
    })
end

function SingleComponentFactory:getImplementationId() return self.__unoid end

-- com.sun.star.lang.XSingleComponentFactory
function SingleComponentFactory:createInstanceWithContext(ctx)
    return self:createInstanceWithArgumentsAndContext(nil, ctx)
end

local Exception = uno.require("com.sun.star.uno.Exception")

function SingleComponentFactory:createInstanceWithArgumentsAndContext(args, ctx)
    if self[3] == nil then self:_load(ctx) end
    if self[3] then
        -- ToDo call initialize method?
        local status, ret = pcall(self[5], ctx, args)
        if status then return ret end
        logger:error("Failed to create component: "..tostring(ret))
        error(Exception(tostring(ret), nil))
        -- if ret is Exception, raise or raise as Exception?
    end
    -- Component not found
    logger:error("Component not found: "..self[1]..", "..self[2])
    return nil
end

-- com.sun.star.lang.XServiceInfo
function SingleComponentFactory:getImplementationName()
    return self[1]
end

function SingleComponentFactory:supportsService(name)
    if self[3] then
        local servicenames = self[4]
        for _, _name in ipairs(servicenames) do
            if _name == name then return true end
        end
    end
    return false
end

function SingleComponentFactory:getSupportedServiceNames()
    return self[3] and self[4] or {}
end

--- Check passed object is callable.
-- @param obj
-- @return true if obj is function or callable by __call
local function iscallable(obj)
    if type(obj) ~= "function" then
        local meta = getmetatable(obj)
        return meta and iscallable(meta.__call)
    end
    return true
end

--- Copy component data.
-- @param info component data
-- @return nil
function SingleComponentFactory:_copyinfo(info)
    if type(info) ~= "table" then
        error("implementation information should be in table") end
    
    local implename = info[2] or info.implename
    if implename ~= self[1] then
        error("implementation do not match, found "..implename.." for "..self[1]) end
    
    local ctor = info[1] or info.ctor
    if not iscallable(ctor) then
        error("constructor is not callable: "..tostring(ctor)) end
    
    local servicenames = info[3] or info.servicenames
    if type(servicenames) ~= "table" then
        error("service names should be table contains service names") end
    local _servicenames = {}
    for _, v in ipairs(servicenames) do
        if type(v) == "string" then table.insert(_servicenames, v) end
    end
    self[3] = true
    self[4] = _servicenames
    self[5] = ctor
end

--- Load component implementation.
-- @param ctx component context
-- @return nil
function SingleComponentFactory:_load(ctx)
    local info = LUNO_REGISTERED:getimple(ctx, self[1], self[2])
    local status, message = pcall(self._copyinfo, self, info)
    if not status then
        self[3] = false
        -- No matching implementation or no enough data
        logger:error(message..": "..self[1]..", "..self[2])
    end
end

----------------------------------------
local LuaLoader = {__unoid = uno.uuid()}
LuaLoader.__index = LuaLoader
uno.asuno(LuaLoader)

function LuaLoader.new(ctx)
    logger:debug("LuaLoader:new, created")
    return setmetatable({}, LuaLoader)
end

-- com.sun.star.lang.XTypeProvider
function LuaLoader:getTypes()
    return uno.Seq("[]type", {
        uno.Type("com.sun.star.loader.XImplementationLoader"), 
        uno.Type("com.sun.star.lang.XServiceInfo"), 
        uno.Type("com.sun.star.lang.XTypeProvider")
    })
end

function LuaLoader:getImplementationId() return self.__unoid end

-- com.sun.star.lang.XServiceInfo
function LuaLoader:getImplementationName()
    return "mytools.loader.Lua"
end

function LuaLoader:supportsService(name)
    return name == "com.sun.star.loader.Lua"
end

function LuaLoader:getSupportedServiceNames()
    return {"com.sun.star.loader.Lua"}
end

-- com.sun.star.lang.XSingleComponentFactory
function LuaLoader:activate(implename, ignored, url, key)
    logger:debug("LuaLoader:activate, "..implename..": "..url)
    return SingleComponentFactory.new(implename, url)
end

function LuaLoader:writeRegistryInfo(key, ignored, url)
    return true
end

logger:debug("LuaLoader, loading done")
----------------------------------------
-- Returned to C++ implementation
return LuaLoader.new
