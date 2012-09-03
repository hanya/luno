

#ifndef _IMPLE_HXX
#define _IMPLE_HXX

#include "lua.hpp"

#ifdef WIN32
#  define LUNO_DLLEXPORT __declspec(dllexport)
#else
#  define LUNO_DLLEXPORT
#endif

#ifndef LUNO_INTROSPECTION_BASED
#define LUNO_INTROSPECTION_BASED
#endif

#include <rtl/ustring.hxx>
#include <rtl/textenc.h>

#include <cppuhelper/implbase2.hxx>
#include <cppuhelper/weakref.hxx>

#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/beans/XIntrospection.hpp>
#include <com/sun/star/container/XHierarchicalNameAccess.hpp>
#include <com/sun/star/lang/XSingleServiceFactory.hpp>
#include <com/sun/star/lang/XUnoTunnel.hpp>
#include <com/sun/star/reflection/InvocationTargetException.hpp>
#include <com/sun/star/reflection/XIdlReflection.hpp>
#include <com/sun/star/reflection/XIdlField2.hpp>
#include <com/sun/star/reflection/XTypeDescription.hpp>
#include <com/sun/star/script/XInvocation2.hpp>
#include <com/sun/star/script/XInvocationAdapterFactory2.hpp>
#include <com/sun/star/script/XTypeConverter.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>

#define OUSTRCONST(s) ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM(s))

// Metatable names
#define LUNO_META_PROXY  "luno.proxy"
#define LUNO_META_ANY    "luno.any"
#define LUNO_META_SEQ    "luno.seq"
#define LUNO_META_MODULE "luno.module"
#define LUNO_META_STRUCT "luno.struct"
#define LUNO_META_VALUE  "luno.value"

#define LUNO_META_TYPES  "luno.types"

// Internal types
enum LUNO_TYPE
{
    LUNO_TYPE_UNDEFINED = 0, 
    LUNO_TYPE_INTERFACE = 1, 
    LUNO_TYPE_STRUCT    = 2, 
    LUNO_TYPE_ENUM      = 3, 
    LUNO_TYPE_TYPE      = 4, 
    LUNO_TYPE_CHAR      = 5, 
    LUNO_TYPE_BYTES     = 6, 
    LUNO_TYPE_ANY       = 7, 
    LUNO_TYPE_SEQ       = 8, 
    //LUNO_TYPE_TYPES     = 9
};


// Key names for tables stored in the registry
#define LUNO_REG_WRAPPED   "luno.wrapped"
#define LUNO_REG_ADAPTERS  "luno.adapters"
#define LUNO_REG_REGISTERD "luno.weak"


#define LUNO_PUSH_REG_WRAPPED() \
    lua_pushstring(L, LUNO_REG_WRAPPED); \
    lua_rawget(L, LUA_REGISTRYINDEX)


#define LUNO_PUSH_REG_ADAPTERS() \
    lua_pushstring(L, LUNO_REG_ADAPTERS); \
    lua_rawget(L, LUA_REGISTRYINDEX)


#define LUNO_PUSH_REGISTERED() \
    lua_pushstring(L, LUNO_REG_REGISTERD); \
    lua_rawget(L, LUA_REGISTRYINDEX)


#if LUA_VERSION_NUM >= 502
#define GET_LENGTH(L_,index) lua_rawlen(L_, index)
#else
#define GET_LENGTH(L_,index) lua_objlen(L_, index)
#endif


#if LUA_VERSION_NUM >= 502
#define SET_FUNCS(L_,funcsreg) luaL_setfuncs(L_, funcsreg, 0)
#else
#define SET_FUNCS(L_,funcsreg) luaL_register(L_, NULL, funcsreg)
#endif


#define NEW_METATABLE(metatable,funcsreg) \
    if (luaL_newmetatable(L, metatable)) \
        SET_FUNCS(L, funcsreg)


#if LUA_VERSION_NUM >= 502
#define SET_METATABLE(metatable) \
    luaL_setmetatable(L, metatable)
#else
#define SET_METATABLE(metatable) \
    luaL_getmetatable(L, metatable); \
    lua_setmetatable(L, -2)
#endif


