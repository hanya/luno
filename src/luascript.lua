--- Script provider for Lua.
----------------------------------------
local uno = require("uno")
----------------------------------------
--- Logging error or something.
-- LUNO_PROVIDER_LOG environmental variable is checked at loading time, 
-- and it can be NOTSET, DEBUG, or ERROR.
-- If LUNO_PROVIDER_LOG_FILE specifies a file to write, log messages are 
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
        else
            print(_message)
        end
    end
end
function logger:debug(message) self:log(1, message) end
function logger:error(message) self:log(2, message) end
do
    local _level = os.getenv("LUNO_PROVIDER_LOG")
    local level = _level and (
        {NOTSET = 0, DEBUG = 1, ERROR = 2})[_level:upper()] or 3
    logger.path = os.getenv("LUNO_PROVIDER_LOG_FILE")
    logger.level = level
end
logger:debug("LuaScriptProvider, executing")

----------------------------------------
--- Addes traceback message to error message.
-- @param message error message passed by xpcall
-- @return error message with traceback
local function tracebackmsgh(message)
    return debug.traceback(message)
    -- ToDo uno exception?
end


----------------------------------------
local LANGUAGE = "Lua"
----------------------------------------
--local css = uno.require("com.sun.star")
local BrowseNodeTypes = uno.require("com.sun.star.script.browse.BrowseNodeTypes")
local ScriptFrameworkErrorException = uno.require(
        "com.sun.star.script.provider.ScriptFrameworkErrorException")

----------------------------------------
--- Interaction handler does noting.
local DummyInteractionHandler = {}
DummyInteractionHandler.__index = DummyInteractionHandler
DummyInteractionHandler.__unotypes = {"com.sun.star.task.XInteractionHandler"}
uno.asuno(DummyInteractionHandler)
uno.addtypeinfo(DummyInteractionHandler)

function DummyInteractionHandler.new()
    return setmetatable({}, DummyInteractionHandler)
end
function DummyInteractionHandler:handle(request) end

--- Progress handler does nothing.
local DummyProgressHandler = {}
DummyProgressHandler.__index = DummyProgressHandler
DummyProgressHandler.__unotypes = {"com.sun.star.ucb.XProgressHandler"}
uno.asuno(DummyProgressHandler)
uno.addtypeinfo(DummyProgressHandler)

function DummyProgressHandler.new()
    return setmetatable({}, DummyProgressHandler)
end
function DummyProgressHandler:push(status) end
function DummyProgressHandler:update(status) end

--- Command environment for dummy use.
local CommandEnvironment = {}
CommandEnvironment.__index = CommandEnvironment
CommandEnvironment.__unotypes = {"com.sun.star.ucb.XCommandEnvironment"}
uno.asuno(CommandEnvironment)
uno.addtypeinfo(CommandEnvironment)

function CommandEnvironment.new()
    return setmetatable({DummyInteractionHandler.new(), 
        DummyProgressHandler.new()}, CommandEnvironment)
end
function CommandEnvironment:getInteractionHandler() return self[1] end
function CommandEnvironment:getProgressHandler() return self[2] end

local Property = uno.require("com.sun.star.beans.Property")
local Command = uno.require("com.sun.star.ucb.Command")

--- Gets document model for transient URI.
-- @param ctx component context
-- @param uri transient URI of the document
-- @return document model
local function getdocumentmodel(ctx, uri)
    local doc
    local ucb = ctx:getServiceManager():createInstanceWithArgumentsAndContext(
            "com.sun.star.ucb.UniversalContentBroker", {"Local", "Office"}, ctx)
    local identifier = ucb:createContentIdentifier(uri)
    local content = ucb:queryContent(identifier)
    local property = Property("DocumentModel", -1, uno.Type("void"), 1)
    local command = Command("getPropertyValues", -1, 
                        uno.Seq("[]com.sun.star.beans.Property", {property}))
    local commandenv = CommandEnvironment.new()
    local state, resultset = pcall(content.execute, content, command, -1, commandenv)
    if status then
        doc = resultset:getObject(1, nil)
    else
        logger:error("LuaScriptProvider, getdocumentmodel: "..tostring(resultset))
    end
    return doc
end

--- Gets transient URI for document model.
-- @param ctx component context
-- @param doc document model
-- @return transient URI with vnd.sun.star.tdoc protocol
local function getdocumentcontext(ctx, doc)
    return ctx:getServiceManager():createInstanceWithContext(
        "com.sun.star.frame.TransientDocumentsDocumentContentFactory", ctx)
        :createDocumentContent(doc):getIdentifier():getContentIdentifier()
end

----------------------------------------
--- XScript for Lua macro.
local LuaScript = {}
LuaScript.__index = LuaScript
LuaScript.__unotypes = {"com.sun.star.script.provider.XScript"}
uno.asuno(LuaScript)
uno.addtypeinfo(LuaScript)

--- Creates new XScript.
-- @param func function to execute
-- @param env execution environment
-- @param filename file name
-- @param funcname function name
-- @return new instance
function LuaScript.new(func, env, filename, funcname)
    return setmetatable({func, env, filename, funcname}, LuaScript)
