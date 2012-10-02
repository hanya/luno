/*
** See Copyright Notice in LICENSE.
**/
#include "luno.hxx"

using namespace com::sun::star::uno;

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

