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

-- Tables to test we retrieve correctly values of every possible type
-- Every type gets a separate table. Each field within the table is a possible variant of this same type
-- Every row is a test case, identified by the id column.

--    Integer types
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

CREATE TABLE types_smallint(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
	field_signed SMALLINT,
	field_unsigned SMALLINT UNSIGNED,
	field_width SMALLINT(8),
	field_zerofill SMALLINT(7) ZEROFILL
);
INSERT INTO types_smallint VALUES
	("regular",   20,     20,     20,      20),
	("negative", -20,     NULL,   -20,     NULL),
	("min",      -0x8000, 0,      NULL,    0),
	("max",       0x7fff, 0xffff, NULL,    NULL)
;

CREATE TABLE types_mediumint(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
	field_signed MEDIUMINT,
	field_unsigned MEDIUMINT UNSIGNED,
	field_width MEDIUMINT(8),
	field_zerofill MEDIUMINT(7) ZEROFILL
);
INSERT INTO types_mediumint VALUES
	("regular",   20,       20,       20,      20),
	("negative", -20,       NULL,     -20,     NULL),   
	("min",      -0x800000, 0,        NULL,    0),
	("max",       0x7fffff, 0xffffff, NULL,    NULL)
;

CREATE TABLE types_int(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
	field_signed INT,
	field_unsigned INT UNSIGNED,
	field_width INT(8),
	field_zerofill INT(7) ZEROFILL
);
INSERT INTO types_int VALUES
	("regular",   20,         20,         20,      20),
	("negative", -20,         NULL,       -20,     NULL),   
	("min",      -0x80000000, 0,          NULL,    0),
	("max",       0x7fffffff, 0xffffffff, NULL,    NULL)
;

CREATE TABLE types_bigint(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
	field_signed BIGINT,
	field_unsigned BIGINT UNSIGNED,
	field_width BIGINT(8),
	field_zerofill BIGINT(7) ZEROFILL
);
INSERT INTO types_bigint VALUES
	("regular",   20,                 20,                 20,      20),
	("negative", -20,                 NULL,               -20,     NULL),   
	("min",      -0x8000000000000000, 0,                  NULL,    0),
	("max",       0x7fffffffffffffff, 0xffffffffffffffff, NULL,    NULL)
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