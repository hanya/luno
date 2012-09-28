
#include "luno.hxx"

#include <osl/module.hxx>
#include <osl/file.hxx>

#include <cppuhelper/implementationentry.hxx>
#include <rtl/strbuf.hxx>

using com::sun::star::uno::Exception;
using com::sun::star::uno::Reference;
using com::sun::star::uno::RuntimeException;
using com::sun::star::uno::Sequence;
using com::sun::star::uno::XComponentContext;
using com::sun::star::uno::XInterface;

using rtl::OUString;
using rtl::OUStringToOString;
using rtl::OStringBuffer;


#define OUSTRCONST(s) ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM(s))
#define OUSTRTOSTR(s) OUStringToOString(s, RTL_TEXTENCODING_UTF8).getStr()

#define IMPLE_NAME "mytools.loader.Lua"
#define SERVICE_NAME "com.sun.star.loader.Lua"


namespace lualoader
{

inline static OUString luno_luatooustring(lua_State *L, const int index)
{
    size_t length;
    const char *s = luaL_checklstring(L, index, &length);
    return OUString(s, length, RTL_TEXTENCODING_UTF8);
}


static void getLibraryPath(OUString &aPath) throw (RuntimeException)
{
    OUString aLibURL;
    ::osl::Module::getUrlFromAddress(
        reinterpret_cast< oslGenericFunction >(getLibraryPath), aLibURL);
    aLibURL = aLibURL.copy(0, aLibURL.lastIndexOf('/'));
    osl::FileBase::RC e = osl::FileBase::getSystemPathFromFileURL(aLibURL, aPath);
    if (e != osl::FileBase::E_None)
        throw RuntimeException(
            OUSTRCONST("Failed to retrieve path to the loader."),
            Reference< XInterface >());
}


OUString getImplementationName()
{
    return OUSTRCONST(IMPLE_NAME);
}


Sequence< OUString > getSupportedServiceNames()
{
    OUString name(RTL_CONSTASCII_USTRINGPARAM(SERVICE_NAME));
    return Sequence< OUString >(&name, 1);
}

#define LUNO_LOADER_CTOR "luno.loader.ctor"

static lua_State *g_L = NULL;

Reference< XInterface > createInstanceWithContext(const Reference< XComponentContext > &xContext) 
    throw (::com::sun::star::uno::Exception)
{
    Reference< XInterface > ret;
    if (g_L == NULL)
    {
        OStringBuffer buff;
        OUString aSysPath;
        getLibraryPath(aSysPath);
        
        // creates new state for each loader?
        // Lua initialize, when closed?
#if LUA_VERSION_NUM >= 502
        g_L = luaL_newstate();
#else
        g_L = lua_open();
#endif
        //luaL_checkversion(g_L);
        if (!luno::Runtime::isInitialized())
        {
            try
            {
                luno::Runtime::initialize(g_L, xContext);
            }
            catch (RuntimeException &e)
            {
                throw Exception(OUSTRCONST("Failed to initialize runtime"), 
                            Reference< XInterface >());
            }
            catch (Exception &e)
            {
                throw;
            }
        }
        luaL_openlibs(g_L);
        
        buff.append(OUSTRTOSTR(aSysPath))
            .append(SAL_PATHDELIMITER)
            .append("lualoader.lua");
        const int error = luaL_loadfile(g_L, buff.makeStringAndClear());
        if (error)
        {
            OUString aMessage = luno_luatooustring(g_L, -1);
            lua_settop(g_L, 0);
            lua_close(g_L);
            g_L = NULL;
            throw Exception(aMessage, Reference< XInterface >());
        }
        
        buff.append(OUSTRTOSTR(aSysPath))
            .append(SAL_PATHDELIMITER);
        lua_pushstring(g_L, buff.makeStringAndClear());
        lua_pushstring(g_L, SAL_DLLEXTENSION);
        
        const int e = lua_pcall(g_L, 2, 1, 0);
        if (e)
        {
            OUString aMessage = luno_luatooustring(g_L, -1);
            lua_settop(g_L, 0);
            lua_close(g_L);
            g_L = NULL;
            throw Exception(aMessage, Reference< XInterface >());
        }
        // store lualoader ctor into the registry
        if (lua_type(g_L, -1) == LUA_TFUNCTION)
        {
            lua_pushstring(g_L, LUNO_LOADER_CTOR);
            lua_pushvalue(g_L, -2);
            lua_rawset(g_L, LUA_REGISTRYINDEX);
        }
        lua_settop(g_L, 0);
    }
    if (g_L != NULL && luno::Runtime::isInitialized())
    {
        lua_pushstring(g_L, LUNO_LOADER_CTOR);
        lua_rawget(g_L, LUA_REGISTRYINDEX);
        
        // ToDo keep loader generator function into the registry?
        if (lua_type(g_L, 1) == LUA_TFUNCTION)
        {
            luno::Runtime runtime;
            runtime.anyToLua(g_L, makeAny(xContext));
            
            const int er = lua_pcall(g_L, 1, 1, 0);
            if (er)
            {
                OUString aMessage = luno_luatooustring(g_L, -1);
                lua_settop(g_L, 0);
                throw Exception(aMessage, Reference< XInterface >());
            }
            try
            {
                runtime.luaToAny(g_L, 1) >>= ret;
            }
            catch (RuntimeException &e)
            {
                lua_settop(g_L, 0);
                throw Exception(e.Message, Reference< XInterface >());
            }
        }
        lua_settop(g_L, 0);
    }
    return ret;
}

} // lualoader

static struct cppu::ImplementationEntry g_entries[] =
{
    {
        lualoader::createInstanceWithContext,
        lualoader::getImplementationName,
        lualoader::getSupportedServiceNames,
        cppu::createSingleComponentFactory,
        0,
        0
    },
    {0, 0, 0, 0, 0, 0}
};

extern "C"
{

void SAL_CALL
component_getImplementationEnvironment(const sal_Char **ppEnvTypeName, uno_Environment **)
{
    *ppEnvTypeName = CPPU_CURRENT_LANGUAGE_BINDING_NAME;
}

void* SAL_CALL
component_getFactory(const sal_Char *pImplName, void *pServiceManager, void *pRegistryKey)
{
    return cppu::component_getFactoryHelper(pImplName, pServiceManager, pRegistryKey, g_entries);
}

}
