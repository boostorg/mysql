#!/bin/bash


cp ci/*.pem /tmp # Copy SSL certs/keys to a known location
if [ $TRAVIS_OS_NAME == "osx" ]; then
	brew update
	brew install $DATABASE
	cp ci/unix-ssl.cnf ~/.my.cnf  # This location is checked by both MySQL and MariaDB
	mysql.server start # Note that running this with sudo fails
	if [ $DATABASE == "mariadb" ]; then
		sudo mysql -u root < ci/root_user_setup.sql
	fi
else
	sudo cp ci/unix-ssl.cnf /etc/mysql/conf.d/ssl.cnf
	sudo service mysql restart
fi


mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE $CMAKE_OPTIONS .. 
make -j6 CTEST_OUTPUT_ON_FAILURE=1 all test
