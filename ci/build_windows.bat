
rem Chocolatey stuff
choco feature enable -n=allowGlobalConfirmation || goto error
choco install ^
	openssl ^
	%DATABASE% ^
	visualstudio2019buildtools ^
	visualstudio2019-workload-vctools || goto error
set PATH=%PATH%;C:\tools\mysql\current\bin

rem Environment setup
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" %TARGET_ARCH%
set BOOST_ROOT=%TMP%\boost
set CTEST_OUTPUT_ON_FAILURE=1

rem Download and build boost
wget -q -O boost_download.zip ^
	https://dl.bintray.com/boostorg/release/1.72.0/source/boost_1_72_0.zip ^
	|| goto error
bash -c "unzip -q boost_download.zip" || goto error
cd boost_1_72_0
cmd /c ".\bootstrap.bat --prefix=%BOOST_ROOT%" || goto error
cmd /c ".\b2 install -d0 --prefix=%BOOST_ROOT% --with-system" || goto error
cd ..

rem Build our project
mkdir build
cd build
cmake -G Ninja ^
      -DCMAKE_C_COMPILER=cl ^
      -DCMAKE_CXX_COMPILER=cl ^
      -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% ^
      -DBOOST_ROOT=%BOOST_ROOT% ^
      %CMAKE_OPTIONS% ^
      .. || goto error
bash -x -c "sed -i 's=/MD=/MT=g' CMakeCache.txt" || goto error  rem Handles both debug and release cases
cmake .. || goto error  rem Reinvocation of CMake apparently required to pick new cache flags
cmake --build . -j || goto error
ctest || goto error

exit /B 0

:error
exit /B 1