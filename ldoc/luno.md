% Basics

This article describes about basics to use the extension. 
The knowledge of UNO basic required to understand well.

## Contents

* [Type Mappings](#type-mappings)
    * [Char](#char)
    * [Type](#type)
    * [Enum](#enum)
    * [Struct and Exception](#struct-and-exception)
    * [Any](#any)
    * [Sequence](#sequence)
    * [Interface](#interface)
* [Proxy](#proxy)
    * [Methods](#methods)
    * [Attributes](#attributes)
    * [Properties](#properties)
* [Exception](#exception)
* [Import Values](#import-values)
* [Functions](#functions)
    * [absolutize](#absolutize)
    * [tourl](#tourl)
    * [tosystempath](#tosystempath)
    * [uuid](#uuid)
    * [getcontext](#getcontext)
    * [require](#require)
    * [typename](#typename)
    * [dir](#dir)
    * [name](#name)
    * [asuno](#asuno)
* [Implement Interfaces](#implement-interfaces)

## Load Module

Load the extension module with `require` function.

    local uno = require("uno")

Current uno.lua is wrapper module for luno.so.

## Type Mappings

The following table shows type conversion map between Lua and UNO.

     UNO        Lua   
    ----------------------------
     void       nil  
     boolean    boolean  
     byte       number (1)
     short      number (1)
     u short    number (1)
     long       number (1)
     u long     number (1)
     hyper      number (1)
     u hyper    number (1)
     float      number (2)
     double     number 
     string     string 
     char       userdata  
     type       userdata 
     enum       userdata 
     struct     userdata 
     exception  userdata
     interface  userdata/table (3)
     sequence   userdata/table (4)

  - 1: Internally, converted into integer as `lua_Integer`.
  - 2: Error causes during cast from float to double, if internal `lua_Number` is defined as double.
  - 3: A table can be passed as UNO component with registration, see [`asuno`](#asuno).
  - 4: Sequence comes from UNO is wrapped as userdata.

Non primitive types are wrapped by userdata.

### String

Pass ASCII 7bit or UTF-8 encoded string to UNO.

### Char

The char type is a char in unicode. 

    uno.Char(char)  ->  userdata

`char` is a character in string. Returns new userdata wraps char data. 
When length of `char` is not 1, error causes.

Resulting value has `value` member field represents wrapped char. 
The result is immutable.

### Type

The type represents one of UNO type.

    uno.Type(typename)  ->  userdata

`typename` is one of UNO type. Returns new userdata wraps type data. 
The sequence type can be represent with `[]`, e.g. `[]long` or `[][]string`.
When `type_name` is invalid, error causes.

Resulting value has `type_name` member field represents wrapped type name. 
The result is immutable.

Here are examples:

    local t = uno.Type("long")
    local t2 = uno.Type("[]long")
    local t3 = uno.Type("[][]double")
    local t4 = uno.Type("com.sun.star.beans.PropertyValue")
    local t5 = uno.Type("[]com.sun.star.beans.PropertyValue")

### Enum

The enum is a value of enum member.

    uno.Enum(typename, value)  ->  userdata

The `typename` is name of enum and `value` specifies one of the enum member name. 
Returns new userdata wraps enum data. 
When `type_name` or `value` is invalid, error causes.

Resulting value has `type_name` member field represents wrapped type name. 
And `value` field represents wrapped value name. 
The result is immutable.

### Struct and Exception

The structure. First, generate constructor function as follows: 

    local Size = require("com.sun.star.awt.Size")

Then new instance can be created with the functions.

    local size = Size()
    -- or with initial values
    -- Width, Height
    local s = Size(100, 200)

The function takes zero or more parameters depending of the struct definition. 
The order of the parameters are the same with the definition of the struct. 
The parameters defined in the base struct should be comes first.

When any values passed to the function is invalid, error causes.

The generated constructor has struct name as upvalue represents type name of the structure 
is instantiated at calling it. It can be read by `name` method of the extension.

Resulting of calling constructor function is userdata wraps struct value. 
It has `type_name` field represents name of the struct.

The member of the struct can be accessed with `.` operator.

    local size = Size()
    size.Width = 100
    size.Height = 200

### Any

A typed value that can be represented in UNO.

    uno.Any(type, value)  ->  userdata

Passed `value` is converted to UNO type represented by `type`. 
`type` can be name of UNO type in string or instance of Type. 

When `type` is invalid or value can not be converted to the specified type, error causes.

The result is immutable.

This type of value is required to pass exactly typed value. In general, this is not 
required since the bridge and the invocation can convert value to correct type. 
But a function takes `any` type of UNO as its parameter and it should be typed well, 
conversion problem causes or simply nothing happen. In such case, use `uno.Any` 
to convert a value to well typed value in UNO.

In the following case, third argument of the `Command` struct should be 
typed as `[]com.sun.star.beans.Property`. If it is passed as lua table to the function, 
it converted into `[]any`, and then `execute` method of the content provider 
failed to convert the sequence of `any` to `[]com.sun.star.beans.Property`. 
Use `uno.Seq` to make typed sequence in this case.

    local Property = uno.require("com.sun.star.beans.Property")
    local Command = uno.require("com.sun.star.task.Command")
    local doc
    local ucb = ctx:getServiceManager().createInstanceWithArgumentsAndContext(
            "com.sun.star.ucb.UniversalContentBroker", {"Local", "Office"}, ctx)
    local identifier = ucb.createContentIdentifier(uri)
    local content = ucb.queryContent(identifier)
    local property = Property("DocumentModel", -1, uno.Type("void"), 1)
    local command = Command("getPropertyValues", -1, 
                        uno.Seq("[]com.sun.star.beans.Property", {property}))
    local commandenv = CommandEnvironment.new()
    local resultset = content:execute(command, 0, commandenv)
    doc = resultset:getObject(1, nil)

When is `Any` required? In general, most of values are correctly converted 
into desired type. But rerely conversion failed, use it for such case. 

In another case, instance of `table` is used to make UNO component and 
it might be have property or attribute. If its value is `nil`, the bridge 
can not be determin the property or attribute is really there or not. 
Set the result of `uno.Any("void", nil)` for its value, it is not nil 
but it is converted to `void` by the bridge.


### Sequence

The sequence of values.

    uno.Seq(typename [, length | table])  ->  userdata

Returns typed sequence specified with `typename`. `length` specifies initial length 
of generated sequence. When `table` is passed, generated sequence initialized with its 
contenst. 
Returns new userdata wraps sequence data.

When `typename` is illegal, length is smaller than 0 or table contains a value that 
cannot be converted to UNO value, error causes.

The resulting userdata having following function fields.

As difference from Lua's table, reading an element from the sequence requests valid index 
 up to length of the sequence. When invalid index is specified, error causes. 
And setting an element is also causes error for index out of bounds of the sequence.

This behavior is choosen because of `void` value problem with sequence.

#### __len

`#` operator can be used to get length of the sequence.

    #seq  ->  number

#### __index and __newindex

`[n]` can be used to get/set content of the sequence with integer index. The index 
starting with 1 to match with Lua's table.

    seq[1]  ->  value
    seq[1] = value  ->  nil

#### __pairs and __ipairs

`pairs` or `ipairs` function can be used to iterate content of the sequence. Result 
of the both functions are the same iteration because of the Sequence can not have 
non-integer keys.

    ipairs(seq)  ->  iterator, sequence, number

This can be used like for table:

    for k, v in pairs(seq) do
        print(v)
    end

`__pairs` and `__ipairs` meta functions needs Lua 5.2 to be worked.

#### __eq

`==` operator can be used to compare two sequences.

    (seq == other)  ->  boolean

#### realloc

Changes the size of the sequence.

    uno.Seq:realloc(length)  ->  nil

When `length` is smaller than 0, error causes.

#### insert

Insert an element to specific index and move elements to make a seat 
for inserted value.

    uno.Seq:insert([pos, ] value)  ->  nil

When `pos` is not specified, `value` is appended to the end of the sequence.
When `pos` is smaller than 1, error causes. When `pos` is larger than length 
of the sequence, error causes.

#### remove

Remove an element specified by index and move back elements to fill the hole.

    uno.Seq:remove([pos])  ->  value

When `pos` is not specified, last element is removed from the sequence. And 
returns removed value. When `pos` is smaller than 1, error causes. When 
`pos` is larger than length of the sequence, error causes. 
When the sequence is empty, error causes.

#### totable

Convert contents into new table of Lua.

    uno.Seq:totable()  ->  table

#### tostring

Convert []byte contents into string.

    uno.Seq:tostring()  ->  string

`seq` must be a sequence typed as []byte. If the argument is not a sequence, 
error causes.

#### tobytes

Convert string to sequence represents []byte.

    uno.Seq.tobytes(string)  ->  uno.Seq

`string` is converted to sequence typed as []byte. Returns new Seq with []type. 

Returned sequence is not suite to modify as bytes. Use this function to wrap 
the string before to pass to UNO methods only.


### Interface

An interface from UNO is wrapped as userdata which works as proxy. Methods, 
attributes and properties can be accessed through `__index` and `__newindex` 
metamethods.



### Proxy

Objects come from UNO such as interface and struct have different methods, properties, 
attributes and fields. They are accessed through metamethod call `__index` and 
`__newindex`.

The proxy uses com.sun.star.script.XInvocation2 interface to call methods or 
to get or set something from the objects. When the target object implements 
com.sun.star.script.XInvocation interface, switched to 
com.sun.star.beans.XIntrospectionAccess based proxy. This should not influence 
anything for user.

#### Methods

If an object supports an interface, its methods and methods of its base interface 
can be called. Use `:` syntax sugar to call method on the proxy.

    local smgr = ctx:getServiceManager()
    -- this is also allowed
    local f = ctx.getServiceManager
    local smgr2 = f()

When a method has out or inout parameter, it returns multiple return values 
in the order that real return value as first return value and then other 
return values for out or inout parameters are follows. Here is an example 
to call the method has out parameter:

    -- long readBytes( [out] sequence< byte > data, [in] long length )
    local b = "test text"
    local pipe = self:create("com.sun.star.io.Pipe")
    pipe:writeBytes(uno.Seq.tobytes(b))
    pipe:flush()
    pipe:closeOutput()
    -- n takes length that really read, d takes value for data parameter
    local n, d = pipe:readBytes(nil, 100)
    local v = d:tostring()
    pipe:closeInput()

When the name of a method confluct with another method from different interface, 
it can be specified with full qualified name as follows:

    local ctx = self:getcontext()
    local smgr = ctx:com_sun_star_uno_XComponentContext_getServiceManager()

#### Attributes

Some interface has attribute definition. They can be accessed with its name as key.

### Properties

Real property is defined through com.sun.star.beans.XPropertySet or similar kind of 
interfaces. They can be accessed with its name as key. And they can be accessed 
with com.sun.star.beans.XPropertySet interface.

#### Pseud Properties

The bridge generates puseud properties from methods. If getXXX, setXXX or isXXX 
method it there, XXX property can be used too. But this is not real property 
as described above.

This is little bit slow than to call corresponding method at first time.

### Exception

UNO exception can be throwen by calling `error` function with UNO exception 
as its argument. Here is an example to throw RuntimeException: 

    local RuntimeException = uno.require("com.sun.star.uno.RuntimeException")
    error(RuntimeException("Error message", nil))

Exception thrown from UNO method can be trap with `pcall` method. 

    local status, message = pcall(vt.testRuntimeException, vt)
    if status then
        -- non error
        -- ....
    else if message.typename == "com.sun.star.uno.RuntimeException" then
        -- error
        print(message.Message)
    end


## Import Values

Some values are defined in UNO and any user can define and add new value to UNO. 
So the extension import values from UNO by `require` function with custom hook 
registered by the module.

The hook function can be called through the extension directly, 
see (`require`)[#require] function.

The valid name is one of module, service, enum, enum value, constants group, constant, 
struct, and exception. When module, service, enum, or constants group is 
requested, table is created with its name and used as container of its child elements.

When module is imported, an empty table retuned which has `__newindex` as metamethod. 
Any member of the modules are not imported and there is no member in the table at the time. 
Once a member of the module is requested as field access, 
the value is imported and set to the module field, if the request is valid. 
The same value is imported only once on the table.

    -- empty table
    local cssawt = require("com.sun.star.awt")
    
    -- imported and field created
    local FontSlant = cssawt.FontSlant
    
    -- FontSlant is table also and its member can be refered as its field
    local it = FontSlant.ITALIC
    
    -- This is allowed but lookup happens many times.
    local com = require("com")
    local ITALIC = com.sun.star.awt.FontSlant.ITALIC

The service can be requested to import but it is not useful if it does not have 
well defined constructor definitions in its IDL. 

    local ScriptURIHelper = uno.require("com.sun.star.script.provider.ScriptURIHelper")
    local helper = ScriptURIHelper.create(ctx, "Python", "user")

The calling of the constructor defined in the IDL, is performed by 
the createInstanceWithArgumentsAndContext method of 
com.sun.star.lang.XMultiComponentFactory interface with argument checking. 
The first argument for the constructor should be component context.

Optionally, services defined without well defined constructor can be 
instantiated with `create` method if they can be instantiated.

    local Desktop = uno.require("com.sun.star.frame.Desktop")
    local desktop = Desktop.create(ctx)

When additional parameters are passed to `create` method, they are 
passed as arguments for `createInstanceWithArgumentsAndContext` method.

## Functions

Some functions are provided by the extension module.

### absolutize

Makes absolute URL from base URL and relative path.

    uno.absolutize(base, relative)  ->  string

### tourl

Converts system path to URL.

    uno.tourl(systempath)  ->  string

UNO uses URL notation to specify a file even it is placed in the local file system. 
It should be formatted with RFC 1738 or others. 

Invalid characters must be % encoded in the URL, use this function to generate URL 
from system path. This function does not add `file://` protocol for non absolute path.

### tosystempath

Converts URL to system path.

    uno.tosystempath(url)  ->  string

### uuid

Generates UUID version 4 for implementation id.

    uno.uuid()  ->  uno.Seq

Return value is new uno.Seq instance.

### getcontext

Returns component context and initialize extension.

    uno.getcontext()  ->  userdata

On IPC, call this function before you do something with UNO.

### require

Returns something of UNO value.

    uno.require(name)  ->  table | string | number | uno.Enum 

This is substantial function of require hook. Standard `require` function has 
some overhead to lookup somethig from local file system. This function can be used 
to load without the overhead.

### typename

Returns type name of UNO values.

    uno.typename(value)  ->  string

Standard `type` function returns name userdata for UNO values wrapped by userdata. 
But this function can be used with them. For non userdata value, error causes. 
When `value` is not an userdata provided by this extension, string userdata is returned.

Returns one of the following name: 

  - enum
  - char
  - type
  - interface
  - struct
  - exception
  - sequence

Other value might be returnd for value created with uno.Any depending its real content.

### dir

Lists element names of interface or struct.

    uno.dir(value)  ->  table

`value` should be interface, struct or exception of UNO as userdata. 
Returns list of methods, properties, attributes and fields in table.

When `value` is not userdata or it does not have `__elements` field, error causes.

### name

Reads name of struct generated by constructor function of it.

    uno.name(ctor)  ->  string

This reads first upvalue of the passed function, so funny result returns for 
any other functions.

When `ctor` is not function, error causes.

### asuno

Register the metatable of the table which is wrapped as UNO component to be passed to UNO.

    uno.asuno(table [, register])  ->  table

`table` should be a table used as metatable and `register` specifies 
to register or to revoke the table, default value is `true`. Returns passed table. 
The passed table is registered into internal table with weak reference. 
If the table is not passed to the UNO, it can be GCed when it is no longer referenced. 

When the table is passed to UNO method or as field value, it is registered internally 
and it lives until it is no longer required in the UNO world. 

UNO components are instantiated for each request like a class of some 
programing languages except for singletons. But Lua does not have built-in 
class mechanism and the set of table and metatable is used to behave 
class and its instantiation. 


## Implements Interfaces

Lua's table can be passed as an instance supports any interface of UNO. But the bridge 
requires special treatment to tell the table works as an UNO component. 
Use `uno.asuno` function to register your metatable of the table to behave UNO 
component, see [`uno.asuno`](#asuno) more detail.

The bridge needs to know what types are supported by the component. For this reason, 
methods of com.sun.star.lang.XTypeProvider interface are used.

Here is an example to implement com.sun.star.awt.XActionListener for dialog buttons.

    local ActionListener = {}
    ActionListener.__index = ActionListener
    -- register the metatable as 
    uno.asuno(ActionListener)
    
    function ActionListener:new()
        -- set metatable to new instance
        return setmetatable({}, ActionListener)
    end
    setmetatable(ActionListener, {__call = ActionListener.new})
    
    -- XTypeProvider
    function ActionListener:getType()
        return uno.Seq("[]type", {
            uno.Type("com.sun.star.awt.XActionListener"), 
            uno.Type("com.sun.star.lang.XTypeClass"), 
        })
    end
    
    function ActionListener:getImplementationId()
        return uno.uuid()
    end
    
    -- XEventListener
    function ActionListener:disposing(ev)
        -- nil for void return value
        -- When a method does not return any value, void is used
        return nil
    end
    
    -- XActionListener
    function ActionListener:actionPerformed(ev)
        print(ev)
        return nil
    end

This `ActionListener` can be used as follows:

    local dp = ctx:getServiceManager():createInstanceWithArguments(
                                "com.sun.star.awt.DialogProvider", ctx)
    local dialog = dp:createDialog(
            "vnd.sun.star.script:Standard.Dialog1?location=application")
    local button1 = dialog:getControl("CommandButton1")
    button1:addActionListener(ActionListener())
    
    dialog:execute()
    dialog:dispose()

If a method has out or inout parameter implemented, it should be returned as 
part of multiple return values. Return real return value first and specify 
values for out parameters in the order defined in the IDL method definition.

    -- boolean testParamMode( [in] long i, [out] long v, [inout] long u );
    
    function bb:testParamMode(i, v, u)
        local b = (i == 100) and (v == nil) and (u == 300)
        return b, 400, 500
        -- b is real return value, 400 is v and 500 is u
    end