end

-- XScript
function LuaScript:invoke(args, paramindex, outparams)
    local ret
    local _args = args:totable()
    local status, ret = xpcall(self[1], tracebackmsgh, table.unpack(_args)) -- 5.2/luajit
    if not status then
        local message
        -- ToDo error
        if type(ret) == "userdata" and uno.typename(ret) == "exception" then
            message = ret.Message
        end
        status, message = pcall(tostring, ret)
        if not status then message = "failed to convert error message to string" end
        message = tostring(message)
        error(message)
        --error(ScriptFrameworkErrorException(message, nil, self[4], LANGUAGE, 0))
    end
    return ret, {}, {}
end

----------------------------------------
--- Wrapper for script URL.
local URI = {}
URI.__index = URI

--- Creates new instance of URI.
-- @param ctx component context
-- @param context name of script context
-- @param uri base URI
-- @return new instance
function URI.new(ctx, context, uri)
    local obj = {ctx = ctx, context = context, uri = uri}
    setmetatable(obj, URI)
    if not uri then obj.uri = obj:gethelper():getRootStorageURI() end
    return obj
end

function URI:__tostring() return self.uri end

--- Create new intance based on this.
-- @param uri new URI
-- @return new instance of URI
function URI:create(uri)
    return URI.new(self.ctx, self.context, tostring(uri))
end

--- Returns ScriptURIHelper instance.
-- @return new instance of helper
function URI:gethelper()
    return self:createservice(
        "com.sun.star.script.provider.ScriptURIHelper", 
        {LANGUAGE, self.context})
end

--- Checks this URI is transient.
-- @return true if transient, false otherwise
function URI:istransient()
    return self.uri:sub(1, 18) == "vnd.sun.star.tdoc:"
end

--- Returns file name.
-- @param suffix removed from the tail if specified
-- @return file name
function URI:basename(suffix)
    local uri = tostring(self)
    local name = uri:match("^.-/([^/]*)/?$")
    if suffix and name:sub(-suffix:len()) == suffix then
        name = name:sub(1, -suffix:len()-1)
    end
    -- ToDo decode
    return name
end

--- Get directory part from URI.
-- @param uri URI to parse.
-- @return new instance of URI
function URI:dirname(uri)
    local dirname = tostring(uri):match("(.+)/[^.]-/+$")
    return URI.new(self.ctx, self.context, dirname)
end

--- Generate macro URI for this URI and function name.
-- @param funcname function name
-- @return macro URI with vnd.sun.star.script protocol
function URI:touri(funcname)
    local uri = self.uri
    if funcname then uri = uri.."$"..funcname end
    return self:gethelper():getScriptURI(uri)
end

--- Convert passed macro URI to URL with function name.
-- @param uri macro URI with vnd.sun.star.script protocol
-- @return file URL and function name
function URI:tourl(uri)
    local url = self:gethelper():getStorageURI(tostring(uri))
    return url:match("^([^$]*)$([^$]*)$")
end

--- Connect URL.
-- @param a connected with this if b is null.
-- @param b connected with a if b is not null.
-- @return new instance of URI
function URI:join(a, b)
    local ret
    if b then
        local _a = tostring(a)
        local _b = tostring(b)
        ret = _a:sub(-1) == "/" and _a.._b or _a.."/".._b
    else
        ret = self.uri:sub(-1) == "/" and self.uri.._a or self.uri.."/"..a
    end
    return URI.new(self.ctx, self.context, ret)
end

--- Expand macro in URL.
-- @param uri URI to expand
-- @return expanded URL
function URI:expand(uri)
    uri = tostring(uri)
    if uri:sub(1, 20) == "vnd.sun.star.expand:" then
        uri = self.ctx:getValueByName(
            "/singletons/com.sun.star.util.theMacroExpander"):expandMacros(uri)
        uri = uri:sub(21)
        local status, _uri = pcall(uno.absolutize, uri, uri)
        if status then uri = _uri end
    end
    if uri:sub(-1) ~= "/" then uri = uri .. "/" end
    return uri
end

--- Create new service instance.
-- @param name service name
-- @param args arguments to initialize in table
-- @return new instance of arguments
function URI:createservice(name, args)
    if args ~= nil then
        return self.ctx:getServiceManager()
            :createInstanceWithArgumentsAndContext(name, args, self.ctx)
    else
        return self.ctx:getServiceManager()
            :createInstanceWithContext(name, self.ctx)
    end
end

----------------------------------------
--- File and directory manipulation.
local Storage = {}
Storage.__index = Storage

--- Creates new Storage.
-- @param ctx component context
-- @return new instance
function Storage.new(ctx)
    local obj = {ctx = ctx, }
    setmetatable(obj, Storage)
    obj.sfa = obj:createservice("com.sun.star.ucb.SimpleFileAccess")
    return obj
end

--- Create new instance of service.
-- @param name service name to instantiate
-- @return new instance of service
function Storage:createservice(name)
    return self.ctx:getServiceManager():createInstanceWithContext(name, self.ctx)
