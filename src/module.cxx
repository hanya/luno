
#include "luno.hxx"

#include <string.h>

#include <osl/file.hxx>
#include <rtl/uuid.h>
#include <rtl/ustrbuf.hxx>

#include <cppuhelper/bootstrap.hxx>
#include <typelib/typedescription.hxx>

#include <com/sun/star/beans/MethodConcept.hpp>
#include <com/sun/star/beans/PropertyConcept.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/reflection/ParamMode.hpp>
#include <com/sun/star/reflection/XServiceConstructorDescription.hpp>
#include <com/sun/star/reflection/XServiceTypeDescription2.hpp>

using com::sun::star::beans::Property;
using com::sun::star::script::InvocationInfo;
using com::sun::star::script::XInvocation2;

using namespace com::sun::star::beans;
using namespace com::sun::star::lang;
using namespace com::sun::star::reflection;
using namespace com::sun::star::uno;

using namespace rtl;

using namespace luno;


#define OUSTRTOSTR(s) OUStringToOString(s, RTL_TEXTENCODING_UTF8).getStr()

namespace luno
{


inline void *luno_checked_udata(lua_State *L, int index, const char *metatable)
{
    void *p = NULL;
    if (lua_isuserdata(L, index))
    {
        p = lua_touserdata(L, index);
        if (p != NULL)
        {
            lua_getmetatable(L, index);
            luaL_getmetatable(L, metatable);
            if (!lua_rawequal(L, -1, -2))
                p = NULL;
            lua_pop(L, 2);
        }
    }
    return p;
}


LunoAdapted *luno_proxy_getudata(lua_State *L, const int index, const char *metatable)
{
    void *p = luno_checked_udata(L, index, metatable);
    if (p != NULL)
        return *(LunoAdapted **)p;
    return NULL;
}


LunoAdapted *luno_proxy_newudata(lua_State *L, const char *metatable)
{
    LunoAdapted **p = (LunoAdapted **)lua_newuserdata(L, sizeof(LunoAdapted *));
    *p = new LunoAdapted();
    SET_METATABLE(metatable);
    return *p;
}

LunoAdapted2 *luno_proxy_newudata2(lua_State *L, const char *metatable)
{
    LunoAdapted2 **p = (LunoAdapted2 **)lua_newuserdata(L, sizeof(LunoAdapted2 *));
    *p = new LunoAdapted2();
    SET_METATABLE(metatable);
    return *p;
}


LunoValue *luno_value_getudata(lua_State *L, const int index)
{
    void *p = luno_checked_udata(L, index, LUNO_META_VALUE);
    if (p != NULL)
        return *(LunoValue **)p;
    return NULL;
}


LunoValue *luno_value_newudata(lua_State *L)
{
    LunoValue **p = (LunoValue **)lua_newuserdata(L, sizeof(LunoValue *));
    *p = new LunoValue();
    SET_METATABLE(LUNO_META_VALUE);
    return *p;
}


LunoSequence *luno_seq_getudata(lua_State *L, const int index)
{
    void *p = luno_checked_udata(L, index, LUNO_META_SEQ);
    if (p != NULL)
        return *(LunoSequence **)p;
    return NULL;
}


LunoSequence *luno_seq_newudata(lua_State *L)
{
    LunoSequence **p = (LunoSequence **)lua_newuserdata(L, sizeof(LunoSequence *));
    *p = new LunoSequence();
    SET_METATABLE(LUNO_META_SEQ);
    return *p;
}


LunoTypes *luno_types_getudata(lua_State *L, const int index)
{
    void *p = luno_checked_udata(L, index, LUNO_META_TYPES);
    if (p != NULL)
        return *(LunoTypes **)p;
    return NULL;
}


LunoTypes *luno_types_newudata(lua_State *L)
{
    LunoTypes **p = (LunoTypes **)lua_newuserdata(L, sizeof(LunoTypes *));
    *p = new LunoTypes();
    SET_METATABLE(LUNO_META_TYPES);
    return *p;
}


#define GET_UDATA_TYPE(index, metatable, tc) \
    luaL_getmetatable(L, metatable); \
    eq = lua_rawequal(L, -1, -2); \
    lua_pop(L, 1); \
    if (eq) \
    { \
        lua_pop(L, 1); \
        return tc; \
    }


int luno_get_udata_type(lua_State *L, const int index)
{
    int eq = 0;
    if (!lua_getmetatable(L, index))
        return LUNO_TYPE_UNDEFINED;
    
    GET_UDATA_TYPE(index, LUNO_META_PROXY, LUNO_TYPE_INTERFACE);
    GET_UDATA_TYPE(index, LUNO_META_STRUCT, LUNO_TYPE_STRUCT);
    GET_UDATA_TYPE(index, LUNO_META_SEQ, LUNO_TYPE_SEQ);
    luaL_getmetatable(L, LUNO_META_VALUE);
    eq = lua_rawequal(L, -1, -2);
    lua_pop(L, 1);
    if (eq)
    {
        lua_pop(L, 1);
        LunoValue *p = luno_value_getudata(L, index);
        switch (p->Wrapped.getValueTypeClass())
        {
        case TypeClass_TYPE:
            return LUNO_TYPE_TYPE;
            break;
        case TypeClass_ENUM:    
            return LUNO_TYPE_ENUM;
            break;
        case TypeClass_CHAR:
            return LUNO_TYPE_CHAR;
            break;
        case TypeClass_ANY:
            return LUNO_TYPE_UNDEFINED;
            break;
        default:
            return LUNO_TYPE_ANY; // pseud
            break;
        }
    }
    lua_pop(L, 1);
    return LUNO_TYPE_UNDEFINED;
}


} // luno


static int luno_types_gc(lua_State *L)
{
    LunoTypes *p = luno_types_getudata(L, 1);
    if (p != NULL)
    {
        delete p;
        p = NULL;
    }
    return 0;
}


static const luaL_Reg luno_types[] = {
    {"__gc", luno_types_gc}, 
    {NULL, NULL}
};



inline static OUString luno_luatooustring(lua_State *L, const int index)
{
    size_t length;
    const char *s = luaL_checklstring(L, index, &length);
    return OUString(s, length, RTL_TEXTENCODING_UTF8);
}

inline static void luno_pushoustring(lua_State *L, const OUString &aStr)
{
    lua_pushstring(L, OUSTRTOSTR(aStr));
}


/** Create absolute URL from base URL and relative path.
 * 
 */
static int luno_absolutize(lua_State *L)
{
    const OUString aBaseURL = luno_luatooustring(L, 1);
    const OUString aRelURL = luno_luatooustring(L, 2);
    OUString aAbsURL;
    osl::FileBase::RC e = osl::FileBase::getAbsoluteFileURL(aBaseURL, aRelURL, aAbsURL);
    if (e != osl::FileBase::E_None)
        luaL_error(L, "Illegal URL passed.");
    luno_pushoustring(L, aAbsURL);
    return 1;
}


/** Convert system path to file URL.
 * 
 */
static int luno_tourl(lua_State *L)
{
    const OUString aSysPath = luno_luatooustring(L, 1);
    OUString aURL;
    osl::FileBase::RC e = osl::FileBase::getFileURLFromSystemPath(aSysPath, aURL);
    if (e != osl::FileBase::E_None)
        luaL_error(L, "Illegal URL passed.");
    luno_pushoustring(L, aURL);
    return 1;
}


/** Convert file URL to system path.
 * 
 */
static int luno_tosystempath(lua_State *L)
{
    const OUString aURL = luno_luatooustring(L, 1);
    OUString aSysPath;
    osl::FileBase::RC e = osl::FileBase::getSystemPathFromFileURL(aURL, aSysPath);
    if (e != osl::FileBase::E_None)
        luaL_error(L, "Illegal system path passed.");
    luno_pushoustring(L, aSysPath);
    return 1;
}


/** Bootstrap and initialize.
 * 
 * This function must be called before to do something with UNO.
 */
static int luno_getcontext(lua_State *L)
{
    Reference< XComponentContext > xContext;
    if (Runtime::isInitialized())
    {
        Runtime runtime;
        xContext = runtime.getImple()->xComponentContext;
    }
    else
    {
#if 0
        const int type = luaL_optint(L, 1, 0);
        if (type)
            xContext = ::cppu::bootstrap();
        else
#endif
            xContext = ::cppu::defaultBootstrap_InitialComponentContext();
        if (xContext.is())
        {
            try
            {
                Runtime::initialize(L, xContext);
            }
            catch (RuntimeException &e)
            {
                return luaL_error(L, "Failed to initialize.");
            }
        }
        else
            return luaL_error(L, "Failed to bootstrap.");
    }
    if (xContext.is())
    {
        Runtime runtime;
        try
        {
            runtime.anyToLua(L, makeAny(xContext));
        }
        catch (RuntimeException &e)
        {
            return luaL_error(L, "Failed to convert the context");
        }
    }
    return 1;
}


#define METHOD_CONCEPT MethodConcept::ALL ^ MethodConcept::DANGEROUS
#define PROPERTY_CONCEPT PropertyConcept::ALL ^ PropertyConcept::DANGEROUS


