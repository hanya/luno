# Components

UNO components can be written in Lua using component loader for Lua. 

## Contents

* [UNO Component](#uno-component)
    * [Passive Registration](#passive-registration)
    * [Require Function](#require-function)
* [Logging](#logging)
* [Reference](#reference)

## UNO Component

UNO component looks like class and its instance in some language. 
But Lua does not built-in class system and it uses table as 
dummy class system. As described in the LUNO section, it should be 
registered before to pass it to UNO using `uno.asuno` method.

And also, an instance of UNO component have to be tell which kind 
of method group means interface, are supported by the component. 
There is helper function provided in `uno` module.

Using these methods, a component can be written as follows: 

```lua
-- job.lua
local uno = require("uno")

-- As metatable
local JobExecutor = {}
JobExecutor.__index = JobExecutor

-- Register metatable to be converted to UNO component
uno.asuno(JobExecutor)

-- Prepare type information for uno.addtypeinfo method
JobExecutor.__unotypes = {
    "com.sun.star.task.XJobExecutor"
}
-- Add methods in com.sun.star.lang.XTypeProvider interface
-- according to values in __unotypes field
uno.addtypeinfo(JobExecutor)

-- Prepare service informations for uno.addserviceinfo method
JobExecutor.__implename = "mytools.task.TestJobExecutor"
JobExecutor.__servicenames = {
    "mytools.task.TestJobExecutor", 
}
-- Add methods in com.sun.star.lang.XServiceInfo interface 
-- according to __implename and __servicenames fields
uno.addserviceinfo(JobExecutor)

-- Instantiate component
-- Constructor function should take component context and 
-- arguments for createInstanceWithContext and 
-- createInstanceWithArgumentsAndContext of 
-- com.sun.star.lang.XSingleComponentFactory interface.
function JobExecutor.new(ctx, args)
    return setmetatable({ctx = ctx, args = args}, JobExecutor)
end

-- com.sun.star.task.XJobExecutor
function JobExecutor:trigger(event)
    print(event)
end

-- Informations about component provided by this file
-- IMPLEMENTATIONS variable should not be local
IMPLEMENTATIONS = {
    {
        JobExecutor.new, 
        JobExecutor.__implename, 
        JobExecutor.__servicenames
    }, 
    -- another information if there
}
return nil
```

In this case, it is prepared to be registered into type registry of 
the UNO to be instantiated by the UNO if required. The component 
should provide service and implementation information.
In the case of simple component like a listener, that is not required 
to be registered to central type registry because it is not instantiated 
from out of specific code, it does not require 
com.sun.star.lang.XServiceInfo interface. 

The component loader should know which is the constructor defined 
in the file and its additional information about like supported 
service names. For this task, define `IMPLEMENTATIONS` variable 
in module scope with table value. The table should be have 
list of tables and each member defines constructor function, 
implementation name and table of supported service names like in the 
above example.

### Passive Registration

The above `job.lua` file should be packed in extension package (OXT 
package). It should have additional information about to tell 
which file contains UNO component implementations. 

```xml
<!-- LuaJobTest.components -->
<?xml version="1.0" encoding="UTF-8"?>
<components xmlns="http://openoffice.org/2010/uno-components">
  <component loader="com.sun.star.loader.Lua" uri="job.lua">
    <implementation name="mytools.task.LuaJobTest">
      <service name="mytools.task.LuaJobTest"/>
    </implementation>
  </component>
</components>
```

`loader` should be "com.sun.star.loader.Lua" in `components` node. 
And `uri` specifies script file that contains component implementation. 
`name` in `implementation` node should be matched with the one in 
the code. `servicename`s should be similar.

Place the above data into a file and it should be specified in 
META-INF/manifest.xml file as follows:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<manifest:manifest>
  <manifest:file-entry manifest:full-path="LuaJobTest.components" 
   manifest:media-type="application/vnd.sun.star.uno-components"/>
</manifest:manifest>
```

## Require Function

In most case, `package.path` and `package.cpath` contains current 
directory as place to find files to import by `require` function. 
But files define components are not executed as main chunk but it 
loaded by Lua script living in the part of UNO process. 
This is bad to import to custom files from local package in the 
extension package that provides UNO component implementations. 
For this reason, the loader replaces original `require` function with 
custom one that knows origin of the directory that contains 
file which implements the component.

Search order of custom require function is, 
$(pwd) is path to executed script for component: 

* loaded: already loaded and kept by the custom require function
* package.preload: loader for pre-loaded package
* path: custom path for lua file, this includes $(pwd)/lua/?.lua;$(pwd)/lua/?/init.lua;$(pwd)/?.lua;$(pwd)/?/init.lua
* cpath: custom path for C extension, this includes $(pwd)/lua/.$(ext);$(pwd)/lua/?/loadall.$(ext);$(pwd)/?.$(ext);$(pwd)/?/loadall.$(ext)
* require: original require function

## Logging

The component loader tries to load Lua scripts that implements UNO 
components. The loader says nothing even the script has syntax error 
or other execution error while loading the script. 
There is loggin function provided to print or store such information 
to console or a file.

To use this thing, set environmental variables: 

* LUNO_LOG: specifies log level. NOTSET, DEBUG, INFO or ERROR.
* LUNO_LOG_FILE: specifies file to store log messages.

## Reference

* [Extensions](#http://wiki.openoffice.org/wiki/Documentation/DevGuide/Extensions/Extensions)

