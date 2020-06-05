
call ci\appveyor\install.bat || exit /b 1
call ci\build.bat || exit /b 1