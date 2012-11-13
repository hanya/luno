
require 'rbconfig'
require 'rake/packagetask'
require 'rake/clean'
require 'rubygems'

luajit = true
REPOSITORY_BASE = "https://github.com/downloads/hanya/luno/"

#-- Commands

ZIP = "zip"
ZIP_ARGS = "-9"

cat = "cat"

#-- SDK and office variables

TARGET_VERSION = "3.4"
OXT_EXT = "oxt"

sdk_home = ENV['OO_SDK_HOME']
ure_home = ENV['OO_SDK_URE_HOME']
base_path = ENV['OFFICE_BASE_PROGRAM_PATH']

CPPU_INCLUDE = './include'
CPPUMAKER = 'cppumaker'
CPPUMAKER_FLAG = "#{CPPU_INCLUDE}/flag"

sdk_include = "#{sdk_home}/include"

#-- Lua variables

LUA_DIR = ENV.include?("LUA_DIR") ? ENV["LUA_DIR"] : "./lua/installed"

lua_exe = "luajit-2.0.0"
lua_lib = nil

if ENV.include?("LUA_DIR")
  lua_include = "#{LUA_DIR}/src"
  lua_link_lib = "#{LUA_DIR}/installed/lib"
else
  lua_include = "" # ToDo
  lua_link_lib = "" # ToDo
end

#-- Compiler and linker variables, would be overwritten

cc   = "g++"
link = "g++"
cxx  = "g++"

obj_ext = "o"
lib_ext = "so"

lib_prefix = "lib"

optflags   = ENV.include?("OPTFLAGS") ? ENV["OPTFLAGS"] : nil
debugflags = ENV.include?("DEBUGFLAGS") ? ENV["DEBUGFLAGS"] : nil

#-- Local directories

SRC_DIR   = "./src"
BUILD_DIR = "./build"
LIB_DIR   = "./build/lib"
LIB2_DIR  = "./build/lib2"
OBJ_DIR   = "./build/obj"
DOC_DIR   = "./build/doc"
LDOC_DIR  = "./ldoc"

directory BUILD_DIR
directory OBJ_DIR
directory LIB_DIR
directory LIB2_DIR

ALL_SRCS     = FileList["#{SRC_DIR}/*.cxx"]
MODULE_SRCS  = FileList["#{SRC_DIR}/module.cxx"]
LOADER_SRCS  = FileList["#{SRC_DIR}/loader.cxx"]
LIB_SRCS     = FileList["#{SRC_DIR}/*.cxx"] - MODULE_SRCS - LOADER_SRCS
STD_MOD_SRCS = FileList["#{SRC_DIR}/*.cxx"] - LOADER_SRCS

#-- Environment specific variables

if ENV['HOST']
  host = ENV['HOST']
else
  host = RbConfig::CONFIG['host']
end

if host.include? 'linux'
  
  platform = "Linux"
  platform += "_" + (host.include?('x86_64') ? 'x86_64' : 'x86')
  
  psep = "/"
  coutflag = "-o "
  optflags = "-O3" if optflags.nil?
  cflags = "-fPIC"
  debugflags = "" if debugflags.nil?
  cxxflags = ""
  compileflag = "-c"
  
  ccdefs = "-DUNX -DGCC -DLINUX -DCPPU_ENV=gcc3 "
  #-DHAVE_GCC_VISIBILITY_FEATURE"
  
  flags = "#{cflags} #{optflags} #{debugflags} #{cxxflags} -fno-strict-aliasing -Wall "
  incflags = "-I. -I#{lua_include} -I#{sdk_include} -I#{CPPU_INCLUDE} "
  
  link_flags = "-shared"
  out_flag = "-o "
  lib_ldflags = ""
  libflag = "-L"
  
  sdk_lib = "#{libflag}#{sdk_home}/lib"
  ure_lib = "#{libflag}#{ure_home}/lib"
  
  sdk_libs = " -luno_cppuhelpergcc3 -luno_cppu -luno_salhelpergcc3 -luno_sal -lm "
  ldflags = "-rdynamic -Wl,-export-dynamic"
  
  uno_link_flags = "'-Wl,-rpath,$ORIGIN' " + 
                   "-Wl,--version-script,#{sdk_home}/settings/component.uno.map"
  #uno_link_flags = "'-Wl,-rpath,$ORIGIN'"
  
  #loader_cflags = "-fpic"
  #loader_flags = "-fvisibility=hidden"
  
  LIB = ""
  libs = luajit ? "-lluajit-5.1" : "-llua"
  
  luno_libs = "-lluno"
  
  local_link_lib = ""
  
  module_ldflags = ""
  LINK_SWITCH = "-L"
  
  ure_types = "#{ure_home}/share/misc/types.rdb"
  
  lua_lib = "libluajit-5.1.#{lib_ext}.2"
  #lua_lib = "liblua.#{lib_ext}"
  lua_lib_dir_prefix = "lib"
  
