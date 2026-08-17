@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

SET includes=/Isrc /I%VULKAN_SDK%/include
SET links=/link /LIBPATH:%VULKAN_SDK%/lib vulkan-1.lib
SET defines=/D DEBUG

echo "Building main..."

cl /EHsc %includes% %defines% src/main.cpp %links%