end

--- Gets modified timestamp of specified file.
-- @param uri file path to check
-- @return timestamp in os.time format
function Storage:getmodified(uri)
    local dt = self.sfa:getDateTimeModified(tostring(uri))
    return os.time({year=dt.Year, month=dt.Month, day=dt.Day, 
            hour=dt.Hours, min=dt.Minutes, sec=dt.Seconds})
end

--- Checks specified file is exist.
-- @param uri file path to check
-- @return true if the file exists, otherwise false
function Storage:exists(uri)
    local status, ret = pcall(self.sfa.exists, self.sfa, tostring(uri))
    if status then return ret end
    return nil
end

--- Checks specified item is directory.
-- @param uri item path to check
-- @return true if the item is directory, otherwise false
function Storage:isdir(uri)
    if self:exists(uri) then
        local status, ret = pcall(self.sfa.isFolder, self.sfa, tostring(uri))
        if status then return ret end
    end
    return nil
end

--- Checks specified item is readonly.
-- @param uri item path to check
-- @return true if the item is readonly, otherwise false
function Storage:isreadonly(uri)
    if self:exists(uri) then
        local status, ret = pcall(self.sfa.isReadOnly, self.sfa, tostring(uri))
        if status then return ret end
    end
    return nil
end

--- Rename specified item.
-- @param uri item path to rename
-- @param desturi new name
-- @return true if succeed to rename, otherwise false
function Storage:rename(uri, desturi)
    if self:exists(uri) and not self:isreadonly(uri) then
        local status, message = pcall(
                self.sfa.move, self.sfa, tostring(uri), tostring(desturi))
        return status
    end
    return nil
end

--- Delete specified item.
-- @param uri item path to delete
-- @return true if succeed to delete
function Storage:delete(uri)
    if self:exists(uri) and not self:isreadonly(uri) then
        local status, message = pcall(self.sfa.kill, self.sfa, tostring(uri))
        return status
    end
    return nil
end

--- Gets list of directory contents.
-- @param uri directory path to get list of contents
-- @param withdir include directories if true
-- @return table of directory contents in full path
function Storage:dircontents(uri, withdir)
    withdir = type(withdir) == "boolean" and withdir or true
    if self:exists(tostring(uri)) then
        local status, ret = pcall(
                self.sfa.getFolderContents, self.sfa, tostring(uri), withdir)
        if status then return ret end
    end
    return {}
end

--- Creates new directory.
-- @param uri directory path to create
-- @return nil
function Storage:dircreate(uri)
    if not self:exists(tostring(uri)) then
        local status, message = pcall(
                self.sfa.createFolder, self.sfa, tostring(uri))
        return status
    end
    return true
end

--- Create file.
-- @param uri file URI to create
-- @return true if created
function Storage:filecreate(uri)
    local _uri = uri:dirname()
    if self:dircreate(_uri) then
        local status, message = pcall(self.writetext, self, uri, "")
        return status
    end
    return false
end

--- Copy file.
-- @param source source file to copy
-- @param dest desitination for copied file
-- @return nil
function Storage:filecopy(source, dest)
    local sfa = self.sfa
    local source_ = tostring(source)
    local dest_ = tostring(dest)
    if sfa:exists(source_) then
        local input = sfa:openFileRead(source_)
        if sfa:exists(dest_) then sfa:kill(dest_) end
        sfa:writeFile(dest_, input)
        input:closeInput()
    end
end

--- Write text into a file.
-- @param uri file URI
-- @param text string to write
-- @return nil
function Storage:writetext(uri, text)
    local textout = self:createservice("com.sun.star.io.TextOutputStream")
    if uri:istransient() then
        local pipe = self:createservice("com.sun.star.io.Pipe")
        textout:setOutputStream(pipe)
        textout:writeString(text)
        textout:closeOutput()
        self.sfa:writeFile(tostring(uri), pipe)
        pipe:closeInput()
    else
        local fileout = self.sfa:openFileWrite(tostring(uri))
        textout:setOutputStream(fileout)
        textout:writeString(text)
        textout:flush()
        textout:closeOutput()
    end
end

--- Load file content as string.
-- @param uri file URI to load content
-- @return text loaded
function Storage:filecontent(uri)
    local text = ""
    local bytes
    local n = 0
    local input = self.sfa:openFileRead(tostring(uri))
    while true do
        n, bytes = input:readBytes(nil, 0xffff)
        text = text .. bytes:tostring()
        if n < 0xffff then break end
    end
    input:closeInput()
    return text
end

--- Let user to choose a file.
-- @return file URL or nil if canceled
function Storage:choosefile()
    local ret
    local fp = self:createservice("com.sun.star.ui.dialogs.FilePicker")
    fp:appendFilter("Lua (*.lua)", "*.lua")
    fp:appendFilter("All Files (*.*)", "*.*")
    local origin = self:createservice("com.sun.star.util.PathSubstitution"):
        substituteVariables("$(user)/Scripts/lua", true)
    fp:setDisplayDirectory(origin)
    fp:setMultiSelectionMode(false)
    
    if fp:execute() == 1 then ret = fp:getFiles()[1] end
    fp:dispose()
    return ret