elsif host.include? 'mswin'
  
  platform = "Windows_x86"
  
  psep = "\\"
  cc = "cl"
  link = "link"
  cxx = "cl"
  mt = "mt -nologo "
  
  obj_ext = "obj"
  lib_ext = "dll"
  lib_prefix = "lib"
  
  #lib_dll = "luno.#{lib_ext}"
  
  out_flag = "/OUT:"
  
  coutflag = "/Fo"
  optflags = "/Ob2" if optflags.nil?
  cflags = "/MD /Zi /EHsc /Zm600 /Zc:forScope,wchar_t- " + 
           "-wd4251 -wd4275 -wd4290 -wd4675 -wd4786 -wd4800 -Zc:forScope"
  debugflags = "" if debugflags.nil?
  cxxflags = ""
  compileflag = "/c"
  
  ccdefs = "-DWIN32 -DWNT -D_DLL -DCPPU_V=msci -DCPPU_ENV=msci"
  
  flags = "#{cflags} #{optflags} #{debugflags} #{cxxflags} "
  incflags = "/I. /I#{lua_include} /I#{sdk_include} /I#{CPPU_INCLUDE}"
  
  sdk_lib = "/LIBPATH:#{sdk_home}/lib"
  ure_lib = ""
  
  libluno_def = "#{SRC_DIR}/libluno.def"
  
  sdk_libs = "icppuhelper.lib icppu.lib isal.lib isalhelper.lib " + 
             "msvcprt.lib msvcrt.lib kernel32.lib"
  ldflags = " /DLL /NODEFAULTLIB:library /DEBUGTYPE:cv "
  lib_ldflags = " #{ldflags} /MAP /DEF:#{libluno_def} "
  link_flags = "/DLL"
  libflag = "/LIBPATH:"
  
  uno_link_flags = "/DEF:#{sdk_home}/settings/component.uno.def"
  
  LIB = ""
  libs = luajit ? "lua51.lib" : "lua.lib"
  
  luno_libs = "libluno.lib"
  
  local_link_lib = ""
  
  module_ldflags = ""
  LINK_SWITCH = "/LIBPATH:"
  
  ure_types = "#{ure_home}/misc/types.rdb"
  
  cat = "type"
  lua_exe = "luajit.exe"
  lua_lib = "lua51.#{lib_ext}"
  lua_lib_dir_prefix = "bin"
end

platform.downcase!

#-- Common settings

lib_dll    = "#{lib_prefix}luno.#{lib_ext}"
MODULE_DLL = "luno.#{lib_ext}"
loader_dll = "lualoader.uno.#{lib_ext}"

ALL_OBJS     = ALL_SRCS.ext(obj_ext)
MODULE_OBJS  = MODULE_SRCS.ext(obj_ext)
LOADER_OBJS  = LOADER_SRCS.ext(obj_ext)
LIB_OBJS     = LIB_SRCS.ext(obj_ext)
STD_MOD_OBJS = STD_MOD_SRCS.ext(obj_ext)

CLEAN.include(ALL_OBJS)

#-- Tasks

rule ".#{obj_ext}" => '.cxx' do |t|
  sh "#{cc} #{flags} #{ccdefs} #{incflags} " + 
     "#{coutflag}#{t.name} #{compileflag} #{t.source} "
end

task :default => [:all]
task :all => [:ext]


types = [
  "com.sun.star.uno.TypeClass", 
  "com.sun.star.beans.XIntrospection", 
  "com.sun.star.beans.XMaterialHolder", 
  "com.sun.star.beans.XPropertySet", 
  "com.sun.star.beans.MethodConcept", 
  "com.sun.star.beans.PropertyConcept", 
  "com.sun.star.container.XHierarchicalNameAccess", 
  "com.sun.star.container.XNameAccess", 
  "com.sun.star.lang.XServiceInfo", 
  "com.sun.star.lang.XSingleServiceFactory", 
  "com.sun.star.lang.XSingleComponentFactory", 
  "com.sun.star.lang.XTypeProvider", 
  "com.sun.star.lang.XMultiServiceFactory", 
  "com.sun.star.lang.XUnoTunnel", 
  "com.sun.star.reflection.InvocationTargetException", 
  "com.sun.star.reflection.XServiceTypeDescription2", 
  "com.sun.star.reflection.XServiceConstructorDescription", 
  "com.sun.star.reflection.XIdlReflection", 
  "com.sun.star.reflection.XTypeDescription", 
  "com.sun.star.reflection.XIdlField2", 
  "com.sun.star.reflection.ParamMode", 
  "com.sun.star.reflection.XEnumTypeDescription", 
  "com.sun.star.registry.XRegistryKey", 
  "com.sun.star.script.XInvocation2", 
  "com.sun.star.script.XInvocationAdapterFactory2", 
  "com.sun.star.script.XTypeConverter", 
  "com.sun.star.uno.XComponentContext", 
  "com.sun.star.uno.XWeak", 
  "com.sun.star.uno.XAggregation", 
]