namespace luno
{

typedef struct
{
    com::sun::star::uno::Any Wrapped;
} LunoValue;


typedef struct : public LunoValue
{
    com::sun::star::uno::Reference< com::sun::star::script::XInvocation2 > xInvocation;
} LunoAdapted;


typedef struct : public LunoAdapted
{
    com::sun::star::uno::Reference< com::sun::star::beans::XIntrospectionAccess > xIntrospectionAccess;
    com::sun::star::uno::Reference< com::sun::star::beans::XPropertySet > xPropertySet;
} LunoAdapted2;


typedef struct : public LunoValue
{
    // ToDo make common
    com::sun::star::uno::Reference< com::sun::star::reflection::XIdlArray > xIdlArray;
} LunoSequence;

typedef struct
{
    com::sun::star::uno::Sequence< com::sun::star::uno::Type > aTypes;
    com::sun::star::uno::Sequence< sal_Int8 > aImpleId;
} LunoTypes;


LunoAdapted *luno_proxy_getudata(lua_State *L, const int index, const char *metatable);
LunoAdapted *luno_proxy_newudata(lua_State *L, const char *metatable);
LunoAdapted2 *luno_proxy_newudata2(lua_State *L, const char *metatable);
LunoValue *luno_value_getudata(lua_State *L, const int index);
LunoValue *luno_value_newudata(lua_State *L);
LunoSequence *luno_seq_getudata(lua_State *L, const int index);
LunoSequence *luno_seq_newudata(lua_State *L);
LunoTypes *luno_types_getudata(lua_State *L, const int index);
LunoTypes *luno_types_newudata(lua_State *L);

int luno_get_udata_type(lua_State *L, const int index);


typedef struct
{
    com::sun::star::uno::Reference< com::sun::star::uno::XComponentContext > xComponentContext;
    com::sun::star::uno::Reference< com::sun::star::lang::XSingleServiceFactory> xInvocation;
    com::sun::star::uno::Reference< com::sun::star::script::XTypeConverter > xTypeConverter;
    com::sun::star::uno::Reference< com::sun::star::reflection::XIdlReflection > xCoreReflection;
    com::sun::star::uno::Reference< com::sun::star::container::XHierarchicalNameAccess > xTypeDescription;
    com::sun::star::uno::Reference< com::sun::star::script::XInvocationAdapterFactory2 > xAdapterFactory;
    com::sun::star::uno::Reference< com::sun::star::beans::XIntrospection > xIntrospection;
    bool bInitialized;
} RuntimeImple;


class LUNO_DLLEXPORT Runtime
{
    RuntimeImple *imple;
public:
    
    Runtime() throw(com::sun::star::uno::RuntimeException);
    
    ~Runtime();
    
    static void initialize(lua_State *L, const com::sun::star::uno::Reference < com::sun::star::uno::XComponentContext > &xContext) throw (com::sun::star::uno::RuntimeException);
    static bool isInitialized() throw (com::sun::star::uno::RuntimeException);
    RuntimeImple *getImple() const { return imple; }
    
    void anyToLua(lua_State *L, const com::sun::star::uno::Any &a) const throw (com::sun::star::uno::RuntimeException);
    com::sun::star::uno::Any luaToAny(lua_State *L, int index) const;
};


class LunoAdapter : public cppu::WeakImplHelper2< 
                                com::sun::star::script::XInvocation, 
                                com::sun::star::lang::XUnoTunnel >
{
    lua_State *L;
    int m_WrappedRef; // in LUNO_REG_WRAPPED
    int m_AdapterRef; // in LUNO_REG_ADAPTERS
    ::com::sun::star::uno::Any m_Types;
    ::com::sun::star::uno::Any m_ImpleId;
    
    ::com::sun::star::uno::Sequence< sal_Int16 > getOutParamIndexes(const rtl::OUString &aMethodName) 
        throw (com::sun::star::uno::RuntimeException);
    
    LunoAdapter(lua_State *L_, const int index, 
        const com::sun::star::uno::Any &aTypes, 
        const com::sun::star::uno::Any &aImpleId);
public:
    virtual ~LunoAdapter();
    
    static LunoAdapter *create(lua_State *L_, const int index, 
        const com::sun::star::uno::Sequence< com::sun::star::uno::Type > &aTypes, 
        const com::sun::star::uno::Sequence< sal_Int8 > &aImpleId);
    
    int getWrappedRef() const { return m_WrappedRef; };
    
    com::sun::star::uno::Sequence< com::sun::star::uno::Type > getWrappedTypes() const;
    
    static com::sun::star::uno::Sequence< sal_Int8 > getUnoTunnelImplementationId();
    
    // XInvocation
    virtual com::sun::star::uno::Reference < com::sun::star::beans::XIntrospectionAccess > SAL_CALL getIntrospection() 
        throw (com::sun::star::uno::RuntimeException);
    
    virtual com::sun::star::uno::Any SAL_CALL invoke(
        const rtl::OUString &  aName, 
        const com::sun::star::uno::Sequence < com::sun::star::uno::Any > &aParams, 
        com::sun::star::uno::Sequence < sal_Int16 > &aOutParamIndex, 
        com::sun::star::uno::Sequence < com::sun::star::uno::Any > &aOutParam)
        throw (com::sun::star::lang::IllegalArgumentException, 
               com::sun::star::script::CannotConvertException, 
               com::sun::star::reflection::InvocationTargetException, 
               com::sun::star::uno::RuntimeException);
    
    virtual void SAL_CALL setValue(
        const rtl::OUString &aName, 
        const com::sun::star::uno::Any &aValue) 
        throw (com::sun::star::beans::UnknownPropertyException, 
               com::sun::star::script::CannotConvertException, 
               com::sun::star::reflection::InvocationTargetException, 
               com::sun::star::uno::RuntimeException);
    
    virtual com::sun::star::uno::Any SAL_CALL getValue(
        const rtl::OUString &aName)
        throw (com::sun::star::beans::UnknownPropertyException, 
               com::sun::star::uno::RuntimeException);
        
    virtual sal_Bool SAL_CALL hasMethod(
        const rtl::OUString &aName)
        throw (com::sun::star::uno::RuntimeException);
    
    virtual sal_Bool SAL_CALL hasProperty(
        const rtl::OUString &aName)
        throw (com::sun::star::uno::RuntimeException);
    
    // XUnoTunnel
    virtual sal_Int64 SAL_CALL getSomething(
        const com::sun::star::uno::Sequence < sal_Int8 > &aIdentifier)
        throw (com::sun::star::uno::RuntimeException);
    
};


}

#endif
