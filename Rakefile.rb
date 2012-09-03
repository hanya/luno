
require 'rbconfig'
require 'rake/packagetask'
require 'rake/clean'
require 'rubygems'

luajit = true


VERSION = "0.1.0"
target = "3.4"

OXT_EXT = "oxt"

ZIP = "zip"
ZIP_ARGS = "-9"

sdk_home = ENV['OO_SDK_HOME']
ure_home = ENV['OO_SDK_URE_HOME']
base_path = ENV['OFFICE_BASE_PROGRAM_PATH']


cc = "g++"
link = "g++"
cxx = "g++"

lib_ext = ""
obj_ext = ""

CPPU_INCLUDE = './include'
CPPUMAKER = 'cppumaker'
CPPUMAKER_FLAG = "#{CPPU_INCLUDE}/flag"

sdk_include = "#{sdk_home}/include"

lua_include = "/home/asuka/local/include"
if luajit
  lua_include = "#{lua_include}/luajit-2.0"
end

c = RbConfig::CONFIG

CFLAGS = c['CFLAGS']
cxxflags = CFLAGS + c['CXXFLAGS']
cxxflags.gsub!(/-O\d?/, "")
cxxflags.gsub!(/-ggdb/, "")

host = c['host']

# Local directories
SRC_DIR = "./src"
BUILD_DIR = "./build"
LIB_DIR = "./build/lib"
OBJ_DIR = "./build/obj"
PKG_DIR = "./pkg"
DOC_DIR = "./build/doc"
LDOC_DIR = "./ldoc"

directory BUILD_DIR
directory OBJ_DIR
directory LIB_DIR


ALL_SRCS = FileList["#{SRC_DIR}/*.cxx"]
MODULE_SRCS = FileList["#{SRC_DIR}/module.cxx"]
LOADER_SRCS = FileList["#{SRC_DIR}/loader.cxx"]
LIB_SRCS = FileList["#{SRC_DIR}/*.cxx"] - MODULE_SRCS - LOADER_SRCS
STD_MOD_SRCS = FileList["#{SRC_DIR}/*.cxx"] - LOADER_SRCS

if host.include? 'linux'
  
  platform = "Linux_x86"
  
  obj_ext = "o"
  lib_ext = "so"
  
  lib_dll = "libluno.#{lib_ext}"
  loader_dll = "lualoader.uno.#{lib_ext}"
  
  
  out_flag = ""
  
  coutflag = ""
  optflags = "-O3 "
  cflags = "-fPIC"
  debugflags = ""
  
  cc_defs = "-fno-strict-aliasing -DUNX -DGCC -DLINUX -DCPPU_ENV=gcc3 "
  
  flags = "#{cflags} #{optflags} #{debugflags} #{cxxflags} -o "
  incflags = "-I. -I#{lua_include} " + "-I#{sdk_include} -I#{CPPU_INCLUDE} "
   
  sdk_lib = "-L#{sdk_home}/lib"
  ure_lib = "-L#{ure_home}/lib"
  
  sdk_libs = " -luno_cppuhelpergcc3 -luno_cppu -luno_salhelpergcc3 -luno_sal -lm "
  ldflags = c['LDFLAGS']
  lib_ldflags = ""
  link_flags = "-shared -o "
  
  uno_link_flags = "'-Wl,-rpath,$ORIGIN' -Wl,--version-script,#{sdk_home}/settings/component.uno.map"
  
  LIB = ""
  libs = "-llua"
  if luajit
    libs = "#{libs}jit-5.1"
  end
  
  luno_libs = "-lluno"
  
  #"#{c['LIBS']} #{dldlibs}"
  
  lua_link_lib = "-L/home/asuka/local/lib"
  
  local_link_lib = ""
  
  module_ldflags = ""
  
  ure_types = "#{ure_home}/share/misc/types.rdb"
  