desc "generate headers"
task :header do
  if types.length > 0
    unless File.exists?(CPPUMAKER_FLAG)
      puts "flag for cppumaker does not exist. starting cppumaker..."
      office_types = "#{base_path}/offapi.rdb"
      sh "#{CPPUMAKER} -Gc -BUCR -O#{CPPU_INCLUDE} " + 
         "-T\"#{types.join(';')}\" \"#{ure_types}\" \"#{office_types}\""
      sh "echo > #{CPPUMAKER_FLAG}"
    end
  end
end

task :luno => [*MODULE_OBJS]  do
  sh "#{link} #{link_flags} #{out_flag}#{LIB_DIR}/#{MODULE_DLL} #{ALL_OBJS.join(' ')} " + 
     " #{LIB} #{local_link_lib} #{module_ldflags} #{libflag}#{lua_link_lib} " + 
     " #{sdk_lib} #{ure_lib}" + 
     " #{sdk_libs} #{libs} "
end

task :luno_std_mod => [*STD_MOD_OBJS] do
  sh "#{link} #{link_flags} #{out_flag}#{LIB_DIR}/#{MODULE_DLL} #{STD_MOD_OBJS.join(' ')} " + 
     " #{LIB} #{local_link_lib} #{module_ldflags} #{libflag}#{lua_link_lib} " + 
     " #{sdk_lib} #{ure_lib}" + 
     " #{sdk_libs} #{libs} "
end

task :ext => [:header, :luno_std_mod]


#-- src package for LuaRocks

ROCK_VERSION = `#{cat} "VERSION.rock"`
ROCK_DIR = "luno-#{ROCK_VERSION}"
ROCK_NAME = "#{ROCK_DIR}.rockspec"

directory "#{BUILD_DIR}/#{ROCK_DIR}"

task :rock => [:header, ] do
  rockdir = "#{BUILD_DIR}/#{ROCK_DIR}"
  mkdir rockdir
  srcdest = "#{BUILD_DIR}/#{ROCK_DIR}/src"
  mkdir srcdest
  cp ["Makefile", "Makefile.win", "LICENSE", "README.md", "VERSION.rock"], rockdir
  cp_r "#{CPPU_INCLUDE}", rockdir
  cp_r "ldoc", rockdir
  #cp_r "#{DOC_DIR}", rockdir
  cp ["#{SRC_DIR}/uno.lua", "#{SRC_DIR}/adapter.cxx", "#{SRC_DIR}/runtime.cxx", "#{SRC_DIR}/types.cxx", 
      "#{SRC_DIR}/module.cxx", "#{SRC_DIR}/luno.hxx", "#{SRC_DIR}/luno.def"], srcdest
  
  sh "chdir #{BUILD_DIR} && tar czvpf #{ROCK_DIR}.tar.gz #{ROCK_DIR}/"
  sh "md5sum #{BUILD_DIR}/#{ROCK_DIR}.tar.gz > #{BUILD_DIR}/md5-#{ROCK_VERSION}.txt"
  
  md5 = `#{cat} "#{BUILD_DIR}/md5-#{ROCK_VERSION}.txt"`
  data = `#{cat} "rockspec.template"`
  data.gsub!("##VERSION##", ROCK_VERSION)
  data.gsub!("##MD5##", md5.split(" ")[0])
  open("#{BUILD_DIR}/luno-#{ROCK_VERSION}.rockspec", "w") do |f|
    f.write(data)
    f.flush
  end
end


### File creations

## Templates

TEMPLATE_MANIFEST = <<EOD
<?xml version="1.0" encoding="UTF-8"?>
<manifest:manifest>
  <manifest:file-entry manifest:full-path="${FULL_PATH}" 
   manifest:media-type="${MEDIA_TYPE}"/>
</manifest:manifest>
EOD

TEMPLATE_COMPONENTS = <<EOD
<?xml version="1.0" encoding="UTF-8"?>
<components xmlns="http://openoffice.org/2010/uno-components">
  <component loader="${LOADER_NAME}" uri="${URI}">
    <implementation name="${IMPLE_NAME}">
      ${SERVICES}
    </implementation>
  </component>
</components>
EOD

