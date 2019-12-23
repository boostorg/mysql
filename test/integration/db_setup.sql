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

CREATE TABLE empty_table (
	id INT,
	field_varchar VARCHAR(255)
);

CREATE TABLE one_row_table (
	id INT,
	field_varchar VARCHAR(255)
);
INSERT INTO one_row_table VALUES (1, 'f0');

CREATE TABLE two_rows_table (
	id INT,
	field_varchar VARCHAR(255)
);
INSERT INTO two_rows_table VALUES (1, 'f0'), (2, 'f1');

CREATE TABLE three_rows_table (
	id INT,
	field_varchar VARCHAR(255)
);
INSERT INTO three_rows_table VALUES (1, 'f0'), (2, 'f1'), (3, 'f2');

CREATE TABLE types_tinyint(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
	field_signed TINYINT,
	field_unsigned TINYINT UNSIGNED,
	field_width TINYINT(4),
	field_zerofill TINYINT(6) ZEROFILL
);
INSERT INTO types_tinyint VALUES
	("regular",   20,   20,   20,      20),
	("negative", -20,   NULL, -20,     NULL),
	("min",      -0x80, 0,    NULL,       0),
	("max",       0x7f, 0xff, NULL,    NULL)
;

CREATE TABLE ints_table(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
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
INSERT INTO ints_table VALUES
	("regular",   20,   20,   20,      20,      20,       20,        20,         20,          20,                 20,                 20),
	("negative", -20,   NULL, -20,     NULL,   -20,       NULL,     -20,         NULL,       -20,                 NULL,               NULL),
	("min",      -0x80, 0,    -0x8000, 0,      -0x800000, 0,        -0x80000000, 0,          -0x8000000000000000, 0,                  0),
	("max",       0x7f, 0xff,  0x7fff, 0xffff,  0x7fffff, 0xffffff,  0x7fffffff, 0xffffffff,  0x7fffffffffffffff, 0xffffffffffffffff, NULL)
;
	

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