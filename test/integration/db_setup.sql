-- Databse
DROP DATABASE IF EXISTS awesome;
CREATE DATABASE awesome;
USE awesome;

-- Tables
CREATE TABLE test_table (
    id INT AUTO_INCREMENT PRIMARY KEY,
    field_varchar VARCHAR(255) NOT NULL,
    field_float FLOAT(20),
    field_date DATE,
    field_tiny TINYINT NOT NULL,
    field_text TEXT,
    field_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=INNODB;

CREATE TABLE child_table (
	id INT AUTO_INCREMENT PRIMARY KEY,
	parent_id INT NOT NULL,
	field_varchar VARCHAR(255),
	FOREIGN KEY (parent_id) REFERENCES test_table(id)
);

-- Users
DROP USER IF EXISTS empty_password_user;
CREATE USER empty_password_user IDENTIFIED BY '';
GRANT ALL PRIVILEGES ON awesome.* TO 'empty_password_user';