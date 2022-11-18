



@ECHO ON
setlocal enabledelayedexpansion

if "%DRONE_JOB_BUILDTYPE%" == "boost" (

echo '==================================> INSTALL'

git clone https://github.com/boostorg/boost-ci.git boost-ci-cloned --depth 1
cp -prf boost-ci-cloned/ci .
rm -rf boost-ci-cloned
REM source ci/travis/install.sh
REM The contents of install.sh below:

for /F %%i in ("%DRONE_REPO%") do @set SELF=%%~nxi
SET BOOST_CI_TARGET_BRANCH=%DRONE_COMMIT_BRANCH%
SET BOOST_CI_SRC_FOLDER=%cd%

call ci\common_install.bat

echo '==================================> COMPILE'

REM set B2_TARGETS=libs/!SELF!/test libs/!SELF!/example
set B2_TARGETS=libs/!SELF!/test
call !BOOST_ROOT!\libs\!SELF!\ci\build.bat

) else if "%DRONE_JOB_BUILDTYPE%" == "standalone-windows" (

REM not used

)