elsif host.include? 'mswin'
  
  platform = "Windows"
  
  obj_ext = "obj"
  lib_ext = "dll"
  
  lib_dll = "luno.#{lib_ext}"
  loader_dll = "lualoader.uno.#{lib_ext}"
  
  out_flag = "/OUT:"
  
  coutflag = "/Fo"
  optflags = "/O3"
  cflags = "/MD /Zi /EHsc /Zm600 /Zc:forScope,wchar_t- -wd4251 -wd4275 -wd4290 -wd4675 -wd4786 -wd4800 -Zc:forScope"
  debugflags = ""
  
  cc_defs = "-DWIN32 -DWNT -D_DLL -DCPPU_V=msci -DCPPU_ENV=msci"
  
  flags = "#{cflags} #{optflags} #{debugflags} #{cxxflags} /OUT:"
  incflags = "/I. /I./include /I#{sdk_include} /I#{CPPU_INCLUDE}"
  
  sdk_lib = "/LIBPATH:#{sdk_home}/lib"
  ure_lib = ""
  
  sdk_libs = "icppuhelper.lib icppu.lib isal.lib isalhelper.lib msvcprt.lib msvcrt.lib kernel32.lib"
  ldflags = " /DLL /NODEFAULTLIB:library /DEBUGTYPE:cv "
  lib_ldflags = ""
  link_flags = ""
  
  uno_link_flags = "/DEF:#{sdk_home}/settings/component.uno.def"
  
  LIB = ""
  libs = "lua.lib"
  if luajit
	libs = "#{libs}jit-5.1"
  end
  
  luno_libs = "luno.lib"
  
  lua_link_lib = "/LIBPATH:"
  
  local_link_lib = ""
  
  module_ldflags = ""
  
  ure_types = "#{ure_home}/misc/types.rdb"
end


MODULE_DLL = "luno.#{lib_ext}"


ALL_OBJS = ALL_SRCS.ext(obj_ext)
MODULE_OBJS = MODULE_SRCS.ext(obj_ext)
LOADER_OBJS = LOADER_SRCS.ext(obj_ext)
LIB_OBJS = LIB_SRCS.ext(obj_ext)
STD_MOD_OBJS = STD_MOD_SRCS.ext(obj_ext)

CLEAN.include(ALL_OBJS)
#CLEAN.include(MODULE_OBJS)
#CLEAN.include(LIB_OBJS)
#CLEAN.include(STD_MOD_OBJS)

rule ".#{obj_ext}" => '.cxx' do |t|
  sh "#{cc} #{incflags} #{cc_defs} #{flags} " + 
     "#{coutflag}#{t.name} -c #{t.source} "
end

task :default => [:all]
task :all => [
  :ext, 
]