static int luno_proxy_set_attr(lua_State *L, const LunoAdapted *p, 
                const OUString &aPropertyName, const int index)
{
    Runtime runtime;
    Any a;
    try
    {
        a = runtime.luaToAny(L, index);
        if (p->xInvocation.is())
            p->xInvocation->setValue(aPropertyName, a);
        else
        {
            LunoAdapted2 *p2 = (LunoAdapted2 *)p;
            Property aProperty = p2->xIntrospectionAccess->getProperty(
                                        aPropertyName, PROPERTY_CONCEPT);
            Reference< XIdlClass > xIdlClassDest(
                    runtime.getImple()->xCoreReflection->forName(aProperty.Type.getTypeName()));
            Reference< XIdlClass > xIdlClass(
                    runtime.getImple()->xCoreReflection->forName(a.getValueTypeName()));
            
            if (xIdlClassDest->isAssignableFrom(xIdlClass))
                p2->xPropertySet->setPropertyValue(aPropertyName, a);
            else
                p2->xPropertySet->setPropertyValue(aPropertyName, 
                    runtime.getImple()->xTypeConverter->convertTo(a, aProperty.Type));
        }
    }
    catch (com::sun::star::container::NoSuchElementException &e)
    {
        return luaL_error(L, "Unknown property (%s)", OUSTRTOSTR(aPropertyName));
    }
    catch (com::sun::star::beans::UnknownPropertyException &e)
    {
        return luaL_error(L, "Unknown property, maybe readonly (%s)", OUSTRTOSTR(aPropertyName));
    }
    catch (com::sun::star::script::CannotConvertException &e)
    {
        return luaL_error(L, "Value could not be assigned to the property (%s)", 
                        OUSTRTOSTR(aPropertyName));
    }
    catch (com::sun::star::reflection::InvocationTargetException &e)
    {
        com::sun::star::uno::Exception ex;
        e.TargetException >>= ex;
        return luaL_error(L, "%s", OUSTRTOSTR(e.Message));
    }
    catch (RuntimeException &e)
    {
        return luaL_error(L, "%s", OUSTRTOSTR(e.Message));
    }
    catch (Exception &e)
    {
        return luaL_error(L, OUSTRTOSTR(e.Message));
    }
    return 0;
}


static int luno_proxy_get_attr(lua_State *L, const LunoAdapted *p, const OUString &aPropertyName)
{
    Runtime runtime;
    Any a;
    try
    {
        // ToDo has property
        if (p->xInvocation.is())
            a = p->xInvocation->getValue(aPropertyName);
        else
        {
            LunoAdapted2 *p2 = (LunoAdapted2 *)p;
            a = p2->xPropertySet->getPropertyValue(aPropertyName);
        }
        runtime.anyToLua(L, a);
    }
    catch (com::sun::star::beans::UnknownPropertyException &e)
    {
        return luaL_error(L, "Unknown property, maybe writeonly (%s)", OUSTRTOSTR(aPropertyName));
    }
    catch (RuntimeException &e)
    {
        return luaL_error(L, "%s", OUSTRTOSTR(e.Message));
    }
    catch (Exception &e)
    {
        return luaL_error(L, OUSTRTOSTR(e.Message));
    }
    return 1;
}


static int luno_proxy_call_method2(lua_State *L, const LunoAdapted2 *p, 
                const OUString &aMethodName, const int nStartIndex)
{
    Runtime runtime;
    try
    {
        const int top = lua_gettop(L);
        if (nStartIndex < 1)
            return luaL_error(L, "Illegal arguments passed");
        
        Sequence< Any > aParams;
        Any retValue;
        
        Reference< XIdlMethod > xIdlMethod(
                p->xIntrospectionAccess->getMethod(aMethodName, METHOD_CONCEPT));
        const Sequence< ParamInfo > aParamInfos = xIdlMethod->getParameterInfos();
        const sal_Int32 nLength = aParamInfos.getLength();
        bool bOutParams = false;
        if (nLength)
        {
            if (nLength != (top - nStartIndex + 1))
                return luaL_error(L, "Illegal length of arguments (%d for %d).", 
                                    (int)(top - nStartIndex + 1), (int)nLength);
            const ParamInfo *pParamInfo = aParamInfos.getConstArray();
            aParams = Sequence< Any >(top - nStartIndex + 1);
            
            for (int i = 0; i < nLength; ++i)
            {
                if (pParamInfo[i].aMode != ParamMode_OUT)
                {
                    // ToDo conversion is not required?
                    Any a = runtime.luaToAny(L, i + nStartIndex);
                    if (pParamInfo[i].aType->isAssignableFrom(
                            runtime.getImple()->xCoreReflection->forName(a.getValueTypeName())))
                        aParams[i] <<= a;
                    else
                    {
                        aParams[i] <<= runtime.getImple()->xTypeConverter->convertTo(
                                    a, 
                                    Type(pParamInfo[i].aType->getTypeClass(), pParamInfo[i].aType->getName()));
                    }
                    bOutParams = true;
                }
            }
        }
        retValue = xIdlMethod->invoke(p->Wrapped, aParams);
        runtime.anyToLua(L, retValue);
        
        int n = 0;
        if (bOutParams)
        {
            const Any *pParams = aParams.getConstArray();
            const ParamInfo *pParamInfo = aParamInfos.getConstArray();
            for (int i = 0; i < nLength; ++i)
            {
                if (pParamInfo[i].aMode != ParamMode_IN)
                {
                    runtime.anyToLua(L, pParams[i]);
                    ++n;
                }
            }
        }
        return 1 + n;
    }
    catch (com::sun::star::reflection::InvocationTargetException &e)
    {
        runtime.anyToLua(L, e.TargetException);
        return lua_error(L); // exception as error message
    }
    catch (com::sun::star::lang::IllegalArgumentException &e)
    {
        return luaL_error(L, "Illegal argument, %s, position: %d", 
                    OUSTRTOSTR(e.Message), (int)e.ArgumentPosition);
    }
    catch (com::sun::star::script::CannotConvertException &e)
    {
        return luaL_error(L, "Can not converted, %s, reason: %d, position: %d", 
                    OUSTRTOSTR(e.Message), (int)e.Reason, (int)e.ArgumentIndex);
    }
    catch (RuntimeException &e)
    {
        return luaL_error(L, "%s", OUSTRTOSTR(e.Message));
    }
    catch (Exception &e)
    {
        return luaL_error(L, OUSTRTOSTR(e.Message));
    }
    return 0;
}


static int luno_proxy_call_method(lua_State *L, LunoAdapted *p, 
                const OUString &aMethodName, const int nStartIndex)
{
    Sequence< short > aOutParamIndex;
    Sequence< Any > aParams;
    Sequence< Any > aOutParams;
    Any outParams;
    Any retValue;
    Runtime runtime;
    try
    {
        const int nTop = lua_gettop(L);
        if (nStartIndex <= nTop)
        {
            aParams = Sequence< Any >(nTop - nStartIndex + 1);
            Any *pParams = aParams.getArray();
            for (int i = nStartIndex; i <= nTop; ++i)
                pParams[i - nStartIndex] = runtime.luaToAny(L, i);
        }
        
        retValue = p->xInvocation->invoke(
                aMethodName, aParams, aOutParamIndex, aOutParams);
        
        runtime.anyToLua(L, retValue);
        const sal_Int32 n = aOutParams.getLength();
        if (aOutParams.getLength())
        {
            const Any *pOutParams = aOutParams.getConstArray();
            for (int i = 0; i < n; ++i)
                runtime.anyToLua(L, pOutParams[i]);
        }
        return 1 + n;
    }
    catch (com::sun::star::reflection::InvocationTargetException &e)
    {
        runtime.anyToLua(L, e.TargetException);
        return lua_error(L); // exception as error message
    }
    catch (com::sun::star::lang::IllegalArgumentException &e)
    {
        return luaL_error(L, "Illegal argument, %s, position: %d", 
                    OUSTRTOSTR(e.Message), (int)e.ArgumentPosition);
    }
    catch (com::sun::star::script::CannotConvertException &e)
    {
        return luaL_error(L, "Can not converted, %s, reason: %d, position: %d", 
                    OUSTRTOSTR(e.Message), (int)e.Reason, (int)e.ArgumentIndex);
    }
    catch (RuntimeException &e)
    {
        return luaL_error(L, "%s", OUSTRTOSTR(e.Message));
    }
    catch (Exception &e)
    {
        return luaL_error(L, OUSTRTOSTR(e.Message));
    }
    return 0;
}


static int luno_proxy_call_func(lua_State *L)
{
    // obj, [args, ...]
    // Get function name from closure upvalues.
    const OUString aMethodName = luno_luatooustring(L, lua_upvalueindex(1));
    LunoAdapted *p = luno_proxy_getudata(L, 1, LUNO_META_PROXY);
    if (p != NULL)
    {
        if (p->xInvocation.is())
        {
            if (p->xInvocation->hasMethod(aMethodName))
                return luno_proxy_call_method(L, p, aMethodName, 2);
        }
        else
        {
            LunoAdapted2 *p2 = (LunoAdapted2 *)p;
            if (p2->xIntrospectionAccess->hasMethod(aMethodName, METHOD_CONCEPT))
                return luno_proxy_call_method2(L, p2, aMethodName, 2);
        }
    }
    else
        return luaL_error(L, "Illegal object passed");
    return luaL_error(L, "Method could not find (%s)", OUSTRTOSTR(aMethodName));
}


static int luno_proxy_gc(lua_State *L)
{
    LunoAdapted *p = luno_proxy_getudata(L, 1, LUNO_META_PROXY);
    if (p != NULL)
    {
        if (p->xInvocation.is())
            delete p;
        else
        {
            LunoAdapted2 *p2 = (LunoAdapted2 *)p;
            delete p2;
        }
        p = NULL;
    }
    return 0;
}


#define CHECK_Interface(p,index) \
    if (p == NULL) \
        return luaL_argerror(L, index, "Illegal argument passed for interface"); \
    else if (p->Wrapped.getValueTypeClass() != TypeClass_INTERFACE) \
        return luaL_argerror(L, index, "Illegal data passed for interface")


static int luno_proxy_eq(lua_State *L)
{
    LunoAdapted *p = luno_proxy_getudata(L, 1, LUNO_META_PROXY);
    CHECK_Interface(p, 1);
    LunoAdapted *p2 = luno_proxy_getudata(L, 2, LUNO_META_PROXY);
    
    lua_pushboolean(L, 
        (p2 != NULL && 
         p2->Wrapped.getValueTypeClass() == TypeClass_INTERFACE && 
         p->Wrapped == p2->Wrapped) ? 1 : 0);
    return 1;
}


