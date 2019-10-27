-- Databse
DROP DATABASE IF EXISTS awesome;
CREATE DATABASE awesome;

-- Users
DROP USER IF EXISTS empty_password_user;
CREATE USER empty_password_user IDENTIFIED BY '';
GRANT ALL PRIVILEGES ON awesome.* TO 'empty_password_user';