types = [
  "com.sun.star.uno.TypeClass", 
  "com.sun.star.beans.XIntrospection", 
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
    sh "#{CPPUMAKER} -Gc -BUCR -O#{CPPU_INCLUDE} -T\"#{types.join(';')}\" \"#{ure_types}\" \"#{office_types}\""
    sh "echo > #{CPPUMAKER_FLAG}"
  end
  end
end

task :luno => [*MODULE_OBJS]  do
  
  sh "#{link} #{link_flags} #{out_flag}#{LIB_DIR}/#{MODULE_DLL} #{ALL_OBJS.join(' ')} " + 
     " #{LIB} #{local_link_lib} #{module_ldflags} #{lua_link_lib} " + 
     " #{sdk_lib} #{ure_lib}" + 
     " #{sdk_libs} #{libs} "
  
end

task :luno_std_mod => [*STD_MOD_OBJS] do
  sh "#{link} #{link_flags} #{out_flag}#{LIB_DIR}/#{MODULE_DLL} #{STD_MOD_OBJS.join(' ')} " + 
     " #{LIB} #{local_link_lib} #{module_ldflags} #{lua_link_lib} " + 
     " #{sdk_lib} #{ure_lib}" + 
     " #{sdk_libs} #{libs} "
end

task :ext => [:header, :luno_std_mod]


## src package for LuaRocks

ROCK_VERSION = `cat "VERSION.rock"`
ROCK_DIR = "luno-#{ROCK_VERSION}"
ROCK_NAME = "#{ROCK_DIR}.rockspec"

directory "#{BUILD_DIR}/#{ROCK_DIR}"

task :rock => [:header, ] do
  rockdir = "#{BUILD_DIR}/#{ROCK_DIR}"
  mkdir rockdir
  srcdest = "#{BUILD_DIR}/#{ROCK_DIR}/src"
  mkdir srcdest
  cp ["Makefile", "LICENSE", "README"], rockdir
  cp_r "#{CPPU_INCLUDE}", rockdir
  cp_r "ldoc", rockdir
  cp_r "#{DOC_DIR}", rockdir
  cp ["#{SRC_DIR}/uno.lua", "#{SRC_DIR}/adapter.cxx", "#{SRC_DIR}/runtime.cxx", 
      "#{SRC_DIR}/module.cxx", "#{SRC_DIR}/luno.hxx", "#{SRC_DIR}/luno.def"], srcdest
  
  sh "chdir #{BUILD_DIR} && tar czvpf #{ROCK_DIR}.tar.gz #{ROCK_DIR}/"
  sh "md5sum #{BUILD_DIR}/#{ROCK_DIR}.tar.gz > #{BUILD_DIR}/md5-#{ROCK_VERSION}.txt"
  
  md5 = `cat "#{BUILD_DIR}/md5-#{ROCK_VERSION}.txt"`
  data = `cat "rockspec.template"`
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
<manifest:file-entry manifest:full-path="${LOADER}" manifest:media-type="${MEDIA_TYPE}"/>
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
<dependencies>
<OpenOffice.org-minimal-version value="${TARGET}" d:name="OpenOffice.org ${TARGET}"/>
</dependencies>
<!--
<registration>
<simple-license accept-by="admin" default-license-id="this" suppress-on-update="true">
<license-text xlink:href="LICENSE" license-id="this"/>
</simple-license>
</registration>
-->
<display-name>
<name lang="en">${DISPLAY_NAME}</name>
</display-name>
<extension-description>
<src lang="en" xlink:href="descriptions/desc.en"/>
</extension-description>
</description>
EOD

# Creates description.xml
def description_task(dir_path, id, target, display_name)
  description_file = "#{dir_path}/description.xml"
  data = String.new TEMPLATE_DESCRIPTION
  data.gsub!(/\$\{ID\}/, id)
  data.gsub!(/\$\{VERSION\}/, VERSION)
  data.gsub!(/\$\{TARGET\}/, target)
  data.gsub!(/\$\{DISPLAY_NAME\}/, display_name)
  open(description_file, "w") do |f|
    f.write(data)
    f.flush
  end
end

# Creates META-INF/manifest.xml
def manifest_task(dir_path, full_path, media_type)
  
  manifest_file = "#{dir_path}/META-INF/manifest.xml"
  manifest_dir = File.dirname(manifest_file)
  mkdir_p manifest_dir unless File.exist?(manifest_dir)
    
  data = String.new TEMPLATE_MANIFEST
  data.gsub!(/\$\{LOADER\}/, full_path)
  data.gsub!(/\$\{MEDIA_TYPE\}/, media_type)
  open(manifest_file, "w") do |f|
    f.write(data)
    f.flush
  end
end

# Creates components.component
def registration_task(dir_path, file_name, loader_name, uri, imple_name, services)
  puts "registration"
  components_f = "#{dir_path}/lib/#{file_name}"
  
  data = String.new TEMPLATE_COMPONENTS
  data.gsub!(/\$\{LOADER_NAME\}/, loader_name)
  data.gsub!(/\$\{URI\}/, uri)
  data.gsub!(/\$\{IMPLE_NAME\}/, imple_name)
  services.map! do |name|
    "<service name=\"#{name}\"/>"
  end
  data.gsub!(/\$\{SERVICES\}/, services.join("\n"))
  open(components_f, "w") do |f|
    f.write(data)
    f.flush
  end
end


# Creates description/desc.en
def extension_description_task(dir_path, desc)
  path = "#{dir_path}/descriptions/desc.en"
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
    sh "#{ZIP} -9 -r -o ../#{pkg_name} * **/*"
  end
end

### Loader package

LOADER_PKG_NAME = "LuaLoader"
LOADER_PKG_DISPLAY_NAME = "Lua Loader"
LOADER_PKG_ID = "mytools.loader.Lua"
LOADER_PKG_DESC = "The UNO component loader written in Lua."

LOADER_REGISTRATION = "LuaLoader.components"
LOADER_IMPLE_NAME = "mytools.loader.Lua"
LOADER_SERVICE_NAMES = ["com.sun.star.loader.Lua"]
LOADER_LUA = "lualoader.lua"

LOADER_PKG_DIR_NAME = "#{LOADER_PKG_NAME}-#{VERSION}"
LOADER_PKG_FULL_NAME = "#{LOADER_PKG_NAME}-#{VERSION}.#{OXT_EXT}"
LOADER_PKG_DIR = "#{PKG_DIR}/#{LOADER_PKG_NAME}-#{VERSION}"
LOADER_PKG = "#{PKG_DIR}/#{LOADER_PKG_FULL_NAME}"

LOADER_PKG_DIR_LIB = "#{LOADER_PKG_DIR}/lib"
LOADER_LIB_LUNO_DLL = "#{LIB_DIR}/#{lib_dll}"
LOADER_MODULE_DLL = "#{LIB_DIR}/#{MODULE_DLL}"
LOADER_LIB_DLL = "#{LIB_DIR}/#{loader_dll}"

directory LOADER_PKG_DIR
directory LOADER_PKG_DIR_LIB

task :loader => [:header, LOADER_PKG]


desc "create libluno library"
file LOADER_LIB_LUNO_DLL => [LIB_DIR, *LIB_OBJS] do |t|
  sh "#{link} #{out_flag}#{t.name} #{LIB_OBJS.join(' ')} " +
     " #{LIB} #{lib_ldflags} " +
     " #{sdk_lib} #{ure_lib}" +
     " #{sdk_libs} #{LIBRUBYARG} #{LIBS} "
  if host.include? 'mswin'
    sh "#{mt} -manifest #{t.name}.manifest -outputresource:#{t.name};2"
  end
end

desc "luno.so for loader"
file LOADER_MODULE_DLL => [LIB_DIR, *MODULE_OBJS, "#{LIB_DIR}/#{lib_dll}"] do |t|
  p "building luno.so"
  sh "#{link} #{out_flag}#{LIB_DIR}/#{MODULE_DLL} #{MODULE_OBJS.join(' ')} " +
     " #{LIB} #{local_link_lib} #{module_ldflags} " +
     " #{sdk_lib} #{ure_lib}" +
     " #{sdk_libs} #{LIBRUBYARG} #{LIBS} #{luno_libs}"
  if host.include? 'mswin'
    sh "#{mt} -manifest #{t.name}.manifest -outputresource:#{t.name};2"
  end
end


desc "create loader component"
file LOADER_LIB_DLL => [LIB_DIR, *LOADER_OBJS] do |t|
  p "building loader"
  sh "#{link} #{out_flag}#{LIB_DIR}/#{loader_dll} #{LOADER_OBJS.join(' ')} " +
     " #{LIB} #{local_link_lib} #{ldflags} #{uno_link_flags}" +
     " #{sdk_lib} #{ure_lib}" +
     " #{sdk_libs} #{LIBRUBYARG} #{LIBS} #{luno_libs} "
  if host.include? 'mswin'
    sh "#{mt} -manifest #{t.name}.manifest -outputresource:#{t.name};2"
  end
end


file LOADER_PKG => [LOADER_PKG_DIR, "#{LOADER_PKG_DIR}/lib", LOADER_LIB_LUNO_DLL, LOADER_MODULE_DLL, LOADER_LIB_DLL] do |t|
  
  dir_path = t.name.sub(/.oxt$/, "")
  media_type = "application/vnd.sun.star.uno-components;platform=#{platform}"
      "#{LIB_DIR}/#{loader_dll}"
  full_path = "lib/#{LOADER_REGISTRATION}"
  
  registration_task(dir_path, LOADER_REGISTRATION,
      "com.sun.star.loader.SharedLibrary", loader_dll,
      LOADER_IMPLE_NAME, LOADER_SERVICE_NAMES)
  manifest_task(dir_path, full_path, media_type)
  description_task(dir_path, LOADER_PKG_ID, target, LOADER_PKG_DISPLAY_NAME)
  extension_description_task(dir_path, LOADER_PKG_DESC)
  
  cp "#{LOADER_LIB_LUNO_DLL}", "#{LOADER_PKG_DIR_LIB}/#{lib_dll}"
  cp "#{LOADER_MODULE_DLL}", "#{LOADER_PKG_DIR_LIB}/#{MODULE_DLL}"
  cp "#{LOADER_LIB_DLL}", "#{LOADER_PKG_DIR_LIB}/#{loader_dll}"
  
  cp "#{SRC_DIR}/#{LOADER_LUA}", "#{LOADER_PKG_DIR_LIB}/#{LOADER_LUA}"
  cp "#{SRC_DIR}/#{UNO_RB}", "#{LOADER_PKG_DIR_LIB}/#{LIB_RB}"
  
  packaging_task dir_path, File.basename(t.name)
end


### Script provider package

SCRIPT_PKG_NAME = "LuaScriptProvider"
SCRIPT_PKG_DISPLAY_NAME = "Lua Script Provider"
SCRIPT_PACKAGE_ID = "mytools.script.provider.ScriptProviderForLua"

SCRIPT_REGISTRATION = "ScriptProviderForLua.components"
SCRIPT_DESC = "The script provider for Lua."
SCRIPT_IMPLE_NAME = "mytools.script.provider.ScriptProviderForLua"
SCRIPT_SERVICE_NAMES = [
  "com.sun.star.script.provider.ScriptProviderForLua",
  "com.sun.star.script.provider.LanguageScriptProvider"]
SCRIPT_RB = "luascript.lua"

SCRIPT_PKG_DIR_NAME = "#{SCRIPT_PKG_NAME}-#{VERSION}"
SCRIPT_PKG_FULL_NAME = "#{SCRIPT_PKG_NAME}-#{VERSION}.#{OXT_EXT}"
SCRIPT_PKG_DIR = "#{PKG_DIR}/#{SCRIPT_PKG_DIR_NAME}"
SCRIPT_PKG = "#{PKG_DIR}/#{SCRIPT_PKG_FULL_NAME}"

SCRIPT_PKG_DIR_LIB = "#{SCRIPT_PKG_DIR}/lib"

directory SCRIPT_PKG_DIR
directory SCRIPT_PKG_DIR_LIB

task :provider => [SCRIPT_PKG]


file SCRIPT_PKG => [SCRIPT_PKG_DIR, SCRIPT_PKG_DIR_LIB] do |t|
  dir_path = t.name.sub(/.oxt$/, "")
  media_type = "application/vnd.sun.star.uno-components"
  
  registration_task(dir_path, SCRIPT_REGISTRATION,
      "com.sun.star.loader.Lua", SCRIPT_RB,
      SCRIPT_IMPLE_NAME, SCRIPT_SERVICE_NAMES)
  manifest_task(dir_path, "lib/#{SCRIPT_REGISTRATION}", media_type)
  description_task(dir_path, SCRIPT_PACKAGE_ID, target, SCRIPT_PKG_DISPLAY_NAME)
  extension_description_task(dir_path, SCRIPT_DESC)
  
  cp "#{SRC_DIR}/#{SCRIPT_RB}", "#{SCRIPT_PKG_DIR_LIB}/#{SCRIPT_RB}"
  
  packaging_task dir_path, File.basename(t.name)
end


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
=begin
file "#{DOC_DIR}/index.html" => "#{LDOC_DIR}/index.md" do |t|
=end
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




