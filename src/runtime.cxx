/*
** See Copyright Notice in LICENSE.
**/
#include "luno.hxx"

#ifndef TYPE_CACHED
//#define TYPE_CACHED
#endif

#include <string.h>

#include <osl/thread.h>
#include <typelib/typedescription.hxx>

#include <com/sun/star/reflection/XEnumTypeDescription.hpp>
#include <com/sun/star/beans/XMaterialHolder.hpp>

using com::sun::star::beans::XIntrospection;
using com::sun::star::beans::XIntrospectionAccess;
using com::sun::star::beans::XMaterialHolder;
using com::sun::star::beans::XPropertySet;
using com::sun::star::lang::XSingleServiceFactory;
using com::sun::star::lang::XMultiComponentFactory;
using com::sun::star::lang::XUnoTunnel;
using com::sun::star::reflection::XIdlArray;
using com::sun::star::reflection::XIdlClass;
using com::sun::star::reflection::XEnumTypeDescription;
using com::sun::star::reflection::XIdlReflection;
using com::sun::star::script::XInvocation;
using com::sun::star::script::XInvocation2;
using com::sun::star::script::XInvocationAdapterFactory2;
using com::sun::star::script::XTypeConverter;

using rtl::OUString;
using rtl::OUStringToOString;

using namespace com::sun::star::uno;