end

----------------------------------------
-- Custom require function
local _require = {}
_require.__index = _require
local __require = require

local function copytable(source, dest)
    for k, v in pairs(source) do dest[k] = v end
    return dest
end

--- Search inside document.
-- @param name module name
-- @param path search path separated with ;
-- @return path to module if found, otherwise nil and message
function _require.docsearchpath(self, name, path)
    local found, p, message
    local _name = name:gsub("%.", package.config:sub(1, 1))
    local storage = self.storage
    message = ""
    if storage then
        for _path in path:gmatch("[^;]*") do
            -- ; and ? from package.config
            p = _path:gsub("%?", _name, 1)
            if storage:exists(p) then found = p; break
            else message = message .. p .. "\n" end
        end
    end
    if found then return found
    else return nil, message end
end

--- Search module.
-- @param name module name
-- @return file path to module if found, otherwise nil and message
function _require.loadmodule(self, name)
    local loader, filepath, message, _message
    if self.pwd:sub(1, 18) == "vnd.sun.star.tdoc:" then
        filepath, _message = self:docsearchpath(name, self.path)
        if filepath then
            local storage = self.storage
            local status, source = xpcall(storage.filecontent, tracebackmsgh, 
                                        storage, filepath)
            if status then
                loader, _message = load(source, source, "t") -- 5.2/luajit?
                if not loader then error(_message) end
            else
                -- Loading error
                error("Error on loading: "..filepath..", "..tostring(source))
            end
        else
            -- Loading C extension from document is not supported yet
            -- Extract to temp and load it?
            return nil, _message
        end
    else
        filepath, _message = package.searchpath(name, self.path) -- 5.2/luajit
        if filepath then
            -- on 5.2, custom env should be set
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
    return loader, filepath
end

--- Require with custom procedure.
-- @param name module name to import
-- @return return value from module
function _require.__call(self, name)
    -- Check already loaded
    local m = self.loaded[name]
    if m then return m end
    
    -- Find loader
    local loader, filepath, message
    loader = package.preload[name]
    
    -- Find from custom path and cpath
    if not loader then loader, filepath = self:loadmodule(name) end
    if loader then
        -- Set env from function which call this
        setfenv(loader, getfenv(2)) -- 5.1/luajit
        
        local status, ret = pcall(loader, name, filepath)
        if status then self.loaded[name] = ret or true
        else error(ret) end
    else
        local status, ret = pcall(__require, name)
        if status then return ret
        else error(ret .. "\n" .. tostring(message)) end
    end
    return self.loaded[name]
end

--- Creates new custom require function.
-- This function helps local import.
-- @param pwd current directory
-- @return new callable table
local function _require_new(pwd, storage)
    local ps = package.config:sub(1, 1)
    local sep = package.config:sub(3, 3)
    local ph = package.config:sub(5, 5)
    local ext = ps == "/" and ".so" or ".dll" -- ToDo OS/2
    pwd = pwd:sub(-1) == ps and pwd or pwd .. ps
    
    local path  = pwd..ph..".lua"..sep..
                  pwd..ph..ps.."init.lua"
    local cpath = pwd..ph..ext..sep..
                  pwd..ph..ps.."loadall"..ext
    local obj = {pwd = pwd, path = path, cpath = cpath, loaded = {}}
    if pwd:sub(1, 18) == "vnd.sun.star.tdoc:" then
        -- Add user's script location to path
        local uri = tostring(URI.new(storage.ctx, "user", nil))
        uri = uri:sub(-1) == ps and uri or uri .. ps
        obj.path = obj.path..sep..
                     uri..ph..".lua"..sep..
                     uri..ph..ps.."init.lua"
        obj.cpath = uri..ph..ext..sep..
                    uri..ph..ps.."loadall"..ext
        
        if storage then obj.storage = storage end -- for inner document
    end
    return setmetatable(obj, _require)
end

----------------------------------------
--- Script container.
local LuaScripts = {}
LuaScripts.__index = LuaScripts

--- Get item with timestamp checking.
-- @param uri URI bound to the item to find
-- @param detetime timestamp in os.time format
-- @return item or nil
function LuaScripts:get(uri, datetime)
    local item = self[tostring(uri)]
    if item then
        if item.timestamp >= datetime then return item
        else self:delete(uri) end
    end
end

--- Adds as new item.
-- @param uri URI bound to the item
-- @param datetime timestamp in os.time format
-- @param capsule function compiled from source
-- @param env table to execute as environment
-- @return created item as table
function LuaScripts:add(uri, datetime, capsule, env)
    local item = {timestamp=datetime, f=capsule, 
                  env=env, status=false, names=nil}
    self[tostring(uri)] = item
    return item
end

--- Deletes specified item.
-- @param uri URI bound to the item
-- @return nil
function LuaScripts:delete(uri)
    self[tostring(uri)] = nil
end

function LuaScripts:copytable(source, dest)
    for k, v in pairs(source) do dest[k] = v end
end

