-- Connection system variables
SET NAMES utf8;

-- Database
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
VALUES ('f0', 42), ('f1', 43), ('fnull', NULL);

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

--   Floating point types
CREATE TABLE types_float(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
	field_signed FLOAT,
	field_unsigned FLOAT UNSIGNED,
	field_width FLOAT(30, 10),
	field_zerofill FLOAT(20) ZEROFILL
);
INSERT INTO types_float VALUES
	("zero",                             0,        0,    0,        0),
	("int_positive",                     4,        NULL, NULL,     NULL),
	("int_negative",                     -4,       NULL, NULL,     NULL),
	("fractional_positive",              4.2,      4.2,  4.2,      4.2),
	("fractional_negative",              -4.2,     NULL, -4.2,     NULL),
	("positive_exp_positive_int",        3e20,     NULL, NULL,     NULL),
	("positive_exp_negative_int",        -3e20,    NULL, NULL,     NULL),
	("positive_exp_positive_fractional", 3.14e20,  NULL, NULL,  3.14e20),
	("positive_exp_negative_fractional", -3.14e20, NULL, NULL,  NULL),
	("negative_exp_positive_fractional", 3.14e-20, NULL, NULL,  3.14e-20)
;

CREATE TABLE types_double(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
	field_signed DOUBLE,
	field_unsigned DOUBLE UNSIGNED,
	field_width DOUBLE(60, 10),
	field_zerofill FLOAT(40) ZEROFILL
);
INSERT INTO types_double VALUES
	("zero",                             0,        0,    0,        0),
	("int_positive",                     4,        NULL, NULL,     NULL),
	("int_negative",                     -4,       NULL, NULL,     NULL),
	("fractional_positive",              4.2,      4.2,  4.2,      4.2),
	("fractional_negative",              -4.2,     NULL, -4.2,     NULL),
	("positive_exp_positive_int",        3e200,     NULL, NULL,     NULL),
	("positive_exp_negative_int",        -3e200,    NULL, NULL,     NULL),
	("positive_exp_positive_fractional", 3.14e200,  NULL, NULL,  3.14e200),
	("positive_exp_negative_fractional", -3.14e200, NULL, NULL,  NULL),
	("negative_exp_positive_fractional", 3.14e-200, NULL, NULL,  3.14e-200)
;

--    Dates and times
CREATE TABLE types_date(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
	field_date DATE
);
INSERT INTO types_date VALUES
	("regular", "2010-03-28"),
	("leap",    "1788-02-29"),
	("min",     "1000-01-01"),
	("max",     "9999-12-31")
;

