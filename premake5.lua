dir_bin = "%{wks.location}/../../bin/%{cfg.buildcfg}"
dir_obj = "%{wks.location}/../../obj/%{prj.name}/%{cfg.buildcfg}"
dir_src = "%{wks.location}/../../src"
dir_lib = "%{wks.location}/../../vendor"
dir_pch = "src"

-- check for Visual Studio version with c++14 support
local sys = string.match(_ACTION, "%a+")
local ver = string.match(_ACTION, "%d+")

if (sys ~= "vs") then
	error("currently only Visual Studio build system is supported (vs*)")
end

if (tonumber(ver) < 2017) then
	error("currently only Visual Studio version 2017 or higher is supported (vs2017+)")
end


workspace "xmt"
   -- solution specific configuration
   location "build/%{_ACTION}"
   filename "xmt"
   configurations { "Debug", "Release" }
   system "Windows"
   startproject "Base"
   architecture "x86" -- x86 app only
   -- solution project common configuration
   targetdir "%{dir_bin}"
   objdir "%{dir_obj}"
   language "C++"
   rtti "Off"
   cppdialect "C++14"
   pchheader "pch.hpp"
	pchsource "%{dir_pch}/%{prj.name}/pch.cpp"   
   resincludedirs { "%{dir_src}/%{prj.name}" }
   includedirs 
   { 
      "%{dir_src}/Shared",
      "%{dir_src}/%{prj.name}",
      "%{dir_src}",
   }  
   files 
   { 
      "%{dir_src}/%{prj.name}/**.h",
      "%{dir_src}/%{prj.name}/**.c",
      "%{dir_src}/%{prj.name}/**.hpp",
      "%{dir_src}/%{prj.name}/**.cpp",
      "%{dir_src}/%{prj.name}/**.rc",
      "%{dir_src}/%{prj.name}/**.manifest",
   }
   vpaths {  [""] = { "**.*" } }   
   filter { "kind:WindowedApp or SharedLib" }
      linkoptions { "/DYNAMICBASE:NO" }   
   filter { "files:**.hpp or **.cpp" }
      forceincludes { "pch.hpp" }   
   filter "configurations:Debug"
      defines { "WIN32", "_DEBUG", "_WINDOWS" }
      symbols "On"
      optimize "Off"
      intrinsics "Off"
      inlining "Disabled"
      staticruntime "On"
      runtime "Debug"
      editandcontinue "On"
   filter "configurations:Release"
      defines { "WIN32", "NDEBUG", "_WINDOWS" }
      symbols "Off"
      optimize "Full"
      intrinsics "On"
      inlining "Auto"
      staticruntime "On"
      runtime "Release"
      editandcontinue "Off"
      flags { "NoIncrementalLink", "NoBufferSecurityCheck", "NoRuntimeChecks", "MultiProcessorCompile" }


project "Base"
   targetname "%{wks.name}"
   kind "WindowedApp"
   files { "%{dir_src}/Shared/**.h", "%{dir_src}/Shared/**.hpp", "%{dir_src}/Shared/**.c", "%{dir_src}/Shared/**.cpp", "%{dir_lib}/stb/include/**.h", "%{dir_lib}/nuklear/include/**.h" }
   includedirs 
   { 
      	"%{dir_lib}/openssl/include",
      	"%{dir_lib}/asio/include",
		"%{dir_lib}/stb/include",
		"%{dir_lib}/nuklear/include",
   }
   libdirs  { "%{dir_lib}/openssl/lib" }
   links { "ws2_32.lib", "CRYPT32.LIB", "Msimg32.lib", "gdiplus.lib", "shlwapi.lib" }
   filter { "configurations:Debug"}
      links { "libcrypto32MTd.lib", "libssl32MTd.lib" }
   filter { "configurations:Release"}
      links { "libcrypto32MT.lib", "libssl32MT.lib" }

project "Utils"
   kind "StaticLib"
   dependson { "Base" }
   files { "%{dir_lib}/nuklear/include/**.h" }
   includedirs { "%{dir_lib}/nuklear/include" }
   includedirs { "%{dir_lib}/zlib/include" }
   libdirs { "%{dir_lib}/zlib/lib" }   
   filter { "configurations:Debug"}
      links { "zlibstaticd.lib" }
   filter { "configurations:Release"}
      links { "zlibstatic.lib" }
   filter { "files:**.h or **.c" }
      flags "NoPCH"
      compileas "C"


function module_base_init()
   kind "SharedLib"
   dependson { "Base", "Utils" }
   files { "%{dir_lib}/nuklear/include/**.h" } -- include UI api
   files { "%{dir_src}/Shared/memstd.cpp", "%{dir_src}/Shared/memstd.hpp" } -- redirect dll new / delete to base memory module
   includedirs { "%{dir_lib}/nuklear/include" }
   libdirs { "%{dir_bin}" }   
   links { "Utils.lib", "%{wks.name}.lib" }   
   includedirs { "%{dir_src}/Utils/" } 
end


project "Test"
   module_base_init()


project "Tvl"
   module_base_init()


project "RunTestMode"
   targetname "%{wks.name}"
   kind "WindowedApp"
   debugargs { "--test" }