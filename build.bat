@echo off
REM ===========================================================================
REM  build.bat - build the CivicShield web server with MinGW-w64 (Windows)
REM ---------------------------------------------------------------------------
REM  Requirements: MinGW-w64 g++ on PATH (e.g. C:\mingw64\bin).
REM  Run from the project root:  build.bat
REM ===========================================================================
setlocal
set CXX=g++
set CXXFLAGS=-std=c++17 -O2 -Wall -Wextra
set INCLUDES=-Ithird_party -Isrc
set TARGET=DisasterManagementServer.exe

echo Building %TARGET% ...
%CXX% %CXXFLAGS% %INCLUDES% ^
  src/csv_utils.cpp src/models.cpp src/crypto.cpp src/priority_queue.cpp ^
  src/city_graph.cpp src/disaster_management.cpp src/authentication.cpp ^
  src/sessions.cpp src/store.cpp src/api_json.cpp src/api_routes.cpp ^
  src/input_utils.cpp src/menus.cpp src/main.cpp ^
  -o %TARGET% -lws2_32 -lwsock32 -lpthread

if errorlevel 1 (
  echo.
  echo BUILD FAILED - is MinGW-w64 g++ on your PATH?  Try: g++ --version
  exit /b 1
)
echo.
echo Build complete: %TARGET%
echo Start the website with:  %TARGET% --port 8080
endlocal