TEMPLATE_DESCRIPTION = <<EOD
<?xml version="1.0" encoding="UTF-8"?>
<description xmlns="http://openoffice.org/extensions/description/2006"
             xmlns:xlink="http://www.w3.org/1999/xlink"
             xmlns:d="http://openoffice.org/extensions/description/2006">
  <identifier value="${ID}"/>
  <version value="${VERSION}"/>
  <platform value="${PLATFORM}"/>
  <dependencies>
    <OpenOffice.org-minimal-version value="${TARGET}" d:name="OpenOffice.org ${TARGET}"/>
  </dependencies>
  <registration>
    <simple-license accept-by="admin" default-license-id="this" suppress-on-update="true">
      <license-text xlink:href="LICENSE" license-id="this"/>
    </simple-license>
  </registration>
  <display-name>
    <name lang="en">${DISPLAY_NAME}</name>
  </display-name>
  <extension-description>
    <src lang="en" xlink:href="descriptions/desc.en"/>
  </extension-description>
  <update-information>
    <src xlink:href="${REPOSITORY_BASE}${FEED}.update.xml"/>
  </update-information>
</description>
EOD

TEMPLATE_FEED = <<EOD
<?xml version="1.0" encoding="UTF-8"?>
<description xmlns="http://openoffice.org/extensions/update/2006"
             xmlns:xlink="http://www.w3.org/1999/xlink"
             xmlns:d="http://openoffice.org/extensions/description/2006">
  <identifier value="${ID}"/>
  <version value="${VERSION}"/>
  <platform value="${PLATFORM}"/>
  <dependencies>
    <d:OpenOffice.org-minimal-version value="${TARGET}" d:name="OpenOffice.org ${TARGET}"/>
  </dependencies>
  <update-download>
    <src xlink:href="${REPOSITORY_BASE}${PKG_NAME}"/>
  </update-download>
</description>
EOD

def update_feed_task(path, id, target, version, pkg_name, platform)
  data = String.new TEMPLATE_FEED
  data.gsub!(/\$\{ID\}/, id)
  data.gsub!(/\$\{VERSION\}/, version)
  data.gsub!(/\$\{PLATFORM\}/, platform)
  data.gsub!(/\$\{TARGET\}/, target)
  data.gsub!(/\$\{PKG_NAME\}/, pkg_name)
  data.gsub!(/\$\{REPOSITORY_BASE\}/, REPOSITORY_BASE)
  open(path, "w") do |f|
    f.write(data)
    f.flush
  end
end

# Creates description.xml
def description_task(path, id, target, display_name, version, feedname, platform)
  data = String.new TEMPLATE_DESCRIPTION
  data.gsub!(/\$\{ID\}/, id)
  data.gsub!(/\$\{VERSION\}/, version)
  data.gsub!(/\$\{PLATFORM\}/, platform)
  data.gsub!(/\$\{TARGET\}/, target)
  data.gsub!(/\$\{DISPLAY_NAME\}/, display_name)
  data.gsub!(/\$\{REPOSITORY_BASE\}/, REPOSITORY_BASE)
  data.gsub!(/\$\{FEED\}/, feedname)
  open(path, "w") do |f|
    f.write(data)
    f.flush
  end
end

# Creates META-INF/manifest.xml
def manifest_task(manifest_file, full_path, media_type)
  data = String.new TEMPLATE_MANIFEST
  data.gsub!(/\$\{FULL_PATH\}/, full_path)
  data.gsub!(/\$\{MEDIA_TYPE\}/, media_type)
  open(manifest_file, "w") do |f|
    f.write(data)
    f.flush
  end
end

# Creates components.component
def registration_task(path, file_name, loader_name, uri, imple_name, services)
  #puts "registration"
  data = String.new TEMPLATE_COMPONENTS
  data.gsub!(/\$\{LOADER_NAME\}/, loader_name)
  data.gsub!(/\$\{URI\}/, uri)
  data.gsub!(/\$\{IMPLE_NAME\}/, imple_name)
  services.map! do |name|
    "<service name=\"#{name}\"/>"
  end
  data.gsub!(/\$\{SERVICES\}/, services.join("\n"))
  open(path, "w") do |f|
    f.write(data)
    f.flush
  end
end


# Creates description/desc.en
def package_description_task(path, desc)
  desc_dir = File.dirname(path)
  mkdir_p desc_dir unless File.exist?(desc_dir)
  open(path, "w") do |f|
    f.write(desc)
    f.flush
  end
end


# Pack as zip archive
def packaging_task(dir_path, pkg_name)
  chdir dir_path do
    sh "#{ZIP} #{ZIP_ARGS} -r -o ../#{pkg_name} * **/*"
  end
end

### Loader package

LOADER_VERSION = `#{cat} "VERSION.loader"`

LOADER_PKG_NAME         = "LuaLoader"
LOADER_PKG_DISPLAY_NAME = "Lua Loader"
LOADER_PKG_ID           = "mytools.loader.Lua"
LOADER_PKG_DESC         = "The UNO component loader written in Lua."