CREATE TABLE types_datetime(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
	field_0 DATETIME(0),
	field_1 DATETIME(1),
	field_2 DATETIME(2),
	field_3 DATETIME(3),
	field_4 DATETIME(4),
	field_5 DATETIME(5),
	field_6 DATETIME(6)
);
INSERT INTO types_datetime VALUES
	("date", "2010-05-02 00:00:00", "2010-05-02 00:00:00",   "2010-05-02 00:00:00",    "2010-05-02 00:00:00",     "2010-05-02 00:00:00",      "2010-05-02 00:00:00",       "2010-05-02 00:00:00"),
	("u",    NULL,                  "2010-05-02 00:00:00.1", "2010-05-02 00:00:00.12", "2010-05-02 00:00:00.123", "2010-05-02 00:00:00.1234", "2010-05-02 00:00:00.12345", "2010-05-02 00:00:00.123456"),
	("s",    "2010-05-02 00:00:50", "2010-05-02 00:00:50",   "2010-05-02 00:00:50",    "2010-05-02 00:00:50",     "2010-05-02 00:00:50",      "2010-05-02 00:00:50",       "2010-05-02 00:00:50"), 
	("m",    "2010-05-02 00:01:00", "2010-05-02 00:01:00",   "2010-05-02 00:01:00",    "2010-05-02 00:01:00",     "2010-05-02 00:01:00",      "2010-05-02 00:01:00",       "2010-05-02 00:01:00"),
	("hs",   "2010-05-02 23:00:50", "2010-05-02 23:00:50",   "2010-05-02 23:00:50",    "2010-05-02 23:00:50",     "2010-05-02 23:00:50",      "2010-05-02 23:00:50",       "2010-05-02 23:00:50"),
	("ms",   "2010-05-02 00:01:50", "2010-05-02 00:01:50",   "2010-05-02 00:01:50",    "2010-05-02 00:01:50",     "2010-05-02 00:01:50",      "2010-05-02 00:01:50",       "2010-05-02 00:01:50"),
	("hu",   NULL,                  "2010-05-02 23:00:00.1", "2010-05-02 23:00:00.12", "2010-05-02 23:00:00.123", "2010-05-02 23:00:00.1234", "2010-05-02 23:00:00.12345", "2010-05-02 23:00:00.123456"),
	("mu",   NULL,                  "2010-05-02 00:01:00.1", "2010-05-02 00:01:00.12", "2010-05-02 00:01:00.123", "2010-05-02 00:01:00.1234", "2010-05-02 00:01:00.12345", "2010-05-02 00:01:00.123456"),
	("hmu",  NULL,                  "2010-05-02 23:01:00.1", "2010-05-02 23:01:00.12", "2010-05-02 23:01:00.123", "2010-05-02 23:01:00.1234", "2010-05-02 23:01:00.12345", "2010-05-02 23:01:00.123456"),
	("su",   NULL,                  "2010-05-02 00:00:50.1", "2010-05-02 00:00:50.12", "2010-05-02 00:00:50.123", "2010-05-02 00:00:50.1234", "2010-05-02 00:00:50.12345", "2010-05-02 00:00:50.123456"),
	("hsu",  NULL,                  "2010-05-02 23:00:50.1", "2010-05-02 23:00:50.12", "2010-05-02 23:00:50.123", "2010-05-02 23:00:50.1234", "2010-05-02 23:00:50.12345", "2010-05-02 23:00:50.123456"),
	("msu",  NULL,                  "2010-05-02 00:01:50.1", "2010-05-02 00:01:50.12", "2010-05-02 00:01:50.123", "2010-05-02 00:01:50.1234", "2010-05-02 00:01:50.12345", "2010-05-02 00:01:50.123456"),
	("h",    "2010-05-02 23:00:00", "2010-05-02 23:00:00",   "2010-05-02 23:00:00",    "2010-05-02 23:00:00",     "2010-05-02 23:00:00",      "2010-05-02 23:00:00",       "2010-05-02 23:00:00"),
	("hm",   "2010-05-02 23:01:00", "2010-05-02 23:01:00",   "2010-05-02 23:01:00",    "2010-05-02 23:01:00",     "2010-05-02 23:01:00",      "2010-05-02 23:01:00",       "2010-05-02 23:01:00"),
	("hms",  "2010-05-02 23:01:50", "2010-05-02 23:01:50",   "2010-05-02 23:01:50",    "2010-05-02 23:01:50",     "2010-05-02 23:01:50",      "2010-05-02 23:01:50",       "2010-05-02 23:01:50"),
	("hmsu", NULL,                  "2010-05-02 23:01:50.1", "2010-05-02 23:01:50.12", "2010-05-02 23:01:50.123", "2010-05-02 23:01:50.1234", "2010-05-02 23:01:50.12345", "2010-05-02 23:01:50.123456"),
	("min",  "1000-01-01",          "1000-01-01",            "1000-01-01",             "1000-01-01",              "1000-01-01",               "1000-01-01",                "1000-01-01"),
	("max",  "9999-12-31 23:59:59", "9999-12-31 23:59:59.9", "9999-12-31 23:59:59.99", "9999-12-31 23:59:59.999", "9999-12-31 23:59:59.9999", "9999-12-31 23:59:59.99999", "9999-12-31 23:59:59.999999")
;