--- Load specified file as an entry.
-- @param storage storage instance
-- @param uri URI to load a file
-- @return table contains something
function LuaScripts:loadfile(storage, uri)
    local item
    local filename = tostring(uri)
    if storage:exists(filename) then
        local time = uri:istransient() and os.time() 
                                       or  storage:getmodified(filename)
        item = self:get(uri, time)
        if not item then
            local env = {}
            env.__main__ = "sf"
            -- env.arg = {[0] = uno.tosystempath(filename)} -- ToDo
            
            local source = storage:filecontent(filename)
            local f, message = loadstring(source, filename) -- 5.1
            --local f, message = load(source, source, "t", env) -- 5.2
            if f then
                setfenv(f, env) -- 5.1
                item = self:add(uri, time, f, env)
            else
                -- Error like a syntax error
                error(ScriptFrameworkErrorException(message, nil, 
                        tostring(uri), LANGUAGE, 0))
            end
        end
    else
        self:delete(uri)
    end
    if not item then
        error(ScriptFrameworkErrorException("Failed to load: "..tostring(uri), 
            nil, tostring(uri), LANGUAGE, 0))
    end
    return item
end

--- Fill env after first execution
-- @param storage storage instance
-- @param uri URI instance
-- @param item script information
-- @return nil
function LuaScripts:fillenv(storage, uri, item)
    local env = item.env
    self:copytable(_G, env)
    env._G = env
    -- Set custom require function
    local dirpath = uri:istransient() and tostring(uri:dirname()) 
                                  or uno.tosystempath(tostring(uri:dirname()))
    env.require = _require_new(dirpath, storage)
    item.status = true
end

--- Get list of function names in specified file.
-- @param storage storage instance
-- @param uri fil URI to load
-- @return table of function names
function LuaScripts:getnames(storage, uri)
    local item = self:loadfile(storage, uri) -- error if failed to load
    local names = item.names
    if not item.status and not names then
        names = {}
        item.names = names
        
        local status, message = pcall(item.f)
        if status then
            local insert = table.insert
            local env = item.env
            -- Filter names
            for k, v in pairs(env) do
                if type(v) == "function" then insert(names, k)
                else names[k] = nil end
            end
            table.sort(names)
            
            self:fillenv(storage, uri, item)
        else
            -- ToDo error on first execution
            logger:error(message)
        end
    end
    return names
end

--- Get specific function as script to execute.
-- @param storage storage instance
-- @param uri file URI to load
-- @param funcname function name
-- @return instance of LuaScript
function LuaScripts:getscript(storage, uri, funcname, scriptcontext)
    local item = self:loadfile(storage, uri)
    if not item.status and not item.names then self:getnames(storage, uri) end
    local env = item.env
    local func = env[funcname]
    if type(func) == "function" then
        env.XSCRIPTCONTEXT = scriptcontext
        return LuaScript.new(func, env, tostring(uri), funcname)
    else
        error(ScriptFrameworkErrorException(
            "function not found: " ..funcname, nil, 
            tostring(uri), LANGUAGE, 0))
    end
end

----------------------------------------
--- XScriptContext for macro.
local ScriptContext = {}
ScriptContext.__index = ScriptContext
ScriptContext.__unotypes = {"com.sun.star.script.provider.XScriptContext"}
uno.asuno(ScriptContext)
uno.addtypeinfo(ScriptContext)

--- Creates new XScriptContext.
-- @param context component context
-- @param document document model
-- @param inv invocation context
-- @return new instance
function ScriptContext.new(context, document, inv)
    return setmetatable({context, document, inv}, ScriptContext)
end

function ScriptContext:__tostring() return "<ScriptContext>" end

-- XScriptContext
function ScriptContext:getComponentContext() return self[1] end

function ScriptContext:getDocument()
    return self[2] or self:getDesktop():getCurrentComponent()
end

function ScriptContext:getDesktop()
    return self[1]:getServiceManager():createInstanceWithContext(
        "com.sun.star.frame.Desktop", self[1])
end

function ScriptContext:getInvocationContext() return self[3] end

----------------------------------------
--- Base node for all script nodes.
local BaseNode = {}
BaseNode.__index = BaseNode
BaseNode.__unotypes = {
    "com.sun.star.script.browse.XBrowseNode", 
    "com.sun.star.script.XInvocation", 
    "com.sun.star.beans.XPropertySet"
}
uno.asuno(BaseNode)
uno.addtypeinfo(BaseNode)

--- Create new browse node.
-- @param name name of this node
-- @param uri instance of URI
-- @param storage instance of Storage
-- @return new instance
function BaseNode.new(name, uri, storage)
    local obj = {name = name, uri = uri, 
                 storage = storage, nodetype = BrowseNodeTypes.CONTAINER}
    setmetatable(obj, BaseNode)
    return obj
end

--- Renames node and file.
-- @param name new name
-- @return true if succeed, otherwise false
function BaseNode:rename(name, isfile)
    local uri = self.uri:join(self.uri:dirname(), name)
    local status = self.storage:rename(self.uri, uri)
    if status then
        if isfile and name:sub(-4) == ".lua" then name = name:sub(1, -5) end
        self.name = name
        self.uri = uri
        return true
    end
    return false
