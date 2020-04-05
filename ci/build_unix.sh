#!/bin/bash


cp ci/*.pem /tmp # Copy SSL certs/keys to a known location
if [ $TRAVIS_OS_NAME == "osx" ]; then
	brew update
	brew install $DATABASE
	
	if [ $DATABASE == "mariadb" ]; then
		DATABASE_PREFIX=/usr/local/Cellar/mariadb/10.4.11
	else
		DATABASE_PREFIX=/usr/local/Cellar/mysql/8.0.19
	fi
	

	find $DATABASE_PREFIX
	find $DATABASE_PREFIX -name "*.cnf" | xargs -I {} bash -c "echo '----Printing file {}' && cat {}"
	
	#sudo bash -c "printf \"[mysqld]\n!include $(pwd)/ci/unix-ssl.cnf\n\" >> /etc/my.cnf"
	#sudo cat /etc/my.cnf
	sudo mysql.server start
	
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