CREATE TABLE types_timestamp(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
	field_0 TIMESTAMP(0) NULL DEFAULT NULL,
	field_1 TIMESTAMP(1) NULL DEFAULT NULL,
	field_2 TIMESTAMP(2) NULL DEFAULT NULL,
	field_3 TIMESTAMP(3) NULL DEFAULT NULL,
	field_4 TIMESTAMP(4) NULL DEFAULT NULL,
	field_5 TIMESTAMP(5) NULL DEFAULT NULL,
	field_6 TIMESTAMP(6) NULL DEFAULT NULL
);
-- TODO: min and max not reproducible due to time zones. Find a programmatic way of determining the values here
INSERT INTO types_timestamp
SELECT * FROM types_datetime
WHERE id NOT IN ('min', 'max');

CREATE TABLE types_time(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
	field_0 TIME(0),
	field_1 TIME(1),
	field_2 TIME(2),
	field_3 TIME(3),
	field_4 TIME(4),
	field_5 TIME(5),
	field_6 TIME(6)
);
INSERT INTO types_time VALUES
	("zero",           "00:00:00",  "00:00:00",     "00:00:00",      "00:00:00",       "00:00:00",        "00:00:00",         "00:00:00"),
	("d",              "48:00:00",  "48:00:00",     "48:00:00",      "48:00:00",       "48:00:00",        "48:00:00",         "48:00:00"),
	("negative_d",     "-48:00:00", "-48:00:00",    "-48:00:00",     "-48:00:00",      "-48:00:00",       "-48:00:00",        "-48:00:00"),
	("h",              "23:00:00",  "23:00:00",     "23:00:00",      "23:00:00",       "23:00:00",        "23:00:00",         "23:00:00"),
	("negative_h",     "-23:00:00", "-23:00:00",    "-23:00:00",     "-23:00:00",      "-23:00:00",       "-23:00:00",        "-23:00:00"),
	("dh",             "71:00:00",  "71:00:00",     "71:00:00",      "71:00:00",       "71:00:00",        "71:00:00",         "71:00:00"),
	("negative_dh",    "-71:00:00", "-71:00:00",    "-71:00:00",     "-71:00:00",      "-71:00:00",       "-71:00:00",        "-71:00:00"),
	("m",              "00:01:00",  "00:01:00",     "00:01:00",      "00:01:00",       "00:01:00",        "00:01:00",         "00:01:00"),
	("negative_m",     "-00:01:00", "-00:01:00",    "-00:01:00",     "-00:01:00",      "-00:01:00",       "-00:01:00",        "-00:01:00"),
	("dm",             "48:01:00",  "48:01:00",     "48:01:00",      "48:01:00",       "48:01:00",        "48:01:00",         "48:01:00"),
	("negative_dm",    "-48:01:00", "-48:01:00",    "-48:01:00",     "-48:01:00",      "-48:01:00",       "-48:01:00",        "-48:01:00"),
	("hm",             "23:01:00",  "23:01:00",     "23:01:00",      "23:01:00",       "23:01:00",        "23:01:00",         "23:01:00"),
	("negative_hm",    "-23:01:00", "-23:01:00",    "-23:01:00",     "-23:01:00",      "-23:01:00",       "-23:01:00",        "-23:01:00"),
	("dhm",            "71:01:00",  "71:01:00",     "71:01:00",      "71:01:00",       "71:01:00",        "71:01:00",         "71:01:00"),
	("negative_dhm",   "-71:01:00", "-71:01:00",    "-71:01:00",     "-71:01:00",      "-71:01:00",       "-71:01:00",        "-71:01:00"),
	("s",              "00:00:50",  "00:00:50",     "00:00:50",      "00:00:50",       "00:00:50",        "00:00:50",         "00:00:50"),
	("negative_s",     "-00:00:50", "-00:00:50",    "-00:00:50",     "-00:00:50",      "-00:00:50",       "-00:00:50",        "-00:00:50"),
	("ds",             "48:00:50",  "48:00:50",     "48:00:50",      "48:00:50",       "48:00:50",        "48:00:50",         "48:00:50"),
	("negative_ds",    "-48:00:50", "-48:00:50",    "-48:00:50",     "-48:00:50",      "-48:00:50",       "-48:00:50",        "-48:00:50"),
	("hs",             "23:00:50",  "23:00:50",     "23:00:50",      "23:00:50",       "23:00:50",        "23:00:50",         "23:00:50"),
	("negative_hs",    "-23:00:50", "-23:00:50",    "-23:00:50",     "-23:00:50",      "-23:00:50",       "-23:00:50",        "-23:00:50"),
	("dhs",            "71:00:50",  "71:00:50",     "71:00:50",      "71:00:50",       "71:00:50",        "71:00:50",         "71:00:50"),
	("negative_dhs",   "-71:00:50", "-71:00:50",    "-71:00:50",     "-71:00:50",      "-71:00:50",       "-71:00:50",        "-71:00:50"),
	("ms",             "00:01:50",  "00:01:50",     "00:01:50",      "00:01:50",       "00:01:50",        "00:01:50",         "00:01:50"),
	("negative_ms",    "-00:01:50", "-00:01:50",    "-00:01:50",     "-00:01:50",      "-00:01:50",       "-00:01:50",        "-00:01:50"),
	("dms",            "48:01:50",  "48:01:50",     "48:01:50",      "48:01:50",       "48:01:50",        "48:01:50",         "48:01:50"),
	("negative_dms",   "-48:01:50", "-48:01:50",    "-48:01:50",     "-48:01:50",      "-48:01:50",       "-48:01:50",        "-48:01:50"),
	("hms",            "23:01:50",  "23:01:50",     "23:01:50",      "23:01:50",       "23:01:50",        "23:01:50",         "23:01:50"),
	("negative_hms",   "-23:01:50", "-23:01:50",    "-23:01:50",     "-23:01:50",      "-23:01:50",       "-23:01:50",        "-23:01:50"),
	("dhms",           "71:01:50",  "71:01:50",     "71:01:50",      "71:01:50",       "71:01:50",        "71:01:50",         "71:01:50"),
	("negative_dhms",  "-71:01:50", "-71:01:50",    "-71:01:50",     "-71:01:50",      "-71:01:50",       "-71:01:50",        "-71:01:50"),
	("u",              NULL,        "00:00:00.1",   "00:00:00.12",   "00:00:00.123",   "00:00:00.1234",   "00:00:00.12345",   "00:00:00.123456"),
	("negative_u",     NULL,        "-00:00:00.1",  "-00:00:00.12",  "-00:00:00.123",  "-00:00:00.1234",  "-00:00:00.12345",  "-00:00:00.123456"),
	("du",             NULL,        "48:00:00.1",   "48:00:00.12",   "48:00:00.123",   "48:00:00.1234",   "48:00:00.12345",   "48:00:00.123456"),
	("negative_du",    NULL,        "-48:00:00.1",  "-48:00:00.12",  "-48:00:00.123",  "-48:00:00.1234",  "-48:00:00.12345",  "-48:00:00.123456"),
	("hu",             NULL,        "23:00:00.1",   "23:00:00.12",   "23:00:00.123",   "23:00:00.1234",   "23:00:00.12345",   "23:00:00.123456"),
	("negative_hu",    NULL,        "-23:00:00.1",  "-23:00:00.12",  "-23:00:00.123",  "-23:00:00.1234",  "-23:00:00.12345",  "-23:00:00.123456"),
	("dhu",            NULL,        "71:00:00.1",   "71:00:00.12",   "71:00:00.123",   "71:00:00.1234",   "71:00:00.12345",   "71:00:00.123456"),
	("negative_dhu",   NULL,        "-71:00:00.1",  "-71:00:00.12",  "-71:00:00.123",  "-71:00:00.1234",  "-71:00:00.12345",  "-71:00:00.123456"),
	("mu",             NULL,        "00:01:00.1",   "00:01:00.12",   "00:01:00.123",   "00:01:00.1234",   "00:01:00.12345",   "00:01:00.123456"),
	("negative_mu",    NULL,        "-00:01:00.1",  "-00:01:00.12",  "-00:01:00.123",  "-00:01:00.1234",  "-00:01:00.12345",  "-00:01:00.123456"),
	("dmu",            NULL,        "48:01:00.1",   "48:01:00.12",   "48:01:00.123",   "48:01:00.1234",   "48:01:00.12345",   "48:01:00.123456"),
	("negative_dmu",   NULL,        "-48:01:00.1",  "-48:01:00.12",  "-48:01:00.123",  "-48:01:00.1234",  "-48:01:00.12345",  "-48:01:00.123456"),
	("hmu",            NULL,        "23:01:00.1",   "23:01:00.12",   "23:01:00.123",   "23:01:00.1234",   "23:01:00.12345",   "23:01:00.123456"),
	("negative_hmu",   NULL,        "-23:01:00.1",  "-23:01:00.12",  "-23:01:00.123",  "-23:01:00.1234",  "-23:01:00.12345",  "-23:01:00.123456"),
	("dhmu",           NULL,        "71:01:00.1",   "71:01:00.12",   "71:01:00.123",   "71:01:00.1234",   "71:01:00.12345",   "71:01:00.123456"),
	("negative_dhmu",  NULL,        "-71:01:00.1",  "-71:01:00.12",  "-71:01:00.123",  "-71:01:00.1234",  "-71:01:00.12345",  "-71:01:00.123456"),
	("su",             NULL,        "00:00:50.1",   "00:00:50.12",   "00:00:50.123",   "00:00:50.1234",   "00:00:50.12345",   "00:00:50.123456"),
	("negative_su",    NULL,        "-00:00:50.1",  "-00:00:50.12",  "-00:00:50.123",  "-00:00:50.1234",  "-00:00:50.12345",  "-00:00:50.123456"),
	("dsu",            NULL,        "48:00:50.1",   "48:00:50.12",   "48:00:50.123",   "48:00:50.1234",   "48:00:50.12345",   "48:00:50.123456"),
	("negative_dsu",   NULL,        "-48:00:50.1",  "-48:00:50.12",  "-48:00:50.123",  "-48:00:50.1234",  "-48:00:50.12345",  "-48:00:50.123456"),
	("hsu",            NULL,        "23:00:50.1",   "23:00:50.12",   "23:00:50.123",   "23:00:50.1234",   "23:00:50.12345",   "23:00:50.123456"),
	("negative_hsu",   NULL,        "-23:00:50.1",  "-23:00:50.12",  "-23:00:50.123",  "-23:00:50.1234",  "-23:00:50.12345",  "-23:00:50.123456"),
	("dhsu",           NULL,        "71:00:50.1",   "71:00:50.12",   "71:00:50.123",   "71:00:50.1234",   "71:00:50.12345",   "71:00:50.123456"),
	("negative_dhsu",  NULL,        "-71:00:50.1",  "-71:00:50.12",  "-71:00:50.123",  "-71:00:50.1234",  "-71:00:50.12345",  "-71:00:50.123456"),
	("msu",            NULL,        "00:01:50.1",   "00:01:50.12",   "00:01:50.123",   "00:01:50.1234",   "00:01:50.12345",   "00:01:50.123456"),
	("negative_msu",   NULL,        "-00:01:50.1",  "-00:01:50.12",  "-00:01:50.123",  "-00:01:50.1234",  "-00:01:50.12345",  "-00:01:50.123456"),
	("dmsu",           NULL,        "48:01:50.1",   "48:01:50.12",   "48:01:50.123",   "48:01:50.1234",   "48:01:50.12345",   "48:01:50.123456"),
	("negative_dmsu",  NULL,        "-48:01:50.1",  "-48:01:50.12",  "-48:01:50.123",  "-48:01:50.1234",  "-48:01:50.12345",  "-48:01:50.123456"),
	("hmsu",           NULL,        "23:01:50.1",   "23:01:50.12",   "23:01:50.123",   "23:01:50.1234",   "23:01:50.12345",   "23:01:50.123456"),
	("negative_hmsu",  NULL,        "-23:01:50.1",  "-23:01:50.12",  "-23:01:50.123",  "-23:01:50.1234",  "-23:01:50.12345",  "-23:01:50.123456"),
	("dhmsu",          NULL,        "71:01:50.1",   "71:01:50.12",   "71:01:50.123",   "71:01:50.1234",   "71:01:50.12345",   "71:01:50.123456"),
	("negative_dhmsu", NULL,        "-71:01:50.1",  "-71:01:50.12",  "-71:01:50.123",  "-71:01:50.1234",  "-71:01:50.12345",  "-71:01:50.123456"),
	("min",           "-838:59:59", "-838:59:58.9", "-838:59:58.99", "-838:59:58.999", "-838:59:58.9999", "-838:59:58.99999", "-838:59:58.999999"),
	("max",           "838:59:59",  "838:59:58.9",  "838:59:58.99",  "838:59:58.999",  "838:59:58.9999",  "838:59:58.99999",  "838:59:58.999999")
