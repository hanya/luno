
LUA_DIR=/usr/local
LUA_LIBDIR=$(LUA_DIR)/lib/lua/5.1
LUA_SHAREDIR=$(LUA_DIR)/share/lua/5.1
CFLAGS=
PREFIX=/usr/local

SRC=./src

LUA_LIB_SUFFIX=

SDK_DIR=/opt/openoffice.org/basis3.4/sdk
URE_DIR=/opt/openoffice.org/ure

INC_SWITCH=-I

OBJS = $(SRC)$(PS)adapter.$(OBJ_EXT) $(SRC)$(PS)runtime.$(OBJ_EXT) $(SRC)$(PS)module.$(OBJ_EXT)

$(SRC)/luno.$(LIB_EXT) : $(OBJS)
	$(LINK) $(LIB_OPTIONS) $(CC_OUTPUT_SWITCH)$(subst /,$(PS),$@) $(OBJS) $(LIB_SWITCH)$(SDK_DIR)$(PS)lib $(LIB_SWITCH)$(URE_LINK_DIR) $(LIBS) $(LINK_LIBS) $(LUA_LINK_LIB)$(LUA_LIB_SUFFIX)$(LINK_LIB_EXT)


$(SRC)/%.$(OBJ_EXT) : $(SRC)/%.cxx
	$(CC) $(CC_OUTPUT_SWITCH)$(subst /,$(PS),$@) $(CFLAGS) $(INC_SWITCH)$(SDK_DIR)$(PS)include $(CC_INCLUDES) $(INC_SWITCH)$(LUA_INC_DIR) $(CC_SOURCE_SWITCH)$<


install:
#	$(COPY) $(SRC)$(PS)luno.$(LIB_EXT) $(LUA_LIBDIR)$(PS)
#	$(COPY) $(SRC)$(PS)uno.lua $(LUA_SHAREDIR)$(PS)

uninstall:
	$(DEL) $(LUA_LIBDIR)$(PS)luno.$(LIB_EXT)
	$(DEL) $(LUA_SHAREDIR)$(PS)uno.lua

clean:
	$(DEL) $(SRC)$(PS)luno.$(LIB_EXT)
	$(DEL) $(SRC)$(PS)adapter.$(OBJ_EXT)
	$(DEL) $(SRC)$(PS)runtime.$(OBJ_EXT)
	$(DEL) $(SRC)$(PS)module.$(OBJ_EXT)