// ToDo check table index
static int luno_proxy_elements(lua_State *L, LunoAdapted *p)
{
    if (p->xInvocation.is())
    {
        const Sequence< InvocationInfo > infos(p->xInvocation->getInfo());
        const int length = infos.getLength();
        const InvocationInfo *pInfos = infos.getConstArray();
        lua_createtable(L, length, 0);
        for (int i = 0; i < length; ++i)
        {
            lua_pushstring(L, OUSTRTOSTR(pInfos[i].aName));
            lua_rawseti(L, 3, i + 1);
        }
    }
    else
    {
        LunoAdapted2 *p2 = (LunoAdapted2 *)p;
        const Sequence< Reference< XIdlMethod > > aXIdlMethods(
                p2->xIntrospectionAccess->getMethods(METHOD_CONCEPT));
        const Sequence< Property > aProperties(
                p2->xIntrospectionAccess->getProperties(PROPERTY_CONCEPT));
        
        const int nMethodsLength = aXIdlMethods.getLength();
        const int nPropertiesLength = aProperties.getLength();
        const Reference< XIdlMethod > *pXIdlMethods = aXIdlMethods.getConstArray();
        lua_createtable(L, nMethodsLength + nPropertiesLength, 0);
        for (int i = 0; i < nMethodsLength; ++i)
        {
            lua_pushstring(L, OUSTRTOSTR(pXIdlMethods[i]->getName()));
            lua_rawseti(L, 3, i + 1);
        }
        const Property *pProperties = aProperties.getConstArray();
        for (int i = nMethodsLength; i < nPropertiesLength; ++i)
        {
            lua_pushstring(L, OUSTRTOSTR(pProperties[i].Name));
            lua_rawseti(L, 3, i + 1);
        }
    }
    
    const int top = lua_gettop(L);
    lua_getglobal(L, "table");
    if (!lua_isnil(L, -1))
    {
        lua_pushstring(L, "sort");
        lua_rawget(L, -2);
        if (!lua_isnil(L, -1))
        {
            lua_pushvalue(L, -3); // table to sort
            lua_call(L, 1, 0); // sort and table copy are removed
        }
    }
    lua_settop(L, top);
    return 1;
}