LOADER_REGISTRATION  = "LuaLoader.components"
LOADER_IMPLE_NAME    = "mytools.loader.Lua"
LOADER_SERVICE_NAMES = ["com.sun.star.loader.Lua"]
LOADER_LUA           = "lualoader.lua"
LOADER_MEDIA_TYPE    = "application/vnd.sun.star.uno-components;platform=#{platform}"

LOADER_PKG_DIR_NAME  = "#{LOADER_PKG_NAME}-#{platform}-#{LOADER_VERSION}"
LOADER_PKG_FULL_NAME = "#{LOADER_PKG_DIR_NAME}.#{OXT_EXT}"
LOADER_PKG_DIR       = "#{BUILD_DIR}/#{LOADER_PKG_DIR_NAME}"
LOADER_PKG           = "#{BUILD_DIR}/#{LOADER_PKG_FULL_NAME}"
LOADER_PKG_PLATFORM  = "#{LOADER_PKG_NAME}-#{platform}"

LOADER_PKG_DIR_LIB  = "#{LOADER_PKG_DIR}/libs"
LOADER_LIB_LUNO_DLL = "#{LIB2_DIR}/#{lib_dll}"
LOADER_MODULE_DLL   = "#{LIB2_DIR}/#{MODULE_DLL}"
LOADER_LIB_DLL      = "#{LIB2_DIR}/#{loader_dll}"

UNO_LUA = "uno.lua"

directory LOADER_PKG_DIR
directory LOADER_PKG_DIR_LIB

task :loader => [:header, LOADER_PKG]

LOADER_PKG_REGISTRATION = "#{LOADER_PKG_DIR_LIB}/#{LOADER_REGISTRATION}"
LOADER_PKG_MANIFEST     = "#{LOADER_PKG_DIR}/META-INF/manifest.xml"
LOADER_PKG_DESCRIPTION  = "#{LOADER_PKG_DIR}/description.xml"
LOADER_PKG_EXT_DESC     = "#{LOADER_PKG_DIR}/descriptions/desc.en"

LOADER_PKG_LIB          = "#{LOADER_PKG_DIR_LIB}/#{lib_dll}"
LOADER_PKG_MODULE       = "#{LOADER_PKG_DIR_LIB}/#{MODULE_DLL}"
LOADER_PKG_LOADER       = "#{LOADER_PKG_DIR_LIB}/#{loader_dll}"
LOADER_PKG_LOADER_LUA   = "#{LOADER_PKG_DIR_LIB}/#{LOADER_LUA}"
LOADER_PKG_UNO          = "#{LOADER_PKG_DIR_LIB}/#{UNO_LUA}"

LOADER_PKG_README       = "#{LOADER_PKG_DIR}/README"
LOADER_PKG_COPYRIGHT    = "#{LOADER_PKG_DIR}/LICENSE"

exe_name = lua_exe.match(/^[^-]*/)
LOADER_LUA_EXE  = "#{LOADER_PKG_DIR}/bin/#{exe_name}"
#LOADER_LUA_EXE  = "#{LOADER_PKG_DIR}/bin/#{lua_exe}"
LOADER_LUA_LIB  = "#{LOADER_PKG_DIR_LIB}/#{lua_lib}"
LOADER_LUA_LIBS = "#{LOADER_PKG_DIR}/jit"
LOADER_LUA_INCLUDE = "#{LOADER_PKG_DIR}/include"

LUA_INCLUDES = ["lua.h", "lualib.h", "luaconf.h", "lua.hpp"]
LUA_INCLUDES << "luajit.h" if luajit

LOADER_UPDATE_FEED = "#{BUILD_DIR}/#{LOADER_PKG_PLATFORM}.update.xml"

directory "#{LOADER_PKG_DIR}/META-INF"
directory "#{LOADER_PKG_DIR}/descriptions"
directory "#{LOADER_PKG_DIR}/bin"
directory "#{LOADER_PKG_DIR}/jit"
directory "#{LOADER_PKG_DIR}/include"


desc "create libluno library"
file LOADER_LIB_LUNO_DLL => [LIB2_DIR, *LIB_OBJS] do |t|
  sh "#{link} #{link_flags} #{out_flag}#{t.name} #{LIB_OBJS.join(' ')} " +
     " #{LIB} #{lib_ldflags} " +
     " #{sdk_lib} #{ure_lib} #{libflag}#{lua_link_lib}" +
     " #{sdk_libs} #{libs} "
  if host.include? 'mswin'
    sh "#{mt} -manifest #{t.name}.manifest -outputresource:#{t.name};2"
  end
end

