/*
** See Copyright Notice in LICENSE.
**/
#include "luno.hxx"

#include <cppuhelper/typeprovider.hxx>

#include <com/sun/star/beans/MethodConcept.hpp>
#include <com/sun/star/reflection/ParamMode.hpp>

using com::sun::star::beans::UnknownPropertyException;
using com::sun::star::beans::XIntrospectionAccess;

using namespace com::sun::star::lang;
using namespace com::sun::star::reflection;
using namespace com::sun::star::script;
using namespace com::sun::star::uno;

using namespace rtl;

#define OUSTRTOASCII(s) OUStringToOString(s, RTL_TEXTENCODING_ASCII_US).getStr()

    
#define LUNO_ADAPTER_PUSH_WRAPPED(ref) \
    LUNO_PUSH_REG_WRAPPED(); \
    lua_rawgeti(L, -1, ref); \
    lua_replace(L, -2)

#define LUNO_ADAPTER_PUSH_ADAPTERS(ref) \
    LUNO_PUSH_REG_ADAPTERS(); \
    lua_rawgeti(L, -1, ref); \
    lua_replace(L, -2)


namespace luno
{


/** Wraps table at index. 
*/
LunoAdapter::LunoAdapter(lua_State *L_, const int index, 
                        const Any &aTypes, const Any &aImpleId)
    : L(L_), 
      m_WrappedRef(0), 
      m_AdapterRef(0), 
      m_Types(aTypes), 
      m_ImpleId(aImpleId)
{
    const int top = lua_gettop(L);
    // push wrapped value to wrapped
    LUNO_PUSH_REG_WRAPPED();
    lua_pushvalue(L, index);
    m_WrappedRef = luaL_ref(L, -2);
    
    LUNO_PUSH_REG_ADAPTERS();
    lua_pushlightuserdata(L, this);
    m_AdapterRef = luaL_ref(L, -2);
    
    lua_settop(L, top);
}


LunoAdapter::~LunoAdapter()
{
    const int top = lua_gettop(L);
    if (m_WrappedRef)
    {
        LUNO_PUSH_REG_WRAPPED();
        luaL_unref(L, -1, m_WrappedRef);
        m_WrappedRef = 0;
    }
    if (m_AdapterRef)
    {
        LUNO_PUSH_REG_ADAPTERS();
        luaL_unref(L, -1, m_AdapterRef);
        m_AdapterRef = 0;
    }
    lua_settop(L, top);
}


LunoAdapter *LunoAdapter::create(lua_State *L_, const int index, 
                            const Sequence< Type > &aTypes, const Sequence< sal_Int8 > &aImpleId)
{
    // ToDo get types and id at first request
    Any aTypes_;
    Any aImpleId_;
    aTypes_ <<= aTypes;
    aImpleId_ <<= aImpleId;
    
    return new LunoAdapter(L_, index, aTypes_, aImpleId_);
}


Sequence< sal_Int16 > LunoAdapter::getOutParamIndexes(const OUString &aName) 
        throw (RuntimeException)
{
    Sequence< sal_Int16 > ret;

    Runtime runtime;
    Reference< XInterface > rAdapter(
        runtime.getImple()->xAdapterFactory->createAdapter(this, getWrappedTypes()));
    Reference< XIntrospectionAccess > xIntrospectionAcc(
        runtime.getImple()->xIntrospection->inspect(makeAny(rAdapter)));
    if (!xIntrospectionAcc.is())
        throw RuntimeException(
            OUSTRCONST("Failed to create adapter."), Reference< XInterface >());
    
    Reference< XIdlMethod > xIdlMethod(
        xIntrospectionAcc->getMethod(aName, com::sun::star::beans::MethodConcept::ALL));
    if (!xIdlMethod.is())
        throw RuntimeException(
            OUSTRCONST("Failed to get reflection for ") + aName, Reference< XInterface >());
    
    const Sequence< ParamInfo > aInfos(xIdlMethod->getParameterInfos());
    const ParamInfo *pInfos = aInfos.getConstArray();
    
    const int length = aInfos.getLength();
    int out = 0;
    int i = 0;
    for (i = 0; i < length; ++i)
    {
        if (pInfos[i].aMode != ParamMode_IN)
            ++out;
    }
    if (out)
    {
        sal_Int16 j = 0;
        ret.realloc(out);
        sal_Int16 *pOutIndexes = ret.getArray();
        for (i = 0; i < length; ++i)
        {
            if (pInfos[i].aMode != ParamMode_IN)
            {
                pOutIndexes[j] = i;
                ++j;
            }
        }
    }
    return ret;
}


Reference< XIntrospectionAccess > LunoAdapter::getIntrospection()
    throw (RuntimeException)
{
    return Reference< XIntrospectionAccess >();
}


Sequence< Type > LunoAdapter::getWrappedTypes() const
{
    Sequence< Type > aTypes;
    m_Types >>= aTypes;
    return aTypes;
}


// Call lua_gettable in safe mode
static int luno_gettable(lua_State *L)
{
    // 1: table, 2: key
    lua_pushvalue(L, 2); // key
    lua_gettable(L, 1);
    return 1;
}


#define THROW_ON_ERROR(error) \
    if (error) \
    { \
        size_t length; \
        const char *message = lua_tolstring(L, -1, &length); \
        const OUString aMessage(message, length, RTL_TEXTENCODING_UTF8); \
        lua_pop(L, 1); \
        throw RuntimeException(aMessage, Reference< XInterface >()); \
    }


#define CHECK_WRAPPED(ref) \
    if (!ref) \
        throw RuntimeException(OUSTRCONST("Nothing wrapped"), Reference< XInterface >())


Any LunoAdapter::invoke(const OUString &aName, const Sequence< Any > &aParams, 
                    Sequence< sal_Int16 > &aOutParamIndex, Sequence< Any > &aOutParam)
    throw (IllegalArgumentException, CannotConvertException, 
            InvocationTargetException,  RuntimeException)
{
    static const OUString aNameGetSomething(RTL_CONSTASCII_USTRINGPARAM("getSomething"));
    static const OUString aNameGetTypes(RTL_CONSTASCII_USTRINGPARAM("getTypes"));
    static const OUString aNameGetImplementationId(RTL_CONSTASCII_USTRINGPARAM("getImplementationId"));
    
    const int nParams = (int)aParams.getLength();
    if (!nParams)
    {
        // return cached value
        if (aName.equals(aNameGetTypes))
            return m_Types;
        else if (aName.equals(aNameGetImplementationId))
            return m_ImpleId;
    }
    else if (nParams == 1 && aName.equals(aNameGetSomething))
    {
        Sequence< sal_Int8 > id;
        if (aParams[0] >>= id)
            return makeAny(getSomething(id));
    }
    
    Any retAny;
    const int top = lua_gettop(L);
    try
    {
        CHECK_WRAPPED(m_WrappedRef);
        
        Runtime runtime;
        
        LUNO_ADAPTER_PUSH_WRAPPED(m_WrappedRef); // to reuse
        lua_pushcfunction(L, luno_gettable);
        lua_pushvalue(L, -2); // wrapped
        lua_pushstring(L, OUSTRTOASCII(aName));
        const int e = lua_pcall(L, 2, 1, 0);
        THROW_ON_ERROR(e);
        
        if (!lua_isfunction(L, -1))
        {
            // ToDo check is callable?
            lua_settop(L, top);
            throw RuntimeException(
                OUSTRCONST("Method not found: ") + aName, Reference< XInterface >());
        }
        lua_insert(L, top + 1); // wrapped - func -> func - wrapped
        
        // push arguments
        const Any *pParams = aParams.getConstArray();
        for (int i = 0; i < nParams; ++i)
            runtime.anyToLua(L, pParams[i]);
        
        // call, obj + params, var args
        const int error = lua_pcall(L, nParams + 1, LUA_MULTRET, 0);
        if (error)
        {
            // ToDo test throw exception from Lua
            if (error == LUA_ERRRUN)
            {
                if (lua_isuserdata(L, -1))
                {
                    LunoAdapted *p = luno_proxy_getudata(L, -1, LUNO_META_STRUCT);
                    if (p != NULL && p->Wrapped.getValueTypeClass() == TypeClass_EXCEPTION)
                    {
                        lua_settop(L, top);
                        throw InvocationTargetException(OUString(), 
                                    Reference< XInterface >(), p->Wrapped);
                    }
                }
                size_t length;
                const char *message = lua_tolstring(L, -1, &length);
                throw RuntimeException(OUString(message, length, RTL_TEXTENCODING_UTF8), 
                            Reference< XInterface >());
            }
            else
            {
                // ToDo LUA_ERRGCMM
                size_t length;
                const char *s = lua_tolstring(L, -1, &length);
                throw RuntimeException(
                    OUString(s, length, RTL_TEXTENCODING_UTF8), Reference< XInterface >());
            }
        }
        // ignore normal return value and wrapped obj
        const int nOut = lua_gettop(L) - top - 1;
        
        if (lua_gettop(L) - top > 0)
            retAny = runtime.luaToAny(L, top + 1);
        
        if (nOut > 1)
        {
            aOutParamIndex = getOutParamIndexes(aName);
            const int nOutParams = (int)aOutParamIndex.getLength();
            if (nOut != nOutParams)
                throw RuntimeException(
                    OUSTRCONST("luno: illegal number of out values for method: ") + aName, 
                    Reference< XInterface >());
            
            aOutParam.realloc(nOutParams);
            for (int i = 0; i < nOutParams; ++i)
                aOutParam[i] = runtime.luaToAny(L, top + 1 + i);
        }
        lua_settop(L, top);
    }
    catch (RuntimeException &e)
    {
        lua_settop(L, top);
        throw;
    }
    return retAny;
}


// Call lua_settable in safe mode
static int luno_settable(lua_State *L)
{
    // i: table, 2: key, 3: value
    lua_pushvalue(L, 2); // key
    lua_pushvalue(L, 3); // value
    lua_settable(L, 1);
    return 0;
}


void LunoAdapter::setValue(const OUString &aName, const Any &aValue) 
    throw (UnknownPropertyException, CannotConvertException, 
            InvocationTargetException, RuntimeException)
{
    CHECK_WRAPPED(m_WrappedRef);
    
    lua_pushcfunction(L, luno_settable);
    LUNO_ADAPTER_PUSH_WRAPPED(m_WrappedRef);
    lua_pushstring(L, OUSTRTOASCII(aName));
    
    try
    {
        Runtime runtime;
        runtime.anyToLua(L, aValue);
    }
    catch (RuntimeException &e)
    {
        lua_pop(L, 3);
        throw;
    }
    
    const int e = lua_pcall(L, 3, 1, 0);
    THROW_ON_ERROR(e);
}


Any LunoAdapter::getValue(const OUString &aName)
    throw (UnknownPropertyException, RuntimeException)
{
    CHECK_WRAPPED(m_WrappedRef);
    
    Any a;
    
    lua_pushcfunction(L, luno_gettable);
    LUNO_ADAPTER_PUSH_WRAPPED(m_WrappedRef);
    lua_pushstring(L, OUSTRTOASCII(aName));
    
    const int e = lua_pcall(L, 2, 1, 0);
    THROW_ON_ERROR(e);
    
    Runtime runtime;
    a = runtime.luaToAny(L, -1);
    lua_pop(L, 1); // result
    return a;
}


sal_Bool LunoAdapter::hasMethod(const OUString &aName)
    throw (RuntimeException)
{
    sal_Bool b = sal_False;
    CHECK_WRAPPED(m_WrappedRef);
    
    lua_pushcfunction(L, luno_gettable);
    LUNO_ADAPTER_PUSH_WRAPPED(m_WrappedRef);
    lua_pushstring(L, OUSTRTOASCII(aName));
    
    const int e = lua_pcall(L, 2, 1, 0);
    THROW_ON_ERROR(e);
    
    if (!lua_isfunction(L, -1))
        b = sal_True;
    lua_pop(L, 1); // value
    return b;
}


sal_Bool LunoAdapter::hasProperty(const OUString &aName)
    throw (RuntimeException)
{
    CHECK_WRAPPED(m_WrappedRef);
    
    sal_Bool b = sal_False;
    lua_pushcfunction(L, luno_gettable);
    LUNO_ADAPTER_PUSH_WRAPPED(m_WrappedRef);
    lua_pushstring(L, OUSTRTOASCII(aName));
    
    const int e = lua_pcall(L, 2, 1, 0);
    THROW_ON_ERROR(e);
    
    if (!lua_isnil(L, -1))
        b = sal_True;
    lua_pop(L, 1); // value
    return b;
}


static cppu::OImplementationId g_Id(sal_False);


Sequence< sal_Int8 > LunoAdapter::getUnoTunnelImplementationId()
{
    return g_Id.getImplementationId();
}


sal_Int64 LunoAdapter::getSomething(const Sequence< sal_Int8 > &aIdentifier)
    throw (RuntimeException)
{
    if (aIdentifier == g_Id.getImplementationId())
        return reinterpret_cast< sal_Int64 >(this);
    return 0;
}

} // luno