;

CREATE TABLE types_year(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
	field_default YEAR
);
INSERT INTO types_year VALUES
	("regular", 2019),
	("min",     1901),
	("max",     2155),
	("zero",    0)
;

CREATE TABLE types_string(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
	field_char CHAR(20),
	field_varchar VARCHAR(30),
	field_tinytext TINYTEXT,
	field_text TEXT,
	field_mediumtext MEDIUMTEXT,
	field_longtext LONGTEXT,
	field_enum ENUM("red", "green", "blue"),
	field_set SET("red", "green", "blue")
);
INSERT INTO types_string VALUES
	("regular", "test_char", "test_varchar", "test_tinytext", "test_text", "test_mediumtext", "test_longtext", "red", "red,green"),
	("utf8",    "ñ",         "Ñ",            "á",             "é",         "í",               "ó",             NULL,  NULL),
	("empty",   "",          "",             "",              "",          "",                "",              NULL,  "")
;

CREATE TABLE types_binary(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
	field_binary BINARY(10),
	field_varbinary VARBINARY(30),
	field_tinyblob TINYBLOB,
	field_blob BLOB,
	field_mediumblob MEDIUMBLOB,
	field_longblob LONGBLOB
);
INSERT INTO types_binary VALUES
	("regular",  "\0_binary", "\0_varbinary", "\0_tinyblob", "\0_blob", "\0_mediumblob", "\0_longblob"),
	("nonascii", X'00FF',     X'01FE',        X'02FD',       X'03FC',   X'04FB',         X'05FA'),
	("empty",   "",          "",             "",            "",        "",              "")
