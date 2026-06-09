@echo off
setlocal EnableExtensions
if not defined VULKAN_SDK (
  echo [compile_shaders.bat] VULKAN_SDK not set — skip SPIR-V compile. Install LunarG Vulkan SDK.
  exit /b 0
)
set "GLSLC=%VULKAN_SDK%\Bin\glslc.exe"
if not exist "%GLSLC%" (
  echo [compile_shaders.bat] glslc not found at %GLSLC%
  exit /b 1
)
set "ROOT=%~dp0.."
set "SHDIR=%ROOT%\resources\shaders\vk"
if not exist "%SHDIR%" mkdir "%SHDIR%"
for %%f in ("%SHDIR%\*.vert" "%SHDIR%\*.frag") do (
  if exist %%f (
    "%GLSLC%" %%f -o %%~dpnf.spv
  )
)
echo [compile_shaders.bat] done
exit /b 0
