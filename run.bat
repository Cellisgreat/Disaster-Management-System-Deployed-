@echo off
REM Start the CivicShield web server and open the browser.
setlocal
if not exist DisasterManagementServer.exe (
  echo Executable not found - running build.bat first...
  call build.bat || exit /b 1
)
start "" http://localhost:8080/
DisasterManagementServer.exe --port 8080
endlocal