;

CREATE TABLE types_not_implemented(
	id VARCHAR(50) NOT NULL PRIMARY KEY,
	field_bit BIT(8),
	field_decimal DECIMAL,
	field_geometry GEOMETRY
);
INSERT INTO types_not_implemented VALUES
	("regular", 0xfe, 300, POINT(1, 2))
;

CREATE TABLE types_flags(
	id VARCHAR(50) NOT NULL,
	field_timestamp TIMESTAMP NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
	field_primary_key INT NOT NULL AUTO_INCREMENT,
	field_not_null CHAR(8) NOT NULL DEFAULT "",
	field_unique INT,
	field_indexed INT,
	PRIMARY KEY (field_primary_key, id),
	UNIQUE KEY (field_unique),
	KEY (field_indexed)
);
INSERT INTO types_flags VALUES
	("default", NULL, 50, "char", 21, 42)
;

-- Users
DROP USER IF EXISTS integ_user;
CREATE USER integ_user IDENTIFIED WITH 'mysql_native_password' BY 'integ_password';
GRANT ALL PRIVILEGES ON *.* TO 'integ_user';

DROP USER IF EXISTS empty_password_user;
CREATE USER empty_password_user IDENTIFIED WITH 'mysql_native_password' BY '';
GRANT ALL PRIVILEGES ON awesome.* TO 'empty_password_user';

FLUSH PRIVILEGES;