desc "luno.so for loader"
#file LOADER_MODULE_DLL => [LIB2_DIR, *MODULE_OBJS, "#{LIB2_DIR}/#{lib_dll}"] do |t|
file LOADER_MODULE_DLL => [LIB2_DIR, "#{LOADER_LIB_LUNO_DLL}", *MODULE_OBJS] do |t|
  #p "building luno.so"
  sh "#{link} #{link_flags} #{out_flag}#{LIB2_DIR}/#{MODULE_DLL} #{MODULE_OBJS.join(' ')} " +
     " #{LIB} #{local_link_lib} #{module_ldflags} " +
     " #{sdk_lib} #{ure_lib} #{LINK_SWITCH}#{LIB2_DIR} #{libflag}#{lua_link_lib}" +
     " #{sdk_libs} #{libs} #{luno_libs}"
  if host.include? 'mswin'
    sh "#{mt} -manifest #{t.name}.manifest -outputresource:#{t.name};2"
  end
end


desc "create loader component"
file LOADER_LIB_DLL => [LIB2_DIR, *LOADER_OBJS] do |t|
  #p "building loader"
  sh "#{link} #{link_flags} #{out_flag}#{LIB2_DIR}/#{loader_dll} #{LOADER_OBJS.join(' ')} " +
     " #{LIB} #{local_link_lib} #{ldflags} #{uno_link_flags}" +
     " #{sdk_lib} #{ure_lib} #{LINK_SWITCH}#{LIB2_DIR} #{libflag}#{lua_link_lib} " +
     " #{sdk_libs} #{libs}  #{luno_libs} "
  if host.include? 'mswin'
    sh "#{mt} -manifest #{t.name}.manifest -outputresource:#{t.name};2"
  end
end

file LOADER_PKG_REGISTRATION => [LOADER_PKG_DIR, LOADER_PKG_DIR_LIB] do |t|
  registration_task(t.name, LOADER_REGISTRATION,
      "com.sun.star.loader.SharedLibrary", loader_dll,
      LOADER_IMPLE_NAME, LOADER_SERVICE_NAMES)
end

file LOADER_PKG_MANIFEST => [LOADER_PKG_DIR, "#{LOADER_PKG_DIR}/META-INF"] do |t|
  manifest_task(t.name, "libs/#{LOADER_REGISTRATION}", LOADER_MEDIA_TYPE)
end

file LOADER_PKG_DESCRIPTION => [LOADER_PKG_DIR] do |t|
  description_task(t.name, LOADER_PKG_ID, TARGET_VERSION, 
                   LOADER_PKG_DISPLAY_NAME, LOADER_VERSION, LOADER_PKG_PLATFORM, platform)
end

file LOADER_PKG_EXT_DESC => [LOADER_PKG_DIR, "#{LOADER_PKG_DIR}/descriptions"] do |t|
  package_description_task(t.name, LOADER_PKG_DESC)
end

file LOADER_PKG_LIB => [LOADER_LIB_LUNO_DLL, LOADER_PKG_DIR_LIB] do |t|
  cp t.prerequisites[0], t.name
end

file LOADER_PKG_MODULE => [LOADER_MODULE_DLL, LOADER_PKG_DIR_LIB] do |t|
  cp t.prerequisites[0], t.name
end

file LOADER_PKG_LOADER => [LOADER_LIB_DLL, LOADER_PKG_DIR_LIB] do |t|
  cp t.prerequisites[0], t.name
end

file LOADER_PKG_LOADER_LUA => ["#{SRC_DIR}/#{LOADER_LUA}", LOADER_PKG_DIR_LIB] do |t|
  cp t.prerequisites[0], t.name
end

file LOADER_PKG_UNO => ["#{SRC_DIR}/#{UNO_LUA}", LOADER_PKG_DIR_LIB] do |t|
  cp t.prerequisites[0], t.name
end

file LOADER_PKG_README => ["./README.loader"] do |t|
  cp t.prerequisites[0], t.name
end

file LOADER_PKG_COPYRIGHT => [".#{psep}LICENSE", "#{LUA_DIR}#{psep}COPYRIGHT"] do |t|
  sh "#{cat} #{t.prerequisites[0]} >> #{t.name}"
  sh "#{cat} #{t.prerequisites[1]} >> #{t.name}"
end

file LOADER_LUA_EXE => ["#{LUA_DIR}/installed/bin/#{lua_exe}", "#{LOADER_PKG_DIR}/bin"] do |t|
  exe_name = lua_exe.match(/^[^-]*/)
  cp t.prerequisites[0], "#{File.dirname(t.name)}/#{exe_name}"
  if not host.include?('mswin')
    sh "chmod a+x #{File.dirname(t.name)}/#{exe_name}"
    #sh "ln -sf #{t.name} #{File.dirname(t.name)}/luajit"
  end
  if host.include? 'mswin'
    cp "#{LUA_DIR}/installed/#{lua_lib_dir_prefix}/#{lua_lib}", "#{LOADER_PKG_DIR}/bin/#{lua_lib}"
  end
end