end

--- Checks the file is readonly.
-- @return true if file is readonly
function BaseNode:isreadonly()
    return self.storage:isreadonly(self.uri)
end

-- XBrowseNode
function BaseNode:getName() return self.name end
function BaseNode:getChildNodes() return {} end
function BaseNode:hasChildNodes() return true end
function BaseNode:getType() return self.nodetype end

-- XInvocation
function BaseNode:getIntrospection() return nil end
function BaseNode:setValue(name, value) return nil end
function BaseNode:getValue(name) return nil end
function BaseNode:hasMethod(name) return nil end
function BaseNode:hasProperty(name) return false end
function BaseNode:invoke(name, arg1, arg2, arg3)
    return false, {0}, {uno.Any("void", nil)}
end

-- XPrpertySet
function BaseNode:getPropertySetInfo() return nil end
function BaseNode:addPropertyChangeListener(name, listener) return nil end
function BaseNode:removePropertyChangeListener(name, listener) return nil end
function BaseNode:addVetoableChangeListener(name, listener) return nil end
function BaseNode:removeVetoableChangeListener(name, listener) return nil end
function BaseNode:setPropertyValue(name, value) return nil end
function BaseNode:getPropertyValue(name) return false end

----------------------------------------
--- Browse node for script.
local ScriptBrowseNode = {}
ScriptBrowseNode.__index = ScriptBrowseNode
setmetatable(ScriptBrowseNode, {__index = BaseNode})
uno.asuno(ScriptBrowseNode)
uno.addtypeinfo(ScriptBrowseNode)

--- Node for script.
-- @param name name for UI
-- @param uri script location
-- @param storage storage instance
-- @param funcname function name
-- @param description description for UI
-- @return new instance
function ScriptBrowseNode.new(name, uri, storage, funcname, description)
    local obj = setmetatable(BaseNode.new(name, uri, storage), ScriptBrowseNode)
    obj.funcname = funcname
    obj.description = description or ""
    obj.nodetype = BrowseNodeTypes.SCRIPT
    return obj
end

function ScriptBrowseNode:hasChildNodes() return false end

function ScriptBrowseNode:getPropertyValue(name)
    if name == "URI" then return self.uri:touri(self.funcname)
    elseif name == "Description" then return self.description
    elseif name == "Editable" then return false
    end
    return nil
end

----------------------------------------
--- Browse node for file.
local FileBrowseNode = {}
FileBrowseNode.__index = FileBrowseNode
setmetatable(FileBrowseNode, {__index = BaseNode})
uno.asuno(FileBrowseNode)
uno.addtypeinfo(FileBrowseNode)

--- Node for file.
-- @param name name for UI
-- @param uri file location
-- @param storage storage instance
-- @return new instance
function FileBrowseNode.new(name, uri, storage)
    return setmetatable(BaseNode.new(name, uri, storage), FileBrowseNode)
end

function FileBrowseNode:getChildNodes()
    local items = {}
    local insert = table.insert
    local storage = self.storage
    local uri = self.uri
    if storage:exists(uri) then
        local status, ret = xpcall(LuaScripts.getnames, tracebackmsgh, 
                                    LuaScripts, storage, uri)
        if status then
            for _, name in ipairs(ret) do
                -- ToDo visible name and description
                insert(items, ScriptBrowseNode.new(
                            name, uri, storage, name, ""))
            end
        else
            -- error
            logger:error("FileBrowseNode:getChildNodes, "..tostring(ret))
        end
    end
    return items
end

function FileBrowseNode:getPropertyValue(name)
    if name == "Renamable" or name == "Deletable" then
        return not self.storage:isreadonly(self.uri)
    elseif name == "Editable" then
        return not self.storage:isreadonly(self.uri)
    end
    return false
end

function FileBrowseNode:invoke(name, args, arg2, arg3)
    local ret = false
    if name == "Editable" then
        local source = self.storage:choosefile()
        if source then
            self.storage:filecopy(source, self.uri)
            ret = self
        end
    elseif name == "Renamable" then
        local name = args[1]
        if name:sub(-4) ~= ".lua" then name = name .. ".lua" end
        local status = self:rename(name, true)
        if status then ret = self end
    elseif name == "Deletable" then
        ret = self.storage:delete(self.uri)
        if ret == nil then ret = false end
    end
    return ret, {0}, {uno.Any("void", nil)}
end

----------------------------------------
--- Browse node for directory.
local DirBrowseNode = {}
DirBrowseNode.__index = DirBrowseNode
setmetatable(DirBrowseNode, {__index = BaseNode})
uno.asuno(DirBrowseNode)
uno.addtypeinfo(DirBrowseNode)

--- Node for directory.
-- @param name name for UI
-- @param uri location
-- @param storage storage instance
-- @param ctx component context
-- @param context location context
-- @return new instance
function DirBrowseNode.new(name, uri, storage, ctx, context)
    local obj = BaseNode.new(name, uri, storage)
    obj.ctx = ctx
    obj.context = context
    return setmetatable(obj, DirBrowseNode)
