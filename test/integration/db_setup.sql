-- Databse
DROP DATABASE IF EXISTS awesome;
CREATE DATABASE awesome;
USE awesome;

-- Tables
CREATE TABLE test_table (
    id INT AUTO_INCREMENT PRIMARY KEY,
    field_decimal DECIMAL (6,2),
    field_varchar VARCHAR(255) NOT NULL,
    field_bit BIT(8),
    field_float FLOAT(20),
    field_tiny TINYINT,
    field_text TEXT,
    field_date DATE,
    field_datetime DATETIME (3),
    field_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    field_time TIME (2)
) ENGINE=INNODB;

INSERT INTO test_table (field_varchar, field_date, field_datetime, field_time)
VALUES ('record_zero', '1999-08-01', '2010-11-30 21:10:11.987', '30:10:15');

CREATE TABLE child_table (
	id INT AUTO_INCREMENT PRIMARY KEY,
	parent_id INT NOT NULL,
	field_varchar VARCHAR(255),
	FOREIGN KEY (parent_id) REFERENCES test_table(id)
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