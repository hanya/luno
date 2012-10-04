# Macros

Macro can be written in Lua to automate tasks provided by 
script provider for Lua.

## Contents

* [Script Location](#script-location)
* [File Format](#file-format)
* [Execution Unit](#execution-unit)
* [Require Function](#require-function)
* [XSCRIPTCONTEXT Variable](#xscriptcontext-variable)
* [Script URI](#script-uri)
* [Examples](#examples)


## Script Location

Lua script should be placed under USER_PROFILE/Scripts/lua directory for 
user's macro. When macro embedded in a document it should be stored in 
DOC/Scripts/lua internal directory of the ODF file.

## File Format

Lua macros should be stored as UTF-8 encoded text file with .lua file 
extension.

## Execution Unit

Only file level non-local functions can be executed as macro. Local 
function can not be accessed with their name with normal way to execute.

The script file loaded as macro is executed in almost empty environment. 
Only the following variables are passed at first execution.

    __main__: "sf" -- can be used to detect the script is executed as macro

After the first execution, the environment is filled as almost normal 
Lua environment. But with custom `require` function to be used to 
import something from file local location.

## `require` Function

`require` function is customized to import module from inner document. 
Search order is as follows, $(pwd) is path to executed script for macro: 

* loaded: already loaded and kept by the custom require function
* package.preload: loader for pre-loaded package
* path: custom path with $(pwd)/?.lua;$(pwd)/?/init.lua;$(user_scripts)/?.lua;$(user_scripts)/?/init.lua
* cpath: $(user_scripts)/?$(ext);$(user_scripts)/?/loadall.$(ext)
* require: original require function

## XSCRIPTCONTEXT Variable

This is the connection point between Lua and UNO. See 
com.sun.star.script.provider.XScriptContext of IDL reference more detail.

## Script URI

A macro written in Lua can be specified in general script protocol. 
Use "Lua" as target language. For example: 

    -- foo function in USER_PROFILE/Scripts/lua/bar/abc.lua file
    vnd.sun.star.script:bar|abc.lua$foo?location=user&language=Lua


## Script Editor

No embedded editor provided, use favorite one. Embedding macro into 
document is supported by macro organizer on Lua mode. 
Create new file into the document and push edit button to replace 
the file with existing file from local file system.

## Examples

Here are macro examples:

```lua
-- This local function is not shown in the list of macros and 
-- it can not be executed as macro.
local function greeting()
    return "Hello!"
end

-- For Writer document.
function test_hello()
    -- Get current document model
    local doc = XSCRIPTCONTEXT:getDocument()
    local text = doc:getText()
    text:setString(greeting())
end
-- here is the place that no one can write code with functions
```

## Packaging into Extension

This section is sidenote to pack Lua macro into extension package.

Make a directory in the extension package and put macros into it. 
Note the directory only contains Lua script, no other kind of scripts 
like Java, BeanShell and Python. Add entry for the directory into 
META-INF/manifest.xml file as follows:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<manifest:manifest xmlns:manifest="http://openoffice.org/2001/manifest">
  <manifest:file-entry manifest:media-type="application/vnd.sun.star.framework-script"
                       manifest:full-path="scripts/"/>
</manifest:manifest>
```