end

function DirBrowseNode:getChildNodes()
    local ret = {}
    local storage = self.storage
    if storage:exists(self.uri) then
        local ctx = self.ctx
        local context = self.context
        local insert = table.insert
        local _uri
        for _, name in ipairs(storage:dircontents(self.uri)) do
            _uri = URI.new(ctx, context, name)
            if storage:isdir(_uri) then
                insert(ret, DirBrowseNode.new(
                        _uri:basename(), _uri, storage, ctx, context))
            elseif name:sub(-4) == ".lua" then
                insert(ret, FileBrowseNode.new(
                        _uri:basename(".lua"), _uri, storage))
            end
        end
    end
    return ret
end

function DirBrowseNode:getPropertyValue(name)
    if name == "Renamable" or name == "Deletable" or name == "Creatable" then
        return (self.context == "user" or self.uri:istransient()) 
               and not self:isreadonly()
    end
    return false
end

function DirBrowseNode:invoke(name, args, arg2, arg3)
    local ret = false
    if name == "Creatable" then
        local node
        local name = args[1]
        if name:sub(-1) == "/" then
            name = name:sub(1, -2)
            local uri = self.uri:create(self.uri:join(name))
            if self.storage:dircreate(uri) then
                node = DirBrowseNode.new(
                        name, uri, self.storage, self.ctx, self.context)
            end
        else
            name = name:sub(-4) == ".lua" and name or name .. ".lua"
            local uri = self.uri:create(self.uri:join(name))
            if self.storage:filecreate(uri, name) then
                if name:sub(-4) == ".lua" then name = name:sub(1, -5) end
                node = FileBrowseNode.new(name, uri, self.storage)
            end
        end
        ret = node
    elseif name == "Renamable" then
        local name = args[1]
        local status = self:rename(name)
        if status then ret = self end
    elseif name == "Deletable" then
        ret = self.storage:delete(self.uri)
        if ret == nil then ret = false end
    end
    return ret, {0}, {uno.Any("void", nil)}
end

----------------------------------------
--- Package management
local PackageManager = {}

--- Extends script provider with package management.
-- @param obj instance of script provider
-- @return passed object
function PackageManager.extend(obj)
    copytable(PackageManager, obj)
    -- ToDo share or other context?
    obj.filepath = uno.tosystempath(obj.ctx:getServiceManager():
        createInstanceWithContext(
        "com.sun.star.util.PathSubstitution", obj.ctx):substituteVariables(
        "$(user)/Scripts/extensions-lua.txt", true))
    obj.packages = nil
    obj:load()
    return obj
end

--- Makes uri ends with /.
-- @param uri URL to check
-- @return URL ends with /
function PackageManager:checkuri(uri)
    return uri:sub(-1) == "/" and uri or uri .. "/"
end

--- Checks package specified by URL is registered.
-- @param uri URL to package
-- @return true if registered, otherwise false
function PackageManager:has(uri)
    uri = self:checkuri(uri)
    return self.packages[uri] ~= nil
end

--- Add new package to register.
-- @param uri URL to package
-- @return nil
function PackageManager:add(uri)
    uri = self:checkuri(uri)
    -- ToDo check Lua script is there?
    self.packages[uri] = true
    self:store()
end

--- Remove package to unregister.
-- @param uri URL to package
-- @return nil
function PackageManager:remove(uri)
    uri = self:checkuri(uri)
    if self.packages[uri] then
        self.packages[uri] = nil
        self:store()
    end
end

--- Checks the package contains Lua script.
-- @param uri URL to package
-- @return true if Lua script found, otherwise false
function PackageManager:hasscripts(uri)
    local status = false
    local storage = self.storage
    if storage:exists(uri) then
        for _, name in ipairs(storage:dircontents(uri)) do
            if storage:isdir(name) then
                status = self:hasscripts(name)
            elseif name:sub(-4) == ".lua" then
                status = true
            end
            if status then break end
        end
    end
    return status
end

--- Load list of registered packages.
-- @return nil
function PackageManager:load()
    local packages = {}
    local f = io.open(self.filepath, "r")
    if f then
        local _uri
        for line in f:lines() do
            line = line:match("^%s*(.-)%s*$")
            if line ~= "" then
                _uri = URI.expand(self, line)
                packages[line] = self:hasscripts(_uri)
            end
        end
        f:close()
    end
    self.packages = packages
end

--- Store list of registered packages.
-- @return nil
function PackageManager:store()
    local insert = table.insert
    local keys = {}
    for k, _ in pairs(self.packages) do
        insert(keys, k)
    end
    local s = table.concat(keys, "\n")
    local f = io.open(self.filepath, "w")
    if f then
        f:write(s); f:flush(); f:close()
    end
end

----------------------------------------
--- Browse node for packages.
local PackageBrowseNode = {}
PackageBrowseNode.__index = PackageBrowseNode
setmetatable(PackageBrowseNode, {__index = BaseNode})
uno.asuno(PackageBrowseNode)
uno.addtypeinfo(PackageBrowseNode)

