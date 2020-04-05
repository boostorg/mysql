#!/bin/bash


cp ci/*.pem /tmp # Copy SSL certs/keys to a known location
if [ $TRAVIS_OS_NAME == "osx" ]; then
	brew update
	brew install $DATABASE
	#sudo cp ci/unix-ssl.cnf /etc/my.cnf  # In OSX, there is no default /etc/my.cnf
	cp ci/unix-ssl.cnf ~/.my.cnf
	mysqld --help --verbose
	mysql.server start # Note that running this with sudo fails
	cat /tmp/mysql-log.txt || echo "Could not open this log"
	cat /usr/local/var/mysql/Traviss-Mac.local.err || echo "Could not open this log"
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
