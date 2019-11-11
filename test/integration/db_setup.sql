-- Databse
DROP DATABASE IF EXISTS awesome;
CREATE DATABASE awesome;
USE awesome;

-- Tables
CREATE TABLE inserts_table (
    id INT AUTO_INCREMENT PRIMARY KEY,
    field_varchar VARCHAR(255) NOT NULL,
    field_date DATE
) ENGINE=INNODB;

CREATE TABLE updates_table (
    id INT AUTO_INCREMENT PRIMARY KEY,
    field_varchar VARCHAR(255) NOT NULL,
    field_int INT
) ENGINE=INNODB;
INSERT INTO updates_table (field_varchar, field_int)
VALUES ('f0', 42), ('f1', 43);

CREATE TABLE ints_table(
	signed_tiny TINYINT,
	unsigned_tiny TINYINT UNSIGNED,
	signed_small SMALLINT,
	unsigned_small SMALLINT UNSIGNED,
	signed_medium MEDIUMINT,
	unsigned_medium MEDIUMINT UNSIGNED,
	signed_int INT,
	unsigned_int INT UNSIGNED,
	signed_big BIGINT,
	unsigned_big BIGINT UNSIGNED,
	zerof INT(8) ZEROFILL
);

CREATE TABLE test_times(
	id INT AUTO_INCREMENT PRIMARY KEY,
	field_date DATE,
	field_time0 TIME,
	field_time1 TIME(1),
	field_time6 TIME(6)
);
INSERT INTO test_times (field_date,   field_time0,  field_time1,   field_time6)
VALUES 				   ('1999-08-01', '-838:59:59', '838:59:58.9', '838:59:58.999999');

-- Users
DROP USER IF EXISTS empty_password_user;
CREATE USER empty_password_user IDENTIFIED BY '';
GRANT ALL PRIVILEGES ON awesome.* TO 'empty_password_user';