--- Node for extension package.
-- @param name name for UI
-- @param uri location for scripts
-- @param ctx component context
-- @param context location context
-- @param manager package manager
-- @return new instance
function PackageBrowseNode.new(name, uri, storage, ctx, context, manager)
    local obj = BaseNode.new(name, uri, storage)
    obj.ctx = ctx
    obj.context = context
    obj.manager = manager
    return setmetatable(obj, PackageBrowseNode)
end

function PackageBrowseNode:getChildNodes()
    local ret = {}
    local storage = self.storage
    local insert = table.insert
    local uri
    for package, _ in pairs(self.manager.packages) do
        if storage:exists(package) then
            uri = URI.new(self.ctx, self.context, package)
            insert(ret, DirBrowseNode.new(
                uri:basename(".lua"), uri, storage, self.ctx, self.context))
        else
            logger:error(
                "PackageBrowseNode.getChildNodes, not found: "..package)
        end
    end
    return ret
end

----------------------------------------
--- Script provider for Lua.
local LuaScriptProvider = {}
LuaScriptProvider.__index = LuaScriptProvider
setmetatable(LuaScriptProvider, {__index = BaseNode})
LuaScriptProvider.__unotypes = {
    "com.sun.star.script.provider.XScriptProvider", 
    "com.sun.star.container.XNameContainer"
}
LuaScriptProvider.__implename = "mytools.script.provider.ScriptProviderForLua"
LuaScriptProvider.__servicenames = {
    "com.sun.star.script.provider.ScriptProviderForLua", 
    "com.sun.star.script.provider.LanguageScriptProvider"}
uno.asuno(LuaScriptProvider)
uno.addtypeinfo(LuaScriptProvider)
uno.addserviceinfo(LuaScriptProvider)

--- Create new instance of script provider for Lua.
-- @param ctx component context
-- @param args arguments to initialize
function LuaScriptProvider.new(ctx, args)
    local obj = setmetatable(BaseNode.new(), LuaScriptProvider)
    obj.nodetype = BrowseNodeTypes.ROOT
    obj.name = LANGUAGE
    
    local doc, inv
    local context
    local ispackage = false
    
    if type(args[1]) == "string" then
        context = args[1]
        ispackage = context:sub(-13) == ":uno_packages"
        if context:sub(1, 18) == "vnd.sun.star.tdoc:" then
            doc = getdocumentmodel(ctx, context)
        end
    else
        inv = args[1]
        doc = inv.ScriptContainer
        context = getdocumentcontext(ctx, doc)
    end
    obj.ctx = ctx
    obj.scriptcontext = ScriptContext.new(ctx, doc, inv)
    obj.uri = URI.new(ctx, context, nil)
    obj.storage = Storage.new(ctx)
    if ispackage then
        PackageManager.extend(obj)
        obj.node = PackageBrowseNode.new(
                LANGUAGE, obj.uri, obj.storage, ctx, context, obj)
    else
        obj.node = DirBrowseNode.new(
                LANGUAGE, obj.uri, obj.storage, ctx, context)
    end
    logger:debug("LuaScriptProvider, new: context: "..tostring(context)
                                    ..", uri: "..tostring(obj.uri))
    return obj
end

function LuaScriptProvider:getChildNodes() return self.node:getChildNodes() end

function LuaScriptProvider:getScript(uri)
    local url, funcname = self.uri:tourl(uri)
    uri = URI.new(self.ctx, self.uri.context, url)
    local status, script = xpcall(LuaScripts.getscript, tracebackmsgh, 
                            LuaScripts, self.storage, uri, funcname, 
                            self.scriptcontext)
    if not status then
        local message = tostring(script)
        error(ScriptFrameworkErrorException(message, nil, uri, 
            LANGUAGE, 0))
    end
    return script
end

function LuaScriptProvider:invoke(name, params, arg2, arg3)
    if name == "Creatable" then
        return self.node:invoke(name, params, arg2, arg3)
    end
    return false, {0}, {uno.Any("void", nil)}
end

function LuaScriptProvider:getPropertyValue(name)
    return self.node:getPropertyValue(name)
end

-- XNameContainer
function LuaScriptProvider:insertByName(name, element)
    if self.packages ~= nil then
        self:add(name, element)
    end
end

function LuaScriptProvider:removeByName(name)
    if self.packages ~= nil then
        self:remove(name)
    end
end

function LuaScriptProvider:hasByName(name)
    if self.packages ~= nil then
        return self:has(name)
    end
    return false
end

function LuaScriptProvider:replaceByName(name, element) end
function LuaScriptProvider:getByName(name) return nil end
function LuaScriptProvider:getElementNames() return {} end
function LuaScriptProvider:getElementType() return uno.Type("void") end
function LuaScriptProvider:hasElements() return false end

IMPLEMENTATIONS = {{
    LuaScriptProvider.new, 
    LuaScriptProvider.__implename, 
    LuaScriptProvider.__servicenames
}}

logger:debug("LuaScriptProvider, execution done")
return {}