file LOADER_LUA_LIB => ["#{LUA_DIR}/installed/#{lua_lib_dir_prefix}/#{lua_lib}", 
                        LOADER_PKG_DIR_LIB] do |t|
  cp t.prerequisites[0], t.name
  #cp "#{LUA_DIR}/lib/libluajit-5.1.so.2", "#{LOADER_PKG_DIR_LIB}/libluajit-5.1.so.2"
  #cp "#{LUA_DIR}/lib/libluajit-5.1.so.2.0.0", "#{LOADER_PKG_DIR_LIB}/libluajit-5.1.so.2.0.0"
  if host.include? 'mswin'
    d = "#{LUA_DIR}/src/#{lua_lib}"
    cp d.gsub(/\.[^.]+$/, ".lib"), t.name.gsub(/\.[^.]+$/, ".lib")
	cp d.gsub(/\.[^.]+$/, ".exp"), t.name.gsub(/\.[^.]+$/, ".exp")
  end
end

file LOADER_LUA_LIBS => ["#{LUA_DIR}#{psep}src#{psep}jit", LOADER_PKG_DIR] do |t|
  cp Dir.glob((t.prerequisites[0] + "#{psep}*").gsub(psep, "/")), t.name
end

file LOADER_LUA_INCLUDE => ["#{LUA_DIR}"] do |t|
  LUA_INCLUDES.each do |t|
    cp "#{LUA_DIR}/src/#{t}", "#{LOADER_LUA_INCLUDE}/#{t}"
  end
end

file LOADER_UPDATE_FEED => [BUILD_DIR] do |t|
  update_feed_task(t.name, LOADER_PKG_ID, TARGET_VERSION, LOADER_VERSION, LOADER_PKG_FULL_NAME, platform)
end


file LOADER_PKG => [LOADER_PKG_DIR, 
                    LOADER_PKG_LIB, LOADER_PKG_MODULE, LOADER_PKG_LOADER, 
                    LOADER_PKG_LOADER_LUA, LOADER_PKG_UNO, 
                    LOADER_PKG_REGISTRATION, LOADER_PKG_MANIFEST, LOADER_PKG_DESCRIPTION, 
                    LOADER_PKG_EXT_DESC, LOADER_PKG_README, LOADER_PKG_COPYRIGHT, 
                    LOADER_LUA_EXE, LOADER_LUA_LIB, LOADER_LUA_LIBS, LOADER_LUA_INCLUDE, 
                    LOADER_UPDATE_FEED] do |t|
  packaging_task t.name.sub(/.oxt$/, ""), File.basename(t.name)
end


### Script provider package

SCRIPT_VERSION = `#{cat} "VERSION.script"`

SCRIPT_PKG_NAME         = "LuaScriptProvider"
SCRIPT_PKG_DISPLAY_NAME = "Lua Script Provider"
SCRIPT_PACKAGE_ID       = "mytools.script.provider.ScriptProviderForLua"

SCRIPT_REGISTRATION = "ScriptProviderForLua.components"
SCRIPT_DESC         = "The script provider for Lua."
SCRIPT_IMPLE_NAME   = "mytools.script.provider.ScriptProviderForLua"
SCRIPT_SERVICE_NAMES = [
  "com.sun.star.script.provider.ScriptProviderForLua",
  "com.sun.star.script.provider.LanguageScriptProvider"]
SCRIPT_LUA = "luascript.lua"
SCRIPT_MEDIA_TYPE = "application/vnd.sun.star.uno-components"

SCRIPT_PKG_DIR_NAME  = "#{SCRIPT_PKG_NAME}-#{SCRIPT_VERSION}"
SCRIPT_PKG_FULL_NAME = "#{SCRIPT_PKG_NAME}-#{SCRIPT_VERSION}.#{OXT_EXT}"
SCRIPT_PKG_DIR       = "#{BUILD_DIR}/#{SCRIPT_PKG_DIR_NAME}"
SCRIPT_PKG           = "#{BUILD_DIR}/#{SCRIPT_PKG_FULL_NAME}"

SCRIPT_PKG_DIR_LIB = "#{SCRIPT_PKG_DIR}/libs"

directory SCRIPT_PKG_DIR
directory SCRIPT_PKG_DIR_LIB

task :provider => [SCRIPT_PKG]


SCRIPT_PKG_REGISTRATION = "#{SCRIPT_PKG_DIR_LIB}/#{SCRIPT_REGISTRATION}"
SCRIPT_PKG_MANIFEST     = "#{SCRIPT_PKG_DIR}/META-INF/manifest.xml"
SCRIPT_PKG_DESCRIPTION  = "#{SCRIPT_PKG_DIR}/description.xml"
SCRIPT_PKG_EXT_DESC     = "#{SCRIPT_PKG_DIR}/descriptions/desc.en"
SCRIPT_PKG_SCRIPT_LUA   = "#{SCRIPT_PKG_DIR_LIB}/#{SCRIPT_LUA}"