namespace luno
{

#define LUNO_META_RUNTIME "luno.runtime"
#define LUNO_REG_RUNTIME  "luno.runtime"

static RuntimeImple *gImple;


static int luno_runtime_gc(lua_State *L)
{
    if (gImple != NULL)
    {
        delete gImple;
        gImple = NULL;
    }
    return 0;
}


static const luaL_Reg luno_runtime[] = {
    {"__gc", luno_runtime_gc}, 
    {NULL, NULL}
};


bool Runtime::isInitialized() throw (RuntimeException)
{
    if (gImple != NULL)
        return gImple->bInitialized;
    return false;
}


Runtime::Runtime() throw (RuntimeException)
{
    // ToDo store struct instance into _G or env table
    imple = gImple;
}


Runtime::~Runtime()
{
    imple = NULL;
}


#define CREATE_SERVICE(name) xMcf->createInstanceWithContext(OUSTRCONST(name), xContext)
#define THROWRNTE(message) throw RuntimeException(OUSTRCONST(message), Reference< XInterface >())


void Runtime::initialize(lua_State *L, const Reference< XComponentContext > &xContext) throw (RuntimeException)
{
    if (gImple)
        THROWRNTE("luno: runtime is already initialized.");
    
    gImple = new RuntimeImple();
    
    gImple->xComponentContext = xContext;
    
    Reference< XMultiComponentFactory > xMcf(xContext->getServiceManager());
    
    gImple->xInvocation.set(
            CREATE_SERVICE("com.sun.star.script.Invocation"), UNO_QUERY);
    if (!gImple->xInvocation.is())
        THROWRNTE("luno: failed to instantiate com.sun.star.script.Invocation.");
    
    gImple->xIntrospection.set(
            CREATE_SERVICE("com.sun.star.beans.Introspection"), UNO_QUERY);
    if (!gImple->xIntrospection.is())
        THROWRNTE("luno: failed to instantiate com.sun.star.beans.Instrospection");
    
    Any cr = xContext->getValueByName(
            OUSTRCONST("/singletons/com.sun.star.reflection.theCoreReflection"));
    cr >>= gImple->xCoreReflection;
    if (!gImple->xCoreReflection.is())
        THROWRNTE("luno: failfed to instantiate com.sun.star.reflection.CoreReflection.");
    
    gImple->xTypeConverter.set(
            CREATE_SERVICE("com.sun.star.script.Converter"), UNO_QUERY);
    if (!gImple->xTypeConverter.is())
        THROWRNTE("luno: failfed to instantiate com.sun.star.script.Converter.");
    
    gImple->xAdapterFactory.set(
            CREATE_SERVICE("com.sun.star.script.InvocationAdapterFactory"), UNO_QUERY);
    if (!gImple->xTypeConverter.is())
        THROWRNTE("luno: failed to instantiate com.sun.star.script.InvocationAdapterFactory.");
    
    Any tdm = xContext->getValueByName(
            OUSTRCONST("/singletons/com.sun.star.reflection.theTypeDescriptionManager"));
    tdm >>= gImple->xTypeDescription;
    if (!gImple->xTypeDescription.is())
        THROWRNTE("luno: failed to get /singletons/com.sun.star.reflection.theTypeDescriptionManager.");
    gImple->bInitialized = true;
    
    lua_pushstring(L, LUNO_REG_RUNTIME);
    lua_newtable(L);
    luaL_newmetatable(L, LUNO_META_RUNTIME);
    SET_FUNCS(L, luno_runtime);
    lua_setmetatable(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);
}


static Sequence< Type > getTypes(lua_State *L, const int index, const Runtime &runtime)
{
    const int top = lua_gettop(L);
    Sequence< Type > ret;
    
    lua_pushstring(L, "getTypes");
    lua_gettable(L, index);
    if (lua_isfunction(L, -1))
    {
        lua_pushvalue(L, index);
        const int error = lua_pcall(L, 1, 1, 0);
        if (error)
            return ret;
        
        const int type = lua_type(L, -1);
        if (type == LUA_TUSERDATA)
        {
            LunoSequence *p = luno_seq_getudata(L, -1);
            if (p != NULL)
                p->Wrapped >>= ret;
        }
        else if (type == LUA_TTABLE)
        {
            // ToDo with hole table to luaToAny
            const int length = (int)GET_LENGTH(L, 1);
            ret.realloc(length);
            for (int i = 1; i <= length; ++i)
            {
                lua_rawgeti(L, -1, i);
                Any a = runtime.luaToAny(L, -1);
                a >>= ret[i -1];
                lua_pop(L, 1);
            }
        }
    }
    lua_settop(L, top);
    
    const sal_Int32 nLength = ret.getLength();
    if (nLength)
    {
        ret.realloc(nLength + 1);
        ret[nLength] = getCppuType(
                (Reference< com::sun::star::lang::XUnoTunnel > *)NULL);
    }
    return ret;
}


static Sequence< sal_Int8 > getImplementationId(lua_State *L, const int index, const Runtime &runtime)
{
    const int top = lua_gettop(L);
    Sequence< sal_Int8 > ret;
    
    lua_pushstring(L, "getImplementationId");
    lua_gettable(L, index);
    if (lua_isfunction(L, -1))
    {
        lua_pushvalue(L, index);
        const int error = lua_pcall(L, 1, 1, 0);
        if (error)
            return ret;
        
        if (lua_isuserdata(L, -1))
        {
            LunoSequence *p = luno_seq_getudata(L, -1);
            if (p != NULL)
                p->Wrapped >>= ret;
        }
    }
    lua_settop(L, top);
    return ret;
}


/** Push converted value to Lua stack.
*/
void Runtime::anyToLua(lua_State *L, const Any &a) const throw (RuntimeException)
{
    switch (a.getValueTypeClass())
    {
        case TypeClass_BOOLEAN:
        {
            sal_Bool b = sal_Bool();
            lua_pushboolean(L, ((a >>= b) && b) ? 1 : 0);
            break;
        }
        case TypeClass_BYTE:
        case TypeClass_SHORT:
        case TypeClass_UNSIGNED_SHORT:
        case TypeClass_LONG:
        {
            sal_Int32 n = 0;
            a >>= n;
            lua_pushinteger(L, (lua_Integer)n);
            break;
        }
#if LUA_VERSION_NUM >= 502
        case TypeClass_UNSIGNED_LONG:
        {
            sal_uInt32 n = 0;
            a >>= n;
            lua_pushunsigned(L, (lua_Unsigned)n);
            break;
        }
#else
        case TypeClass_UNSIGNED_LONG:
#endif
        case TypeClass_HYPER:
        case TypeClass_UNSIGNED_HYPER:
        {
            // unsigned hyper loses data
            sal_Int64 n = 0;
            a >>= n;
            lua_pushinteger(L, (lua_Integer)n);
            break;
        }
        case TypeClass_FLOAT:
        case TypeClass_DOUBLE:
        {
            // maybe float has error
            lua_Number n = 0;
            a >>= n;
            lua_pushnumber(L, n);
            break;
        }
        case TypeClass_STRING:
        {
            OUString str;
            a >>= str;
            lua_pushstring(L, OUStringToOString(str, RTL_TEXTENCODING_UTF8).getStr());
            break;
        }
        case TypeClass_TYPE:
        case TypeClass_ENUM:
        case TypeClass_CHAR:
        {
            LunoValue *p = luno_value_newudata(L);
            if (p != NULL)
                p->Wrapped = a;
            else
                throw RuntimeException(
                    OUSTRCONST("Failed to create new userdata"), Reference< XInterface >());
            break;
        }
        case TypeClass_STRUCT:
        case TypeClass_EXCEPTION:
        {
            LunoAdapted *p = luno_proxy_newudata(L, LUNO_META_STRUCT);
            if (p != NULL)
            {
                Sequence< Any > aArguments(1);
                aArguments[0] <<= a;
                Reference< XInvocation2 > xInvocation(
                    getImple()->xInvocation->createInstanceWithArguments(aArguments), UNO_QUERY);
                
                p->Wrapped = a;
                p->xInvocation = xInvocation;
            }
            break;
        }
        case TypeClass_INTERFACE:
        {
            // get wrapped if adapted
            Reference< XUnoTunnel > xUnoTunnel;
            a >>= xUnoTunnel;
            if (xUnoTunnel.is())
            {
                sal_Int64 addr = xUnoTunnel->getSomething(
                            LunoAdapter::getUnoTunnelImplementationId());
                if (addr)
                {
                    LunoAdapter *p = (LunoAdapter*)sal::static_int_cast< sal_IntPtr >(addr);
                    if (p != NULL)
                    {
                        const int ref = p->getWrappedRef();
                        if (ref)
                        {
                            LUNO_PUSH_REG_WRAPPED();
                            lua_rawgeti(L, -1, ref);
                            lua_replace(L, -2); // remove LUNO_REG_WRAPPED
                            break;
                        }
                    }
                }
            }
            
            Reference< XInterface > xInterface;
            a >>= xInterface;
            if (xInterface.is())
            {
                Reference< XInvocation > xInvocationDirect(a, UNO_QUERY);
                if (!xInvocationDirect.is())
                {
                    Sequence< Any > aArguments(1);
                    aArguments[0] <<= a;
                    Reference< XInvocation2 > xInvocation(
                        getImple()->xInvocation->createInstanceWithArguments(aArguments), UNO_QUERY);
                    
                    LunoAdapted *p = luno_proxy_newudata(L, LUNO_META_PROXY);
                    if (p != NULL)
                    {
                        p->Wrapped = a;
                        p->xInvocation = xInvocation;
                    }
                }
                else
                {
                    // Real XInvocation wrapped by invocation
                    Reference< XIntrospectionAccess > xIntrospectionAccess(
                                            getImple()->xIntrospection->inspect(a));
                    
                    LunoAdapted2 *p = luno_proxy_newudata2(L, LUNO_META_PROXY);
                    if (p != NULL)
                    {
                        p->Wrapped = a;
                        // p->xInvocation is invalid reference
                        p->xIntrospectionAccess = xIntrospectionAccess;
                        p->xPropertySet = Reference< XPropertySet >::query(
                            xIntrospectionAccess->queryAdapter(
                                getCppuType((Reference< XPropertySet > *)NULL)));
                    }
                }
                // ToDo check resulting interfaces are valid
            }
            break;
        }
        case TypeClass_SEQUENCE:
        {
            Reference< com::sun::star::reflection::XIdlClass > xIdlClass = 
                        getImple()->xCoreReflection->forName(a.getValueTypeName());
            if (xIdlClass.is() && xIdlClass->getTypeClass() == TypeClass_SEQUENCE)
            {
                Reference< XIdlArray > xIdlArray(xIdlClass, UNO_QUERY);
                LunoSequence *p = luno_seq_newudata(L);
                if (p != NULL)
                {
                    p->Wrapped = a;
                    p->xIdlArray = xIdlArray;
                }
            }
            break;
        }
        case TypeClass_VOID:
        {
            lua_pushnil(L);
            break;
        }
        case TypeClass_ANY:
        {
            lua_pushnil(L);
            break;
        }
        default:
        {
            throw RuntimeException(
                OUSTRCONST("Illegal type ") + a.getValueTypeName(), Reference< XInterface >());
            break;
        }
    }
}


/** Returns converted value from Lua stack without changing the stack.
*/
Any Runtime::luaToAny(lua_State *L, int index) const
{
    Any a;
    switch(lua_type(L, index))
    {
        case LUA_TNIL:
        {
            break;
        }
        case LUA_TNUMBER:
        {
            const lua_Number b = lua_tonumber(L, index);
            a <<= b;
            break;
        }
        case LUA_TBOOLEAN:
        {
            const sal_Bool b = lua_toboolean(L, index) ? sal_True : sal_False;
            a = Any(&b, getBooleanCppuType());
            break;
        }
        case LUA_TSTRING:
        {
            size_t length;
            const char *s = lua_tolstring(L, index, &length);
            a <<= OUString(s, length, RTL_TEXTENCODING_UTF8);
            break;
        }
        case LUA_TTABLE:
        {
            const int nInitialTop = lua_gettop(L);
            // check metatable of the table is registered as UNO component
            LUNO_PUSH_REGISTERED();
            if (!lua_getmetatable(L, index))
                lua_pushvalue(L, index);
            lua_rawget(L, -2);
            
            if (lua_isnil(L, -1))
            {
                lua_pop(L, 2); // result and table
                
                const int length = (int)GET_LENGTH(L, index);
                Sequence< Any > aValues((sal_Int32)length);
                Any *pValues = aValues.getArray();
                for (int i = 1; i <= length; ++i)
                {
                    lua_rawgeti(L, index, i);
                    pValues[i - 1] = luaToAny(L, lua_gettop(L));
                    lua_pop(L, 1); // value
                }
                a <<= aValues;
                lua_settop(L, nInitialTop);
            }
            else
            {
                lua_pop(L, 1); // reuse Registered
#ifdef TYPE_CACHED
                const int nRegisterIndex = lua_gettop(L);
#endif
                
                // as object which can be respond as UNO object, check already wrapped
                Reference< XInterface > xInterfaceMapped;
                LunoAdapter *pAdapted = NULL;
                const int nTop = lua_gettop(L);
                const int nAdaptersIndex = nTop + 1;
                const int nWrappedIndex = nTop + 2;
                
                LUNO_PUSH_REG_ADAPTERS();
                LUNO_PUSH_REG_WRAPPED();
                
                const int length = (int)GET_LENGTH(L, -1);
                for (int i = 1; i <= length; ++i)
                {
                    lua_rawgeti(L, nAdaptersIndex, i);
                    const void *p = lua_topointer(L, -1);
                    lua_pop(L, 1); // value
                    if (p != NULL)
                    {
                        const int ref = ((LunoAdapter *)p)->getWrappedRef();
                        lua_rawgeti(L, nWrappedIndex, ref);
                        if (lua_rawequal(L, index, -1))
                            pAdapted = (LunoAdapter *)p;
                        lua_pop(L, 1);
                    }
                }
                lua_settop(L, nTop);
                
                if (pAdapted == NULL)
                {
#ifdef TYPE_CACHED
                    Sequence< Type > aTypes;
                    Sequence< sal_Int8 > aImpleId;
                    LunoTypes *p = NULL;
                    
                    if (!lua_getmetatable(L, index))
                        lua_pushvalue(L, index);
                    lua_rawget(L, nRegisterIndex);
                    
                    if (!lua_isuserdata(L, -1))
                    {
                        lua_pop(L, 1); // value
                        // as key
                        if (!lua_getmetatable(L, index))
                            lua_pushvalue(L, index);
                        
                        p = luno_types_newudata(L);
                        if (p != NULL)
                        {
                            p->aTypes = getTypes(L, index, *this);
                            p->aImpleId = getImplementationId(L, index, *this);
                            lua_rawset(L, nRegisterIndex);
                            
                            if (!lua_getmetatable(L, index))
                                lua_pushvalue(L, index);
                            lua_rawget(L, nRegisterIndex);
                        }
                        else
                            lua_pop(L, 1);
                    }
                    p = luno_types_getudata(L, -1);
                    if (p != NULL)
                    {
                        aTypes = p->aTypes;
                        aImpleId = p->aImpleId;
                    }
#else
                    Sequence< Type > aTypes(getTypes(L, index, *this));
                    Sequence< sal_Int8 > aImpleId(getImplementationId(L, index, *this));
#endif
                    
                    if (!aTypes.getLength())
                        luaL_error(L, "passed table does not support any UNO types");
                    else if (!aImpleId.getLength())
                        luaL_error(L, "passed table does not have implementataion id");
                    // destructed when refcount reach to 0
                    pAdapted = LunoAdapter::create(L, index, aTypes, aImpleId);
                }
                xInterfaceMapped = imple->xAdapterFactory->createAdapter(
                                        pAdapted, pAdapted->getWrappedTypes());
                a = makeAny(xInterfaceMapped);
                lua_settop(L, nInitialTop);
            }
            break;
        }
        case LUA_TUSERDATA:
        {
            LunoValue *p = *(LunoValue **)lua_touserdata(L, index);
            if (p != NULL)
            {
                if (luno_get_udata_type(L, index) != LUNO_TYPE_UNDEFINED)
                    a <<= p->Wrapped;
                else
                    luaL_error(L, "Failed to detect the type of userdata");
            }
            else
                luaL_error(L, "Illegal userdata passed");
            break;
        }
        default:
            luaL_error(L, "Could not be converted to UNO value (%s)", luaL_typename(L, index));
            break;
    }
    return a;
}

}
