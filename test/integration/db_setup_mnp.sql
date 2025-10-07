--
-- Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
--
-- Distributed under the Boost Software License, Version 1.0. (See accompanying
-- file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
--

USE boost_mysql_integtests;

-- Setup that requires the presence of the mysql_native_password functionality
DROP USER IF EXISTS 'mysqlnp_user'@'%';
CREATE USER 'mysqlnp_user'@'%' IDENTIFIED WITH 'mysql_native_password';
ALTER USER 'mysqlnp_user'@'%' IDENTIFIED BY 'mysqlnp_password';
GRANT ALL PRIVILEGES ON boost_mysql_integtests.* TO 'mysqlnp_user'@'%';

DROP USER IF EXISTS 'mysqlnp_empty_password_user'@'%';
CREATE USER 'mysqlnp_empty_password_user'@'%' IDENTIFIED WITH 'mysql_native_password';
ALTER USER 'mysqlnp_empty_password_user'@'%' IDENTIFIED BY '';
GRANT ALL PRIVILEGES ON boost_mysql_integtests.* TO 'mysqlnp_empty_password_user'@'%';

FLUSH PRIVILEGES;