SCRIPT_PKG_README       = "#{SCRIPT_PKG_DIR}/README"
SCRIPT_PKG_COPYRIGHT    = "#{SCRIPT_PKG_DIR}/LICENSE"

SCRIPT_UPDATE_FEED      = "#{BUILD_DIR}/#{SCRIPT_PKG_NAME}.update.xml"

directory "#{SCRIPT_PKG_DIR}/META-INF"
directory "#{SCRIPT_PKG_DIR}/descriptions"


file SCRIPT_PKG_REGISTRATION => [SCRIPT_PKG_DIR, SCRIPT_PKG_DIR_LIB] do |t|
  registration_task(t.name, SCRIPT_REGISTRATION,
      "com.sun.star.loader.Lua", SCRIPT_LUA,
      SCRIPT_IMPLE_NAME, SCRIPT_SERVICE_NAMES)
end

file SCRIPT_PKG_MANIFEST => [SCRIPT_PKG_DIR, "#{SCRIPT_PKG_DIR}/META-INF"] do |t|
  manifest_task(t.name, "libs/#{SCRIPT_REGISTRATION}", SCRIPT_MEDIA_TYPE)
end

file SCRIPT_PKG_DESCRIPTION => [SCRIPT_PKG_DIR] do |t|
  description_task(t.name, SCRIPT_PACKAGE_ID, TARGET_VERSION, 
                SCRIPT_PKG_DISPLAY_NAME, SCRIPT_VERSION, SCRIPT_PKG_NAME, "all")
end

file SCRIPT_PKG_EXT_DESC => [SCRIPT_PKG_DIR, "#{SCRIPT_PKG_DIR}/descriptions"] do |t|
  package_description_task(t.name, SCRIPT_DESC)
end

file SCRIPT_PKG_SCRIPT_LUA => ["#{SRC_DIR}/#{SCRIPT_LUA}", 
                                SCRIPT_PKG_DIR, SCRIPT_PKG_DIR_LIB] do |t|
  cp t.prerequisites[0], t.name
end

file SCRIPT_PKG_README => ["./README.script"] do |t|
  cp t.prerequisites[0], t.name
end

file SCRIPT_PKG_COPYRIGHT => ["./LICENSE"] do |t|
  cp t.prerequisites[0], t.name
end

file SCRIPT_UPDATE_FEED => [BUILD_DIR] do |t|
  update_feed_task(t.name, SCRIPT_PACKAGE_ID, TARGET_VERSION, SCRIPT_VERSION, SCRIPT_PKG_FULL_NAME, "all")
end


file SCRIPT_PKG => [SCRIPT_PKG_DIR, SCRIPT_PKG_DIR_LIB, 
                    SCRIPT_PKG_SCRIPT_LUA, SCRIPT_PKG_REGISTRATION, 
                    SCRIPT_PKG_EXT_DESC, SCRIPT_PKG_DESCRIPTION, SCRIPT_PKG_MANIFEST, 
                    SCRIPT_PKG_README, SCRIPT_PKG_COPYRIGHT, 
                    SCRIPT_UPDATE_FEED] do |t|
  packaging_task t.name.sub(/.oxt$/, ""), File.basename(t.name)
end

=begin
### Documents

LDOCS = FileList["#{LDOC_DIR}/*.md"]
HTML_DOCS = LDOCS.pathmap("#{DOC_DIR}/%n.html")

#DOC_TITLES = {}

#CLEAN.include(HTML_DOCS)

directory DOC_DIR


rule '.html' => lambda{|f| f.sub(/^(.*)\.html$/, "#{LDOC_DIR}/\\1.md")} do |t|
  print("rule?")
  sh "pandoc #{t.source} -f markdown -t html -c ./ldoc.css -T LUNO" + 
     " > #{t.pathname('')}"
  
end

file "#{DOC_DIR}/install.html" => "#{LDOC_DIR}/install.md"
file "#{DOC_DIR}/luno.html" => "#{LDOC_DIR}/luno.md"
file "#{DOC_DIR}/ipc.html" => "#{LDOC_DIR}/ipc.md"


#file HTML_DOCS => LOCS

HTML_DOCS.zip(LDOCS).each do |h, m|
  #sh "pandoc #{t.prerequisites[0]} -f markdown -t html -c ./ldoc.css -T LUNO" + 
  #   " > #{t.name}"
  file h => m do |t|
    sh "pandoc #{m} -f markdown -t html5 -c ./ldoc.css -T LUNO" + 
       " > #{h}"
  end
end

task "#{DOC_DIR}/ldoc.css" do |t|
  cp "#{LDOC_DIR}/ldoc.css", t.name
end


desc "Generates documents"
task :ldocs => [DOC_DIR, "#{DOC_DIR}/ldoc.css", *HTML_DOCS]
#[DOC_DIR, "#{DOC_DIR}/index.html"]
#[*HTML_DOCS]
=end