static int luno_proxy_index(lua_State *L)
{
    size_t length;
    const char *name = luaL_checklstring(L, 2, &length);
    const OUString aName(name, length, RTL_TEXTENCODING_UTF8);
    
    LunoAdapted *p = luno_proxy_getudata(L, 1, LUNO_META_PROXY);
    CHECK_Interface(p, 1);
    
    if (p->xInvocation.is())
    {
        if (p->xInvocation->hasMethod(aName))
        {
            // Set function name as upvalues.
            lua_pushstring(L, name);
            lua_pushcclosure(L, luno_proxy_call_func, 1);
            // ToDo keep xIdlMethod and execute with luno_proxy_call_method2?
            return 1;
        }
        else if (p->xInvocation->hasProperty(aName))
        {
            luno_proxy_get_attr(L, p, aName);
            return 1;
        }
    }
    else
    {
        LunoAdapted2 *p2 = (LunoAdapted2 *)p;
        if (p2->xIntrospectionAccess->hasMethod(aName, METHOD_CONCEPT))
        {
            // Set function name as upvalues.
            lua_pushstring(L, name);
            lua_pushcclosure(L, luno_proxy_call_func, 1);
            // ToDo set XIdlMethod as upvalue to reduce searching
            return 1;
        }
        else if (p2->xIntrospectionAccess->hasProperty(aName, PROPERTY_CONCEPT))
        {
            luno_proxy_get_attr(L, p, aName);
            return 1;
        }
    }
    if (strcmp(name, "__elements") == 0)
    {
        luno_proxy_elements(L, p);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}


static int luno_proxy_newindex(lua_State *L)
{
    const OUString aName = luno_luatooustring(L, 2);
    
    LunoAdapted *p = luno_proxy_getudata(L, 1, LUNO_META_PROXY);
    CHECK_Interface(p, 1);
    
    if (p->xInvocation.is())
    {
        if (p->xInvocation->hasProperty(aName))
        {
            luno_proxy_set_attr(L, p, aName, lua_gettop(L));
            return 0;
        }
    }
    else
    {
        LunoAdapted2 *p2 = (LunoAdapted2 *)p;
        if (p2->xIntrospectionAccess->hasProperty(aName, PROPERTY_CONCEPT))
        {
            // ToDo
            luno_proxy_set_attr(L, p, aName, lua_gettop(L));
            return 0;
        }
    }
    return luaL_error(L, "Illegal property (%s)", OUSTRTOSTR(aName));
}


static int luno_proxy_tostring(lua_State *L)
{
    LunoAdapted *p = luno_proxy_getudata(L, 1, LUNO_META_PROXY);
    if (p != NULL)
    {
        OUStringBuffer buff;
        lua_pushfstring(L, "%p", (void*)p);
        buff.appendAscii("<LunoProxy#")
            .appendAscii(lua_tostring(L, -1));
        lua_pop(L, 1);
        
        const Reference< XServiceInfo > xServiceInfo(p->Wrapped, UNO_QUERY);
        if (xServiceInfo.is())
        {
            buff.appendAscii(" implename: ")
                .append(xServiceInfo->getImplementationName())
                .appendAscii("; ");
            
            buff.appendAscii("service names: {");
            const Sequence< OUString > aServiceNames(xServiceInfo->getSupportedServiceNames());
            const OUString *pServiceNames = aServiceNames.getConstArray();
            const int nLength = aServiceNames.getLength();
            for (int i = 0; i < nLength; ++i)
            {
                buff.append(pServiceNames[i])
                    .appendAscii(", ");
            }
            buff.appendAscii("}; ");
        }
        
        const Reference< XTypeProvider > xTypeProvider(p->Wrapped, UNO_QUERY);
        if (xTypeProvider.is())
        {
            buff.appendAscii(" types: {");
            const Sequence< Type > aTypes(xTypeProvider->getTypes());
            const Type *pTypes = aTypes.getConstArray();
            const int nLength = aTypes.getLength();
            for (int i = 0; i < nLength; ++i)
            {
                buff.append(pTypes[i].getTypeName())
                    .appendAscii(", ");
            }
            buff.appendAscii("}");
        }
        
        buff.appendAscii(">");
        
        lua_pushstring(L, OUSTRTOSTR(buff.makeStringAndClear()));
    }
    else
        lua_pushstring(L, "");
    return 1;
}


static const luaL_Reg luno_proxy[] = {
    {"__index", luno_proxy_index}, 
    {"__newindex", luno_proxy_newindex}, 
    {"__gc", luno_proxy_gc}, 
    {"__eq", luno_proxy_eq}, 
    {"__tostring", luno_proxy_tostring}, 
    {NULL, NULL}
};


#define CHECK_Struct(p) \
    if (p == NULL) \
        return luaL_argerror(L, 1, "Illegal argument passed"); \
    else if (!(p->Wrapped.getValueTypeClass() == TypeClass_STRUCT || \
        p->Wrapped.getValueTypeClass() == TypeClass_EXCEPTION)) \
        return luaL_argerror(L, 1, "Illegal data passed for struct or exception")


static int luno_struct_fill(lua_State *L, 
            const typelib_CompoundTypeDescription *pCompType, 
            const Reference< XInvocation2 > &xInvocation, const Runtime &runtime, const int nArgs)
{
    int nIndex = 0;
    if (pCompType->pBaseTypeDescription)
        nIndex = luno_struct_fill(
            L, pCompType->pBaseTypeDescription, xInvocation, runtime, nArgs);
    
    int i;
    for (i = 0; i < pCompType->nMembers; ++i)
    {
        if (i + nIndex > nArgs)
            break;
        xInvocation->setValue(
            pCompType->ppMemberNames[i], runtime.luaToAny(L, i + nIndex + 1));
    }
    return i + nIndex;
}


static int luno_struct_new(lua_State *L)
{
    size_t length;
    const char *name = luaL_checklstring(L, lua_upvalueindex(1), &length);
    const OUString aTypeName = OUString(name, length, RTL_TEXTENCODING_UTF8);
    
    const int nArgs = lua_gettop(L) - 1; // 1 is table
    Runtime runtime;
    Any aStruct;
    
    const Reference< XIdlClass > xIdlClass(
        runtime.getImple()->xCoreReflection->forName(aTypeName), UNO_QUERY);
    if (!xIdlClass.is() || 
        !(xIdlClass->getTypeClass() == TypeClass_STRUCT || 
          xIdlClass->getTypeClass() == TypeClass_EXCEPTION))
        return luaL_error(L, "unknown type name (%s)", name);
    xIdlClass->createObject(aStruct);
    
    try
    {
        runtime.anyToLua(L, aStruct);
    }
    catch (RuntimeException &e)
    {
        return luaL_error(L, "Failed to convert struct (%s)", name);
    }
    LunoAdapted *p = luno_proxy_getudata(L, lua_gettop(L), LUNO_META_STRUCT);
    if (nArgs > 0 && p != NULL)
    {
        TypeDescription desc(aTypeName);
        if (desc.is())
        {
            typelib_CompoundTypeDescription *pCompType = 
                    (typelib_CompoundTypeDescription *) desc.get();
            luno_struct_fill(L, pCompType, p->xInvocation, runtime, nArgs);
        }
        else
            return luaL_error(L, "unknown type name (%s)", name);
    }
    return 1;
}


static int luno_struct_index(lua_State *L)
{
    size_t length;
    const char *name = luaL_checklstring(L, 2, &length);
    const OUString aName(name, length, RTL_TEXTENCODING_UTF8);
    
    LunoAdapted *p = luno_proxy_getudata(L, 1, LUNO_META_STRUCT);
    if (p != NULL)
    {
        if (p->xInvocation->hasProperty(aName))
            luno_proxy_get_attr(L, p, aName);
        else if (strcmp(name, "__elements") == 0)
        {
            luno_proxy_elements(L, p);
            return 1;
        }
        else if (strcmp(name, "typename") == 0)
        {
            luno_pushoustring(L, p->Wrapped.getValueTypeName());
            return 1;
        }
        else
            lua_pushnil(L);
    }
    else
        lua_pushnil(L);
    return 1;
}


static int luno_struct_newindex(lua_State *L)
{
    const int top = lua_gettop(L);
    size_t length;
    const char *name = luaL_checklstring(L, 2, &length);
    const OUString aName(name, length, RTL_TEXTENCODING_UTF8);
    
    LunoAdapted *p = luno_proxy_getudata(L, 1, LUNO_META_STRUCT);
    if (p != NULL)
    {
        if (p->xInvocation->hasProperty(aName))
            luno_proxy_set_attr(L, p, aName, top);
    }
    // ToDo return value is required? should return nil?
    return 0;
}


static int luno_struct_eq(lua_State *L)
{
    LunoAdapted *p = luno_proxy_getudata(L, 1, LUNO_META_STRUCT);
    CHECK_Struct(p);
    LunoAdapted *p2 = luno_proxy_getudata(L, 2, LUNO_META_STRUCT);
    
    lua_pushboolean(L, (
            p2 != NULL && 
            p->Wrapped.getValueTypeClass() == p2->Wrapped.getValueTypeClass() && 
            p->Wrapped == p2->Wrapped) ? 1 : 0);
    return 1;
}


static int luno_struct_gc(lua_State *L)
{
    LunoAdapted *p = luno_proxy_getudata(L, 1, LUNO_META_STRUCT);
    if (p!= NULL)
    {
        delete p;
        p = NULL;
    }   
    return 0;
}


static int luno_struct_tostring(lua_State *L)
{
    LunoAdapted *p = luno_proxy_getudata(L, 1, LUNO_META_STRUCT);
    if (p != NULL)
    {
        if (p->Wrapped.getValueTypeClass() == TypeClass_STRUCT)
            lua_pushfstring(L, "<LunoStruct#%p (%s)>", 
                (void*)p, OUSTRTOSTR(p->Wrapped.getValueTypeName()));
        else
        {
            com::sun::star::uno::Exception e;
            p->Wrapped >>= e;
            lua_pushfstring(L, "<LunoException#%p (%s), %s>", 
                (void*)p, OUSTRTOSTR(p->Wrapped.getValueTypeName()), 
                OUSTRTOSTR(e.Message));
        }
    }
    else
        lua_pushstring(L, "");
    return 1;
}


static const luaL_Reg luno_struct[] = {
    {"__index", luno_struct_index}, 
    {"__newindex", luno_struct_newindex}, 
    {"__gc", luno_struct_gc}, 
    {"__eq", luno_struct_eq}, 
    {"__tostring", luno_struct_tostring}, 
    {NULL, NULL}
};


#define CHECK_Value(p,index) \
    if (p == NULL) \
        return luaL_argerror(L, 1, "Illegal userdata passed")


static int luno_type_new(lua_State *L)
{
    const OUString aTypeName = luno_luatooustring(L, 2);
    TypeDescription desc(aTypeName);
    if (desc.is())
    {
        Type t = desc.get()->pWeakRef;
        LunoValue *p = luno_value_newudata(L);
        p->Wrapped <<= t;
    }
    else
        return luaL_error(L, "Illegal type name (%s)", OUSTRTOSTR(aTypeName));
    return 1;
}


static OUString luno_enum_get_value_name(lua_State *L, const LunoValue *p)
{
    const sal_Int32 nValue = *(sal_Int32 *)p->Wrapped.getValue();
    TypeDescription desc(p->Wrapped.getValueType());
    if (desc.is())
    {
        desc.makeComplete();
        const typelib_EnumTypeDescription *pEnumDesc = 
                    (typelib_EnumTypeDescription *) desc.get();
        for (int i = 0; i < pEnumDesc->nEnumValues; ++i)
        {
            if (pEnumDesc->pEnumValues[i] == nValue)
                return OUString(pEnumDesc->ppEnumNames[i]);
        }
    }
    return OUString();
}


static int luno_enum_new(lua_State *L)
{
    size_t nTypeNameLen, nValueLen;
    const char *type_name = luaL_checklstring(L, 2, &nTypeNameLen);
    const char *value = luaL_checklstring(L, 3, &nValueLen);
    const OUString aTypeName(type_name, nTypeNameLen, RTL_TEXTENCODING_UTF8);
    const OUString aStrValue(value, nValueLen, RTL_TEXTENCODING_UTF8);
    
    TypeDescription desc(aTypeName);
    if (desc.is())
    {
        if (desc.get()->eTypeClass != typelib_TypeClass_ENUM)
            return luaL_error(L, "Type name is not one of enum (%s)", type_name);
        
        desc.makeComplete();
        
        const typelib_EnumTypeDescription *pEnumDesc = 
                        (typelib_EnumTypeDescription*) desc.get();
        int i = 0;
        for (i = 0; i < pEnumDesc->nEnumValues; ++i)
        {
            if ((*((OUString*)&pEnumDesc->ppEnumNames[i])).equals(aStrValue))
                break;
        }
        if (i == pEnumDesc->nEnumValues)
            return luaL_error(L, "Value does not find (%s.%s)", type_name, value);
        
        LunoValue *p = luno_value_newudata(L);
        p->Wrapped.setValue(&pEnumDesc->pEnumValues[i], desc.get()->pWeakRef);
    }
    else
        return luaL_error(L, "Illegal type name (%s)", type_name);
    return 1;
}


static int luno_char_new(lua_State *L)
{
    size_t nValueLen;
    const char *value = luaL_checklstring(L, 2, &nValueLen);
    if (nValueLen != 1)
        luaL_argerror(L, 2, "Illegal length");
    const OUString aValue(value, nValueLen, RTL_TEXTENCODING_UTF8);
    
    Any a;
    const sal_Unicode c = aValue.toChar();
    a.setValue(&c, getCharCppuType());
    LunoValue *p = luno_value_newudata(L);
    p->Wrapped = a;
    return 1;
}


static int luno_any_new(lua_State *L)
{
    Type t;
    const int type = lua_type(L, 2);
    if (type == LUA_TSTRING)
    {
        OUString aTypeName = luno_luatooustring(L, 2);
        TypeDescription desc(aTypeName);
        if (desc.is())
            t = desc.get()->pWeakRef;
        else
            return luaL_error(L, "Illegal type name (%s)", OUSTRTOSTR(aTypeName));
    }
    else if (type == LUA_TUSERDATA)
    {
        LunoValue *p = (LunoValue *)luno_checked_udata(L, 2, LUNO_META_VALUE);
        if (p != NULL)
            p->Wrapped >>= t;
        else
            return luaL_error(L, "Illegal userdata passed");
    }
    Runtime runtime;
    Any a = runtime.luaToAny(L, 3);
    try
    {
        Any result = runtime.getImple()->xTypeConverter->convertTo(a, t);
        LunoValue *p = luno_value_newudata(L);
        p->Wrapped = result;
    }
    catch (com::sun::star::lang::IllegalArgumentException &e)
    {
        return luaL_error(L, "%s", OUSTRTOSTR(e.Message)); // non-simple to simple type
    }
    catch (com::sun::star::script::CannotConvertException &e)
    {
        return luaL_error(L, "Unable to convert to type (%s)", OUSTRTOSTR(t.getTypeName()));
    }
    catch (RuntimeException &e)
    {
        return luaL_error(L, "%s", OUSTRTOSTR(e.Message));
    }
    return 1;
}


static int luno_value_gc(lua_State *L)
{
    LunoValue *p = luno_value_getudata(L, 1);
    if (p != NULL)
    {
        delete p;
        p = NULL;
    }
    return 0;
}


static int luno_value_eq(lua_State *L)
{
    LunoValue *p = luno_value_getudata(L, 1);
    CHECK_Value(p, 1);
    LunoValue *p2 = luno_value_getudata(L, 2);
    
    lua_pushboolean(L, 
        (p2 != NULL && 
        p->Wrapped.getValueTypeClass() == p2->Wrapped.getValueTypeClass() && 
        p->Wrapped == p2->Wrapped) ? 1 : 0);
    return 1;
}


static int luno_value_index(lua_State *L)
{
    LunoValue *p = luno_value_getudata(L, 1);
    CHECK_Value(p, 1);
    
    const char *name = luaL_checkstring(L, 2);
    switch (p->Wrapped.getValueTypeClass())
    {
        case TypeClass_TYPE:
        {
            if (strcmp(name, "typename") == 0)
            {
                Type t;
                p->Wrapped >>= t;
                lua_pushstring(L, OUSTRTOSTR(t.getTypeName()));
            }
            else
                lua_pushnil(L);
            break;
        }
        case TypeClass_ENUM:
        {
            if (strcmp(name, "typename") == 0)
                lua_pushstring(L, OUSTRTOSTR(p->Wrapped.getValueTypeName()));
            else if (strcmp(name, "value") == 0)
                lua_pushstring(L, OUSTRTOSTR(luno_enum_get_value_name(L, p)));
            else
                lua_pushnil(L);
            break;
        }
        case TypeClass_CHAR:
        {
            if (strcmp(name, "value") == 0)
                lua_pushstring(L, 
                    OUSTRTOSTR(OUString(*(sal_Unicode*)p->Wrapped.getValue())));
            else
                lua_pushnil(L);
            break;
        }
        default:
        {
            // created by any
            if (strcmp(name, "typename") == 0)
                lua_pushstring(L, OUSTRTOSTR(p->Wrapped.getValueTypeName()));
            else if (strcmp(name, "typeclass") == 0)
            {
                Runtime runtime;
                try
                {
                    runtime.anyToLua(L, makeAny(p->Wrapped.getValueTypeClass()));
                }
                catch (RuntimeException &e)
                {
                    return luaL_error(L, "Failed to convert value (%s)", 
                            OUSTRTOSTR(p->Wrapped.getValueTypeName()));
                }
            }
            else if (strcmp(name, "value") == 0)
            {
                Runtime runtime;
                try
                {
                    runtime.anyToLua(L, p->Wrapped);
                }
                catch (RuntimeException &e)
                {
                    return luaL_error(L, "Failed to convert value (%s)", 
                            OUSTRTOSTR(p->Wrapped.getValueTypeName()));
                }
            }
            else
                lua_pushnil(L);
            break;
        }
    }
    return 1;
}


static int luno_value_tostring(lua_State *L)
{
    LunoValue *p = luno_value_getudata(L, 1);
    CHECK_Value(p, 1);
    
    OUStringBuffer buff;
    switch (p->Wrapped.getValueTypeClass())
    {
        case TypeClass_TYPE:
        {
            Type t;
            p->Wrapped >>= t;
            
            buff.appendAscii("<Type ")
                .append(OUString(t.getTypeName()))
                .appendAscii(">");
            break;
        }
        case TypeClass_ENUM:
        {
            buff.appendAscii("<Enum ")
                .append(OUString(p->Wrapped.getValueTypeName()))
                .appendAscii(".")
                .append(OUString(luno_enum_get_value_name(L, p)))
                .appendAscii(">");
            break;
        }
        case TypeClass_CHAR:
        {
            buff.appendAscii("<Char ")
                .append(OUString(*(sal_Unicode*)p->Wrapped.getValue()))
                .appendAscii(">");
            break;
        }
        default:
        {
            // created by any
            buff.appendAscii("<")
                .append(p->Wrapped.getValueTypeName())
                .appendAscii(">");
            break;
        }
    }
    lua_pushstring(L, OUSTRTOSTR(buff.makeStringAndClear()));
    return 1;
}


static const luaL_Reg luno_value[] = {
    {"__index", luno_value_index}, 
    {"__gc", luno_value_gc}, 
    {"__eq", luno_value_eq}, 
    {"__tostring", luno_value_tostring}, 
    {NULL, NULL}
};


#define CHECK_Seq(p) \
    if (p == NULL || \
        p->Wrapped.getValueTypeClass() != TypeClass_SEQUENCE || \
        !p->xIdlArray.is()) \
        return luaL_error(L, "Illegal data passed for seq")


static LunoSequence *luno_seq_create(lua_State *L, const OUString &aTypeName, const int create)
{
    LunoSequence *p = NULL;
    TypeDescription desc(aTypeName);
    if (desc.is())
    {
        Runtime runtime;
        const Reference< XIdlClass > xIdlClass(
                runtime.getImple()->xCoreReflection->forName(aTypeName));
        if (xIdlClass.is() && xIdlClass->getTypeClass() == TypeClass_SEQUENCE)
        {
            const Reference< XIdlArray > xIdlArray(xIdlClass, UNO_QUERY);
            if (xIdlArray.is())
            {
                p = luno_seq_newudata(L);
                if (p != NULL)
                {
                    p->xIdlArray = xIdlArray;
                    if (create)
                        xIdlClass->createObject(p->Wrapped);
                }
            }
        }
    }
    return p;
}


static int luno_seq_new(lua_State *L)
{
    // [type_name, [length, | init] ]
    const int nArgs = lua_gettop(L);
    
    OUString aTypeName;
    if (nArgs == 2)
        aTypeName = OUString(RTL_CONSTASCII_USTRINGPARAM("[]any"));
    else
        aTypeName = luno_luatooustring(L, 2);
    
    int nDataType = 0;
    sal_Int32 nLength = 0;
    if (nArgs > 2)
    {
        // ToDo initialize from Seq
        // get initial length or initial elements
        const int type = lua_type(L, 3);
        if (type == LUA_TNUMBER)
        {
            nLength = (sal_Int32)luaL_checkint(L, 3);
            if (nLength < 0)
                return luaL_argerror(L, 3, "Length should be positive number");
        }
        else if (type == LUA_TTABLE)
        {
            nLength = (sal_Int32)GET_LENGTH(L, 3);
            nDataType = 1;
        }
        else
            return luaL_argerror(L, 3, "Length or initial data from table required");
    }
    
    LunoSequence *p = luno_seq_create(L, aTypeName, 1);
    if (p != NULL)
    {
        try
        {
            if (nLength > 0)
                p->xIdlArray->realloc(p->Wrapped, nLength);
        }
        catch (IllegalArgumentException &e)
        {
            return luaL_error(L, "Illegal sequence passed");
        }
        
        if (nDataType == 1)
        {
            // initialize from table
            try
            {
                Runtime runtime;
                Reference< XIdlArray > xIdlArray(p->xIdlArray);
                Reference< XIdlClass > xIdlClass(p->xIdlArray, UNO_QUERY);
                Reference< XIdlClass > xCompIdl = xIdlClass->getComponentType();
                if (!xCompIdl.is())
                    return luaL_error(L, "Component type of the sequence is illegal");
                Type t(xCompIdl->getTypeClass(), xCompIdl->getName());
                
                for (int i = 0; i < nLength; i++)
                {
                    lua_rawgeti(L, 3, i + 1);
                    Any a = runtime.luaToAny(L, -1);
                    
                    if (a.isExtractableTo(t))
                        xIdlArray->set(p->Wrapped, i, a);
                    else
                    {
                        Any converted = runtime.getImple()->xTypeConverter->convertTo(a, t);
                        xIdlArray->set(p->Wrapped, i, converted);
                    }
                    lua_pop(L, 1);
                }
            }
            catch (IllegalArgumentException &e)
            {
                return luaL_error(L, "Initial table contains value which could not be inserted to sequence (%s)", OUSTRTOSTR(aTypeName));
            }
            catch (com::sun::star::script::CannotConvertException &e)
            {
                return luaL_error(L, "Failed to convert value to table element");
            }
            catch (ArrayIndexOutOfBoundsException &e)
            {
                return luaL_error(L, "Seq index out of bounds, should be from 1 to %d", (int)nLength);
            }
            catch (RuntimeException &e)
            {
                return luaL_error(L, "Failed to convert value to UNO value");
            }
        }
    }
    else
        return luaL_error(L, "Illegal type name (%s)", OUSTRTOSTR(aTypeName));
    return 1;
}


static int luno_seq_gc(lua_State *L)
{
    LunoSequence *p = luno_seq_getudata(L, 1);
    if (p != NULL)
    {
        delete p;
        p = NULL;
    }
    return 0;
}


static int luno_seq_len(lua_State *L)
{
    LunoSequence *p = luno_seq_getudata(L, 1);
    CHECK_Seq(p);
    try
    {
        lua_pushinteger(L, (int)p->xIdlArray->getLen(p->Wrapped));
    }
    catch (IllegalArgumentException &e)
    {
        return luaL_error(L, "Illegal sequence passed");
    }
    catch (RuntimeException &e)
    {
        return luaL_error(L, "Unknown error");
    }
    return 1;
}


static int luno_seq_eq(lua_State *L)
{
    LunoSequence *p = luno_seq_getudata(L, 1);
    CHECK_Seq(p);
    
    LunoSequence *p2 = luno_seq_getudata(L, 2);
    lua_pushboolean(L, 
            (p2 != NULL && 
             p2->Wrapped.getValueTypeClass() == TypeClass_SEQUENCE && 
             p->Wrapped == p2->Wrapped) ? 1 : 0);
    return 1;
}


static int luno_seq_tostring(lua_State *L)
{
    LunoSequence *p = luno_seq_getudata(L, 1);
    CHECK_Seq(p);
    
    OUStringBuffer buff;
    buff.appendAscii("<Seq ")
        .append(p->Wrapped.getValueTypeName())
        .appendAscii(", length: ")
        .append(p->xIdlArray->getLen(p->Wrapped))
        .appendAscii(">");
    lua_pushstring(L, OUSTRTOSTR(buff.makeStringAndClear()));
    return 1;
}


static int luno_seq_tobytestring(lua_State *L)
{
    LunoSequence *p = luno_seq_getudata(L, 1);
    CHECK_Seq(p);
    
    Sequence< sal_Int8 > aBytes;
    if (p->Wrapped >>= aBytes)
        lua_pushlstring(L, (char *)aBytes.getConstArray(), (int)aBytes.getLength());
    else
        return luaL_argerror(L, 1, "Should be Seq of []byte");
    return 1;
}


/** Generate version 4 uuid.
 * 
 */
static int luno_uuid(lua_State *L)
{
    LunoSequence *p = luno_seq_create(L, OUSTRCONST("[]byte"), 0);
    if (p != NULL)
    {
        Sequence< sal_Int8 > aBytes(16);
        rtl_createUuid((sal_uInt8*)aBytes.getArray(), 0, sal_False);
        p->Wrapped <<= makeAny(aBytes);
    }
    else
        return luaL_error(L, "Failed to create new userdata");
    return 1;
}


static int luno_seq_tobytes(lua_State *L)
{
    size_t length;
    const char *bytes = luaL_checklstring(L, 1, &length);
    
    LunoSequence *p = luno_seq_create(L, OUSTRCONST("[]byte"), 0);
    if (p != NULL)
    {
        Sequence< sal_Int8 > aBytes((sal_Int8 *)bytes, (sal_Int32)length);
        p->Wrapped <<= aBytes;
    }
    else
        return luaL_error(L, "Failed to create new userdata");
    return 1;
}


static int luno_seq_iter(lua_State *L)
{
    LunoSequence *p = luno_seq_getudata(L, 1);
    CHECK_Seq(p);
    
    const int n = luaL_checkint(L, 2) + 1; // next index
    const sal_Int32 nLength = p->xIdlArray->getLen(p->Wrapped);
    if (1 <= n && n <= nLength)
    {
        lua_pushinteger(L, n);
        try
        {
            Runtime runtime;
            Any a = p->xIdlArray->get(p->Wrapped, n - 1); // match index with lua
            runtime.anyToLua(L, a);
        }
        catch (IllegalArgumentException &e)
        {
            return luaL_error(L, "Illegal sequence passed");
        }
        catch (ArrayIndexOutOfBoundsException &e)
        {
            return luaL_error(L, "Seq index out of bounds, should be from 1 to %d (%d)", (int)nLength, n);
        }
        catch (RuntimeException &e)
        {
            return luaL_error(L, "Failed to convert value to Lua value");
        }
    }
    else
    {
        lua_pushnil(L);
        lua_pushnil(L);
    }
    return 2;
}


static int luno_seq_ipairs(lua_State *L)
{
    LunoSequence *p = luno_seq_getudata(L, 1);
    CHECK_Seq(p);
    
    lua_pushcfunction(L, luno_seq_iter);
    lua_pushvalue(L, 1);
    lua_pushinteger(L, 0);
    return 3;
}


static int luno_seq_realloc(lua_State *L)
{
    LunoSequence *p = luno_seq_getudata(L, 1);
    CHECK_Seq(p);
    
    const int n = luaL_checkint(L, 2);
    if (n < 0)
        return luaL_error(L, "Illegal length for seq (%d)", n);
    
    try
    {
        p->xIdlArray->realloc(p->Wrapped, n);
    }
    catch (IllegalArgumentException &e)
    {
        return luaL_error(L, "Illegal sequence passed");
    }
    catch (RuntimeException &e)
    {
        return luaL_error(L, "Unknown error");
    }
    return 0;
}


static int luno_seq_totable(lua_State *L)
{
    LunoSequence *p = luno_seq_getudata(L, 1);
    CHECK_Seq(p);
    
    Runtime runtime;
    sal_Int32 nLength = 0;
    try
    {
        nLength = p->xIdlArray->getLen(p->Wrapped);
        lua_createtable(L, nLength, 0);
        const int nTable = lua_gettop(L);
        
        for (int i = 0; i < nLength; i++)
        {
            runtime.anyToLua(L, p->xIdlArray->get(p->Wrapped, i));
            lua_rawseti(L, nTable, i + 1);
        }
    }
    catch (IllegalArgumentException &e)
    {
        return luaL_error(L, "Illegal sequence passed");
    }
    catch (ArrayIndexOutOfBoundsException &e)
    {
        return luaL_error(L, "Seq index out of bounds, should be from 1 to %d", (int)nLength);
    }
    catch (RuntimeException &e)
    {
        return luaL_error(L, "Failed to convert value to Lua value");
    }
    return 1;
}


static int luno_seq_insert(lua_State *L)
{
    LunoSequence *p = luno_seq_getudata(L, 1);
    CHECK_Seq(p);
    
    int pos = 0;
    int value = 0;
    const int top = lua_gettop(L);
    
    Runtime runtime;
    try
    {
        Reference< XIdlArray > xIdlArray(p->xIdlArray);
        
        const int length = xIdlArray->getLen(p->Wrapped);
        if (top == 3)
        {
            value = 3;
            pos = luaL_checkint(L, 2);
        }
        else if (top == 2)
        {
            value = 2;
            pos = length + 1; // no index specified, append
        }
        else
            return luaL_error(L, "Illegal number of arguments passed");
        
        if (pos < 1 || (pos > length +1))
            return luaL_error(L, "Insertion position is illegal (%d)", pos);
        
        const Reference< XIdlClass > xIdlClass(xIdlArray, UNO_QUERY);
        const Reference< XIdlClass > xCompIdl(xIdlClass->getComponentType());
        if (!xCompIdl.is())
            return luaL_error(L, "Component type of the sequence is illegal");
        const Type t(xCompIdl->getTypeClass(), xCompIdl->getName());
        
        Any a = runtime.luaToAny(L, value);
        Any converted;
        
        if (a.isExtractableTo(t))
            converted = a;
         else
            converted = runtime.getImple()->xTypeConverter->convertTo(a, t);
        
        xIdlArray->realloc(p->Wrapped, length + 1);
        
        // move values
        for (sal_Int32 i = length + 1; i > pos; --i)
            xIdlArray->set(p->Wrapped, i -1, xIdlArray->get(p->Wrapped, i -2));
        
        xIdlArray->set(p->Wrapped, pos - 1, converted);
    }
    catch (com::sun::star::script::CannotConvertException &e)
    {
        return luaL_error(L, "Can not converted, %s, reason: %d, position: %d", 
                    OUSTRTOSTR(e.Message), (int)e.Reason, (int)e.ArgumentIndex);
    }
    catch (IllegalArgumentException &e)
    {
        return luaL_error(L, "Value could not be assigned");
    }
    catch (ArrayIndexOutOfBoundsException &e)
    {
        return luaL_error(L, OUSTRTOSTR(e.Message));
    }
    catch (RuntimeException &e)
    {
        return luaL_error(L, "Failed to convert value to UNO value");
    }
    return 0;
}


static int luno_seq_remove(lua_State *L)
{
    LunoSequence *p = luno_seq_getudata(L, 1);
    CHECK_Seq(p);
    
    const int top = lua_gettop(L);
    
    Runtime runtime;
    try
    {
        const Reference< XIdlArray > xIdlArray(p->xIdlArray);
        
        const int length = xIdlArray->getLen(p->Wrapped);
        if (length < 1)
            return luaL_error(L, "Attempt to remove value from empty sequence");
        int pos = length;
        if (top > 1)
            pos = luaL_checkint(L, 2);
        if (pos < 1 || pos > length)
            return luaL_error(L, "Illegal position specified (%d)", pos);
        
        runtime.anyToLua(L, xIdlArray->get(p->Wrapped, pos -1));
        
        for (sal_Int32 i = pos -1; i < length -1; ++i)
            xIdlArray->set(p->Wrapped, i, xIdlArray->get(p->Wrapped, i +1));
        
        xIdlArray->realloc(p->Wrapped, length - 1);
    }
    catch (com::sun::star::script::CannotConvertException &e)
    {
        return luaL_error(L, "Can not converted, %s, reason: %d, position: %d", 
                    OUSTRTOSTR(e.Message), (int)e.Reason, (int)e.ArgumentIndex);
    }
    catch (IllegalArgumentException &e)
    {
        return luaL_error(L, "Value could not be assigned");
    }
    catch (ArrayIndexOutOfBoundsException &e)
    {
        return luaL_error(L, OUSTRTOSTR(e.Message));
    }
    catch (RuntimeException &e)
    {
        return luaL_error(L, "Failed to convert value to UNO value");
    }
    return 1;
}


static int luno_seq_index(lua_State *L)
{
    LunoSequence *p = luno_seq_getudata(L, 1);
    CHECK_Seq(p);
    const int nType = lua_type(L, 2);
    if (nType == LUA_TNUMBER)
    {
        const int n = lua_tointeger(L, 2); // index
        
        const sal_Int32 nLength = p->xIdlArray->getLen(p->Wrapped);
        if (1 <= n && n <= nLength)
        {
            try
            {
                Runtime runtime;
                Any a = p->xIdlArray->get(p->Wrapped, n - 1); // match index with lua
                runtime.anyToLua(L, a);
            }
            catch (IllegalArgumentException &e)
            {
                return luaL_error(L, "Illegal sequence passed");
            }
            catch (ArrayIndexOutOfBoundsException &e)
            {
                return luaL_error(L, "Seq index out of bounds, should be from 1 to %d (%d)", (int)nLength, n);
            }
            catch (RuntimeException &e)
            {
                return luaL_error(L, "Failed to convert value to Lua value");
            }
        }    
        else
            return luaL_error(L, "Seq index out of bounds, should be from 1 to %d (%d)", (int)nLength, n);
    }
    else if (nType == LUA_TSTRING)
    {
        const char *name = lua_tostring(L, 2);
        if (strcmp(name, "insert") == 0)
            lua_pushcfunction(L, luno_seq_insert);
        else if (strcmp(name, "remove") == 0)
            lua_pushcfunction(L, luno_seq_remove);
        else if (strcmp(name, "realloc") == 0)
            lua_pushcfunction(L, luno_seq_realloc);
        else if (strcmp(name, "totable") == 0)
            lua_pushcfunction(L, luno_seq_totable);
        else if (strcmp(name, "tostring") == 0)
            lua_pushcfunction(L, luno_seq_tobytestring);
        else if (strcmp(name, "type") == 0)
            lua_pushstring(L, OUSTRTOSTR(p->Wrapped.getValueTypeName()));
        else
            lua_pushnil(L);
    }
    return 1;
}


static int luno_seq_newindex(lua_State *L)
{
    LunoSequence *p = luno_seq_getudata(L, 1);
    CHECK_Seq(p);
    const int n = luaL_checkint(L, 2);
    if (lua_gettop(L) != 3)
        return luaL_error(L, "Illegal length of arguments");
    
    const sal_Int32 nLength = p->xIdlArray->getLen(p->Wrapped);
    if (1 <= n && n <= nLength)
    {
        Runtime runtime;
        try
        {
            const Reference< XIdlClass > xIdlClass(p->xIdlArray, UNO_QUERY);
            const Reference< XIdlClass > xCompIdl(xIdlClass->getComponentType());
            if (!xCompIdl.is())
                return luaL_error(L, "Component type of the sequence is illegal");
            const Type t(xCompIdl->getTypeClass(), xCompIdl->getName());
            
            Any a = runtime.luaToAny(L, 3);
            
            if (a.isExtractableTo(t))
                p->xIdlArray->set(p->Wrapped, n - 1, a); // match index with lua
            else
            {
                Any converted = runtime.getImple()->xTypeConverter->convertTo(a, t);
                p->xIdlArray->set(p->Wrapped, n - 1, converted);
            }
        }
        catch (com::sun::star::script::CannotConvertException &e)
        {
            return luaL_error(L, "Can not converted, %s, reason: %d, position: %d", 
                        OUSTRTOSTR(e.Message), (int)e.Reason, (int)e.ArgumentIndex);
        }
        catch (IllegalArgumentException &e)
        {
            return luaL_error(L, "Value could not be assigned");
        }
        catch (ArrayIndexOutOfBoundsException &e)
        {
            return luaL_error(L, "Seq index out of bounds, should be from 1 to %d (%d)", (int)nLength, n);
        }
        catch (RuntimeException &e)
        {
            return luaL_error(L, "Failed to convert value to UNO value");
        }
    }
    else
        return luaL_error(L, "Seq index out of bounds, should be from 1 to %d (%d)", (int)nLength, n);
    return 0;
}


static const luaL_Reg luno_seq_funcs[] = 
{
    {"realloc", luno_seq_realloc}, 
    {"totable", luno_seq_totable}, 
    {"tostring", luno_seq_tostring}, 
    {"tobytes", luno_seq_tobytes}, 
    {NULL, NULL}
};


static const luaL_Reg luno_seq[] = 
{
    {"__gc", luno_seq_gc}, 
    {"__index", luno_seq_index}, 
    {"__newindex", luno_seq_newindex}, 
    {"__len", luno_seq_len}, // #
    {"__eq", luno_seq_eq}, 
    {"__tostring", luno_seq_tostring}, 
    {"__pairs", luno_seq_ipairs}, // with non-int key
    {"__ipairs", luno_seq_ipairs}, // without non-int key
    {NULL, NULL}
};


static Reference< XServiceTypeDescription2 > luno_get_servicedesc(
        const Runtime &runtime, const OUString &aService)
{
    Reference< XServiceTypeDescription2 > xServiceDesc;
    try
    {
        if (runtime.getImple()->xTypeDescription->hasByHierarchicalName(aService))
        {
            Any a = runtime.getImple()->xTypeDescription->getByHierarchicalName(aService);
            if (a.getValueTypeClass() == TypeClass_INTERFACE)
            {
                const Reference< XTypeDescription > xTd(a, UNO_QUERY);
                if (xTd.is() && xTd->getTypeClass() == TypeClass_SERVICE)
                    xServiceDesc = Reference< XServiceTypeDescription2 >(a, UNO_QUERY);
            }
        }
    }
    catch (com::sun::star::container::NoSuchElementException &e)
    {
        (void)e;
    }
    catch (RuntimeException &e)
    {
        (void)e;
    }
    return xServiceDesc;
}


static Reference< XServiceConstructorDescription > luno_get_ctordesc(
        const Runtime &runtime, const OUString &aService, const OUString &aCtor)
{
    Reference< XServiceConstructorDescription > xCtor;
    const Reference< XServiceTypeDescription2 > xServiceDesc(
            luno_get_servicedesc(runtime, aService));
    if (xServiceDesc.is())
    {
        const Sequence< Reference< XServiceConstructorDescription > > aCtors(
                                    xServiceDesc->getConstructors());
        const sal_Int32 nLength = aCtors.getLength();
        if (nLength)
        {
            const Reference< XServiceConstructorDescription > *pCtors = 
                                    aCtors.getConstArray();
            for (int i = 0; i < nLength; ++i)
            {
                if (pCtors[i]->getName().equals(aCtor))
                    xCtor = pCtors[i];
                    break;
            }
        }
    }
    return xCtor;
}


static int luno_service_ctor(lua_State *L)
{
    Runtime runtime;
    const OUString aService = luno_luatooustring(L, lua_upvalueindex(1));
    
    LunoAdapted *p = luno_proxy_getudata(L, 1, LUNO_META_PROXY);
    CHECK_Interface(p, 1);
    
    // Needs component context at first argument
    const Reference< XComponentContext > xContext(p->Wrapped, UNO_QUERY);
    if (!xContext.is())
        return luaL_error(L, "First argument should be component context");
    
    const Reference< XMultiComponentFactory > xMcf(xContext->getServiceManager());
    if (!xMcf.is())
        return luaL_error(L, "Component factory can not be retrieved");
    
    const int top = lua_gettop(L);
    try
    {
        if (lua_isnoneornil(L, lua_upvalueindex(2)))
        {
            const int nLength = top > 2 ? top - 2 : 0;
            Sequence< Any > aParams(nLength);
            if (nLength)
            {
                Any *pParams = aParams.getArray();
                for (int i = 0; i < nLength; ++i)
                    pParams[i] = runtime.luaToAny(L, i + 2);
            }
            Reference< XInterface > xInterface = xMcf->createInstanceWithArgumentsAndContext(
                            aService, aParams, xContext);
            runtime.anyToLua(L, makeAny(xInterface));
        }
        else
        {
            const OUString aCtor = luno_luatooustring(L, lua_upvalueindex(2));
            const Reference< XServiceConstructorDescription > xCtor(
                        luno_get_ctordesc(runtime, aService, aCtor));
            if (!xCtor.is())
                return luaL_error(L, "Illegal service or constructor (%d, %d)", 
                    OUSTRTOSTR(aService), OUSTRTOSTR(aCtor));
            
            const Sequence< Reference< XParameter > > aParameters = xCtor->getParameters();
            const sal_Int32 nLength = aParameters.getLength();
            if (top < nLength + 1)
                return luaL_error(L, "Illegal number of arguments (%d for %d)", 
                                (int)(top - 1), (int)nLength);
            
            const Reference< XParameter > *pParameters = aParameters.getConstArray();
            Sequence< Any > aParams(nLength);
            Any *pParams = aParams.getArray();
            bool bOutParam = false;
            if (nLength)
            {
                for (int i = 0; i < nLength; ++i)
                {
                    const Reference< XParameter > xParam(pParameters[i]);
                    if (xParam.is() && xParam->isIn())
                    {
                        Any a = runtime.luaToAny(L, i + 2);
                        const Reference< XTypeDescription > xParamTypeDescription = xParam->getType();
                        const Type t(xParamTypeDescription->getTypeClass(), xParamTypeDescription->getName());
                        
                        if (a.isExtractableTo(t))
                            pParams[i] = a;
                        else
                        {
                            Any converted = runtime.getImple()->xTypeConverter->convertTo(a, t);
                            pParams[i] = converted;
                        }
                    }
                    else
                        bOutParam = true;
                }
            }
            Reference< XInterface > xInterface = xMcf->createInstanceWithArgumentsAndContext(
                            aService, aParams, xContext);
            runtime.anyToLua(L, makeAny(xInterface));
            
            int nOutParams = 0;
            if (bOutParam)
            {
                for (sal_Int32 i = 0; i < nLength; ++i)
                {
                    const Reference< XParameter > xParam(pParameters[i]);
                    if (xParam.is() && xParam->isOut())
                    {
                        runtime.anyToLua(L, pParams[i]);
                        ++nOutParams;
                    }
                }
            }
            return nOutParams + 1;
        }
    }
    catch (com::sun::star::script::CannotConvertException &e)
    {
        return luaL_error(L, "Can not converted, %s, reason: %d, position: %d", 
                    OUSTRTOSTR(e.Message), (int)e.Reason, (int)e.ArgumentIndex);
    }
    catch (IllegalArgumentException &e)
    {
        return luaL_error(L, "Value could not be assigned (%d)", (int)e.ArgumentPosition);
    }
    catch (RuntimeException &e)
    {
        return luaL_error(L, "Failed to convert value to UNO value");
    }
    catch (com::sun::star::uno::Exception &e)
    {
        runtime.anyToLua(L, makeAny(e));
        return lua_error(L);
    }
    return 1;
}


#define LUNO_FIELD_MODULE_NAME "__modulename"


static int luno_require(lua_State *L)
{
    size_t length;
    const char *name = luaL_checklstring(L, 1, &length);
    const OUString aTypeName(OUString(name, length, RTL_TEXTENCODING_UTF8));
    try
    {
        OUString aTmpTypeName, aValueName;
        const sal_Int32 ends = aTypeName.lastIndexOfAsciiL(".", 1);
        if (ends >= 0)
        {
            aTmpTypeName = aTypeName.copy(0, ends);
            aValueName = aTypeName.copy(ends + 1);
        }
        
        Runtime runtime;
        Any a;
        try
        {
            if (runtime.getImple()->xTypeDescription->hasByHierarchicalName(aTypeName))
                a = runtime.getImple()->xTypeDescription->getByHierarchicalName(aTypeName);
            else
            {
                const Reference< XServiceTypeDescription2 > xServiceDesc(
                        luno_get_servicedesc(runtime, aTmpTypeName));
                if (xServiceDesc.is())
                {
                    const Sequence< Reference< XServiceConstructorDescription > > aCtors(
                                                    xServiceDesc->getConstructors());
                    const sal_Int32 nLength = aCtors.getLength();
                    if (nLength)
                    {
                        Reference< XServiceConstructorDescription > xCtor;
                        const Reference< XServiceConstructorDescription > *pCtors = 
                                                aCtors.getConstArray();
                        for (int i = 0; i < nLength; ++i)
                        {
                            if (pCtors[i]->getName().equals(aValueName))
                                xCtor = pCtors[i];
                                break;
                        }
                        if (xCtor.is())
                        {
                            lua_pushstring(L, OUSTRTOSTR(aTmpTypeName));
                            lua_pushstring(L, OUSTRTOSTR(aValueName));
                            lua_pushcclosure(L, luno_service_ctor, 2);
                            return 1;
                        }
                        else
                            return luaL_error(L, "Unknown constructor name (%s)", name);
                    }
                    else if (aValueName.equalsAscii("create"))
                    {
                        // service without constructor definitions
                        lua_pushstring(L, OUSTRTOSTR(aTmpTypeName));
                        lua_pushcclosure(L, luno_service_ctor, 1);
                        return 1;
                    }
                }
                return luaL_error(L, "Unknown type name (%s)", name);
            }
        }
        catch (com::sun::star::container::NoSuchElementException &e)
        {
            return luaL_error(L, "Unknown type name (%s)", name);
        }
        if (a.getValueTypeClass() == TypeClass_INTERFACE)
        {
            const Reference< XTypeDescription > xTd(a, UNO_QUERY);
            if (!xTd.is())
                return luaL_error(L, "unknown type name (%s)", name);
            switch(xTd->getTypeClass())
            {
            case TypeClass_STRUCT:
            case TypeClass_EXCEPTION:
            {
                Any aStruct;
                const Reference< XIdlClass > xIdlClass(
                    runtime.getImple()->xCoreReflection->forName(aTypeName), UNO_QUERY);
                if (xIdlClass.is())
                {
                    // create ctor for struct/exception, with type name as upvalue
                    lua_pushstring(L, name);
                    lua_pushcclosure(L, luno_struct_new, 1);
                }
                else
                    return luaL_error(L, "Maybe template type required (%s)", name);
                break;
            }
            case TypeClass_INTERFACE:
            {
                // interface name can be checked
                lua_pushstring(L, name);
                break;
            }
            case TypeClass_SERVICE:
            case TypeClass_MODULE:
            case TypeClass_CONSTANTS:
            case TypeClass_ENUM:
            {
                // module table with module name
                lua_newtable(L);
                SET_METATABLE(LUNO_META_MODULE);
                
                lua_pushstring(L, LUNO_FIELD_MODULE_NAME);
                lua_pushstring(L, OUSTRTOSTR(aTypeName));
                lua_rawset(L, -3);
                break;
            }
            default:
                // ToDo return as Type?
                return luaL_error(L, "Unknown type (%s)", name);
                break;
            }
        }
        else
        {
            // an enum or a constant
            Any desc = runtime.getImple()->xTypeDescription->getByHierarchicalName(aTmpTypeName);
            if (desc.getValueTypeClass() == TypeClass_INTERFACE)
            {
                const Reference< XTypeDescription > xTdm(desc, UNO_QUERY);
                if (xTdm.is())
                {
                    if (xTdm->getTypeClass() == TypeClass_ENUM)
                    {
                        lua_pushcfunction(L, luno_enum_new);
                        luaL_getmetatable(L, LUNO_META_VALUE); // push dummy table
                        lua_pushstring(L, OUSTRTOSTR(aTmpTypeName));
                        lua_pushstring(L, OUSTRTOSTR(aValueName));
                        lua_call(L, 3, 1);
                    }
                    else if (xTdm->getTypeClass() == TypeClass_CONSTANTS)
                        runtime.anyToLua(L, a);
                }
                else
                    return luaL_error(L, "Illegal type name (%s)", OUSTRTOSTR(aTmpTypeName));
            }
            else
                return luaL_error(L, "Illegal type name (%s)", OUSTRTOSTR(aTmpTypeName));
        }
    }
    catch (com::sun::star::container::NoSuchElementException &e)
    {
        return luaL_error(L, "Unknown type name (%s)", name);
    }
    catch (RuntimeException &e)
    {
        return luaL_error(L, OUSTRTOSTR(e.Message));
    }
    return 1;
}


static int luno_require_loader(lua_State *L)
{
    luno_require(L);
    return 1;
}


static int luno_require_searcher(lua_State *L)
{
    // ToDo check the pass represents a value or check the name is not a path?
    lua_pushcfunction(L, (lua_CFunction)luno_require_loader);
    return 1;
}


static int luno_module_index(lua_State *L)
{
    if (!lua_istable(L, 1))
        return luaL_argerror(L, 1, "table desired");
    
    lua_pushcfunction(L, luno_require);
    
    // construct module.name.key as the argument to import
    lua_pushstring(L, LUNO_FIELD_MODULE_NAME);
    lua_rawget(L, 1);
    
    lua_pushstring(L, ".");
    lua_pushvalue(L, 2); // key to top
    lua_concat(L, 3);
    
    lua_call(L, 1, 1);
    
    // set loaded value to table field
    lua_pushstring(L, lua_tostring(L, 2));
    lua_pushvalue(L, -2); // module table value
    lua_rawset(L, 1);
    return 1;
}


static const luaL_Reg luno_module[] = {
    {"__index", luno_module_index}, 
    {NULL, NULL}
};


static int luno_get_typename(lua_State *L)
{
    if (!lua_isuserdata(L, 1))
        return luaL_argerror(L, 1, "Illegal data passed, userdata is desired");
    LunoValue *p = *(LunoValue **)lua_touserdata(L, 1);
    if (p == NULL)
    {
        lua_pushstring(L, "userdata");
        return 1;
    }
    
    const char *type_name;
    switch (luno_get_udata_type(L, 1))
    {
        case LUNO_TYPE_ENUM:
            type_name = "enum";
            break;
        case LUNO_TYPE_TYPE:
            type_name = "type";
            break;
        case LUNO_TYPE_CHAR:
            type_name = "char";
            break;
        case LUNO_TYPE_INTERFACE:
            type_name = "interface";
            break;
        case LUNO_TYPE_STRUCT:
            if (p->Wrapped.getValueTypeClass() == TypeClass_STRUCT)
                type_name = "struct";
            else
                type_name = "exception";
            break;
        case LUNO_TYPE_SEQ:
            type_name = "sequence";
            break;
        case LUNO_TYPE_ANY:
            type_name = OUSTRTOSTR(p->Wrapped.getValueTypeName());
            break;
        case LUNO_TYPE_UNDEFINED:
            type_name = "userdata";
            break;
        default:
            type_name = "userdata";
            break;
    }
    lua_pushstring(L, type_name);
    return 1;
}


static int luno_get_name(lua_State *L)
{
    if (lua_isfunction(L, 1))
    {
        const char *name = lua_getupvalue(L, 1, 1);
        if (name == NULL)
            lua_pushstring(L, "");
    }
    else
        return luaL_argerror(L, 1, "Should be ctor function");
    return 1;
}


static int luno_dir(lua_State *L)
{
    if (!lua_isuserdata(L, 1))
        return luaL_argerror(L, 1, "Userdata wraps interface, struct or exception required");
    lua_getfield(L, -1, "__elements");
    return 1;
}


static int luno_asuno(lua_State *L)
{
    if (!lua_istable(L, 1))
        return luaL_argerror(L, 1, "Table required to register to wrap as UNO component");
    
    const int top = lua_gettop(L);
    // check already registered
    LUNO_PUSH_REGISTERED();
    lua_pushvalue(L, 1); // as key
    lua_rawget(L, -2);
    const int notfound = lua_isnil(L, -1);
    lua_pop(L, 1);
    
    const int regist = top > 1 ? (lua_isboolean(L, 2) && lua_toboolean(L, 2)) : 1;
    if (notfound && regist)
    {
        lua_pushvalue(L, 1); // key
        lua_pushinteger(L, 1);
        lua_rawset(L, -3); // LUNO_REG_REGISTERD[table] = 1
    }
    else if (!notfound && !regist)
    {
        lua_pushvalue(L, 1); // key
        lua_pushnil(L); // unregister
        lua_rawset(L, -3); // LUNO_REG_REGISTERD[table] = nil
    }
    lua_settop(L, top);
    lua_pushvalue(L, 1);
    return 1;
}


static const luaL_Reg luno_lib[] = {
    {"absolutize", luno_absolutize}, 
    {"tourl", luno_tourl}, 
    {"tosystempath", luno_tosystempath}, 
    {"uuid", luno_uuid}, 
    {"getcontext", luno_getcontext}, 
    {"require", luno_require}, 
    {"typename", luno_get_typename}, 
    {"dir", luno_dir},  // reads list of element names from __elements field
    {"name", luno_get_name}, 
    {"asuno", luno_asuno}, 
    {NULL, NULL}
};


#if LUA_VERSION_NUM >= 502
#define SEARCHERS_NAME "searchers"
#else
#define SEARCHERS_NAME "loaders"
#endif


#if LUA_VERSION_NUM >= 502
#define NEW_LIB(name,funcsreg) luaL_newlib(L, funcsreg)
#else
#define NEW_LIB(name,funcsreg) luaL_register(L, name, funcsreg)
#endif


#define VALUE_AS_CALLABLE_TABLE_WITH_FUNCS(name,ctor,funcsreg) \
    lua_pushstring(L, name); \
    lua_newtable(L); \
    SET_FUNCS(L, funcsreg); \
    lua_pushstring(L, "new"); \
    lua_pushcfunction(L, ctor); \
    lua_rawset(L, -3); \
    lua_createtable(L, 0, 1); \
    lua_pushstring(L, "__call"); \
    lua_pushcfunction(L, ctor); \
    lua_rawset(L, -3); \
    lua_setmetatable(L, -2); \
    lua_rawset(L, top)


#define VALUE_AS_CALLABLE_TABLE(name,ctor) \
    lua_pushstring(L, name); \
    lua_newtable(L); \
    lua_pushstring(L, "new"); \
    lua_pushcfunction(L, ctor); \
    lua_rawset(L, -3); \
    lua_createtable(L, 0, 1); \
    lua_pushstring(L, "__call"); \
    lua_pushcfunction(L, ctor); \
    lua_rawset(L, -3); \
    lua_setmetatable(L, -2); \
    lua_rawset(L, top)


extern "C"
{

LUNO_DLLEXPORT int luaopen_luno(lua_State *L)
{
    NEW_LIB("luno", luno_lib);
    const int top = lua_gettop(L);
    
    NEW_METATABLE(LUNO_META_PROXY,  luno_proxy);
    NEW_METATABLE(LUNO_META_MODULE, luno_module);
    NEW_METATABLE(LUNO_META_STRUCT, luno_struct);
    NEW_METATABLE(LUNO_META_SEQ,    luno_seq);
    NEW_METATABLE(LUNO_META_VALUE,  luno_value);
    NEW_METATABLE(LUNO_META_TYPES,  luno_types);
    
    // add Values as table which is callable
    VALUE_AS_CALLABLE_TABLE("Enum", luno_enum_new);
    VALUE_AS_CALLABLE_TABLE("Type", luno_type_new);
    VALUE_AS_CALLABLE_TABLE("Char", luno_char_new);
    VALUE_AS_CALLABLE_TABLE("Any",  luno_any_new);
    VALUE_AS_CALLABLE_TABLE_WITH_FUNCS("Seq",  luno_seq_new, luno_seq_funcs);
    
    lua_settop(L, top);
    
    // register require hook
    lua_getglobal(L, "table");
    if (lua_istable(L, -1))
    {
        lua_getglobal(L, "package");
        if (lua_istable(L, -1))
        {
            lua_pushstring(L, "insert");
            lua_rawget(L, -3);
            if (lua_isfunction(L, -1))
            {
                lua_pushstring(L, SEARCHERS_NAME);
                lua_rawget(L, -3);
                if (lua_istable(L, -1))
                {
                    lua_pushcfunction(L, (lua_CFunction)luno_require_searcher);
                    lua_call(L, 2, 0);
                }
            }
        }
    }
    
    // add table to keep wrapped table and adapter
    // keeps wrapped tables
    lua_pushstring(L, LUNO_REG_WRAPPED);
    lua_newtable(L);
    lua_rawset(L, LUA_REGISTRYINDEX);
    //luaL_getsubtable(L, LUA_REGISTRYINDEX, LUNO_REG_WRAPPED);
    
    // keeps adapter instance
    lua_pushstring(L, LUNO_REG_ADAPTERS);
    lua_newtable(L);
    lua_rawset(L, LUA_REGISTRYINDEX);
    //luaL_getsubtable(L, LUA_REGISTRYINDEX, LUNO_REG_ADAPTERS);
    
    // keeps tables registerd as wrapping
    /*
    if (lua_getsubtable(L, LUA_REGISTRYINDEX, LUNO_REG_REGISTERD))
    {
        lua_pushstring(L, "__mode");
        lua_pushstring(L, "k");
        lua_rawset(L, -3);
    }
    */
    lua_pushstring(L, LUNO_REG_REGISTERD);
    lua_newtable(L);
    lua_pushstring(L, "__mode");
    lua_pushstring(L, "k");
    lua_rawset(L, -3);
    lua_rawset(L, LUA_REGISTRYINDEX);
    
    lua_settop(L, top);
    return 1;
}

}
