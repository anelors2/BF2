@echo off
call "F:\Visual Studio\VC\Auxiliary\Build\vcvarsall.bat" x86
if not exist "d:\0ct\B2\external_imgui\build" mkdir "d:\0ct\B2\external_imgui\build"
cd /d "d:\0ct\B2\external_imgui\build"
cmake -G "Visual Studio 17 2022" -A Win32 ..
cmake --build . --config Release
