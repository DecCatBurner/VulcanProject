@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

SET includes=/Isrc /I%VULKAN_SDK%/Include
SET links=/link /LIBPATH:%VULKAN_SDK%/lib vulkan-1.lib user32.lib
SET defines=/D DEBUG /D WIN32_BUILD /D VK_USE_PLATFORM_WIN32_KHR

echo "Building main..."

cl /EHsc /Z7 /Fe"main" %includes% %defines% src/platform/win32_platform.cpp %links%

where vulkan-1.dll
dumpbin /headers "%VULKAN_SDK%\lib\vulkan-1.lib" | findstr machine
dumpbin /headers main.exe | findstr machine