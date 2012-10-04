# IPC

UNO supports inter process connection to automate it through TCP or named pipe.

Detailed instructions can be found in the reference listed below.

## Contents

* [Environment Variables](#environment-variables)
    * [URE_BOOTSTRAP](#ure_bootstrap)
    * [`LD_LIBRARY_PATH` or PATH](#ld_library_path-or-path)
* [Parameters](#parameters)
* [Connection](#connection)
* [Local Context and Remote Context](#local-context-and-remote-context)
* [Named Pipe](#named-pipe)
* [IPC Example](#ipc-example)
* [Reference](#reference)

## Environment Variables

Loading the extension requires libraries of the office. And variables for bootstrapping 
is required. They can be specified by environmental variables as follows.

### URE_BOOTSTRAP

This variable should specify fundamental(rc|.ini) file containing type informations. 
It have to starting with `vnd.sun.star.pathname` protocol. Here is an example: 

    export URE_BOOTSTRAP=vnd.sun.star.pathname:file:///opt/openoffice.org3/program/fundamentalrc

It might be as follows on Windows environment:

    set URE_BOOTSTRAP=vnd.sun.star.pathname:file://C:/Program%2FFiles/OpenOffice.org/program/fundamental.ini


### `LD_LIBRARY_PATH` or PATH

Libraries of the office should be found at loading the extension. Depending of 
your environment, set `LD_LIBRARY_PATH` or PATH variable. Here is an example: 

    "/opt/openoffice.org3/program/../basis-link/program:/opt/openoffice.org3/program/../basis-link/ure-link/lib"

It might be as follows on Windows environment:

    set PATH=%PATH%;C:\Program Files\OpenOffice.org\URE\bin;C:\Program Files\OpenOffice.org\program


## Parameters

The office have to be started with `accept` option with specific arguments 
to allow to connect to your office instance through TCP or named pipe.

    # socket connection
    soffice '-accept=socket,host=localhost,port=2083;urp;'
    # named pipe
    soffice '-accept=pipe,name=luapipe;urp;'

See the reference for valid arguments.

ToDo StarOffice.ServiceManager is required?

## Connection

Start the office or start the office in your script of connecting to the office.

Here is an example to connect to the office with specific parameters:

```lua
local function connect(data, bridgename)
    local description, protocol, object
    if type(data) == "string" then
        if data:sub(1, 4) == "uno:" then data = data:sub(5) end
        description, protocol, object = data:match("^([^;]*);([^;]*);([^;]*)$")
        if not description or not protocol or not object then
            error("Illegal UNO URL: " .. tostring(data)) end
        if not bridgename then bridgename = "" end
    elseif type(data) ~= "table" then
        data = {type = "pipe", name = "luapipe", 
                protocol = "urp", object = "StarOffice.ComponentContext"}
     -- data = {type = "socket", host = "localhost", port = 2002, 
     --         protocol = "urp", object = "StarOffice.ComponentContext"}
    end
    if type(data) == "table" then
        local function get(tbl, name, default)
            if tbl[name] == nil then return default else return tbl[name] end
        end
        
        local contype = get(data, "type", "pipe")
        if contype == "pipe" then
            description = "pipe,name=" .. tostring(get(data, "name", "luapipe"))
        elseif contype == "socket" then
            description = "socket,host=" .. tostring(get(data, "host", "localhost")) .. 
                          "port=" .. tostring(get(data, "port", 2002))
        else
            error("Unknown connection type: " .. tostring(contype))
        end
        protocol = get(data, "protocol", "urp")
        object = get(data, "object", "StarOffice.ComponentContext")
        bridgename = get(data, "bridgename", "")
    end
    
    local local_ctx = uno.getcontext()
    local local_smgr = local_ctx:getServiceManager()
    local connector = local_smgr:createInstanceWithContext(
                            "com.sun.star.connection.Connector", local_ctx)
    local connection = connector:connect(description)
    local factory = local_smgr:createInstanceWithContext(
                            "com.sun.star.bridge.BridgeFactory", local_ctx)
    local bridge = factory:createBridge(bridgename, protocol, connection, nil)
    return bridge:getInstance(object)
end
```

This can be used like as follows:

```lua
-- pass parameters in string, no default value is applied
local ctx = connect "uno:pipe,name=luapipe;urp;StarOffice.ComponentContext"

-- or in table, default value is used for object and protocol
local ctx = connect{type = "socket", host = "localhost", port = 2082}
```

## Local Context and Remote Context

The `getcontext` method returns initialized context of the extension. When 
it is done in out of the office, it is initialized with local context. 
This is the different things from remote context which can be taken 
through com.sun.star.bridge.UnoUrlResolver service or other bridge. 
They are not compatible each other to instantiate service.

## Named Pipe

When connected with named pipe, sometimes segmentation fault causes 
at the finish of the script. Add the following statement: 

    os.exit(true)

## IPC Example

Using above `connect` function, here is an example to make new document:

```lua
local uno = require("uno")

local function doc_example()
    local ctx = connect()
    local smgr = ctx:getServiceManager()
    local desktop = smgr:createInstanceWithContext("com.sun.star.frame.Desktop", ctx)
    local doc = desktop:loadComponentFromURL("private:factory/swriter", "_blank", 0, {})
    local text = doc:getText()
    text:setString("Hello from Lua!")
end

doc_example()
os.exit(true)
```

## Reference

* [IPC](http://wiki.services.openoffice.org/wiki/Documentation/DevGuide/ProUNO/UNO_Interprocess_Connections)
