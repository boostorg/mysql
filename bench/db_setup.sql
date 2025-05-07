--
-- Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
--
-- Distributed under the Boost Software License, Version 1.0. (See accompanying
-- file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
--

-- Database
DROP DATABASE IF EXISTS boost_mysql_bench;
CREATE DATABASE boost_mysql_bench;
USE boost_mysql_bench;

-- Required for the WITH RECURSIVE and the amount of rows we're generating
SET SESSION cte_max_recursion_depth = 15000;

-- Having this prevents sporadic CI failures
SET global max_connections = 5000;

-- An arbitrary value to pass to RAND(@seed). RAND(@i) generates a
-- deterministic sequence, vs RAND(@seed)
SET @seed = 3;

-- Table, with a column per major type
CREATE TABLE test_data(
    id INT NOT NULL PRIMARY KEY AUTO_INCREMENT,
    s8 TINYINT NOT NULL,
    u8 TINYINT UNSIGNED NOT NULL,
    s16 SMALLINT NOT NULL,
    u16 SMALLINT UNSIGNED NOT NULL,
    s32 INT NOT NULL,
    u32 INT UNSIGNED NOT NULL,
    s64 BIGINT NOT NULL,
    u64 BIGINT UNSIGNED NOT NULL,
    s1 VARCHAR(256),
    s2 TEXT,
    b1 VARBINARY(256),
    b2 BLOB,
    flt FLOAT,
    dbl DOUBLE,
    dt DATE,
    dtime DATETIME,
    t TIME
);

-- Generate 10000 random rows
INSERT INTO test_data(
    s8,
    u8,
    s16,
    u16,
    s32,
    u32,
    s64,
    u64,
    s1,
    s2,
    b1,
    b2,
    flt,
    dbl,
    dt,
    dtime,
    t
)
WITH RECURSIVE cte AS (
    SELECT 0 num
    UNION ALL
    SELECT num + 1 FROM cte
    WHERE num < 5000
) SELECT
    FLOOR(RAND(@seed)*(0x7f+0x80+1)-0x80),
    FLOOR(RAND(@seed)*(0xff+1)),
    FLOOR(RAND(@seed)*(0x7fff+0x8000+1)-0x8000),
    FLOOR(RAND(@seed)*(0xffff+1)),
    FLOOR(RAND(@seed)*(0x7fffffff+0x80000000+1)-0x80000000),
    FLOOR(RAND(@seed)*(0xffffffff+1)),
    FLOOR(RAND(@seed)*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
    FLOOR(RAND(@seed)*(0xffffffffffffffff)),
    REPEAT('a', 180),
    REPEAT('b', FLOOR(RAND(@seed)*(54000-36000+1)+36000)),
    REPEAT('c', 180),
    REPEAT('d', FLOOR(RAND(@seed)*(54000-36000+1)+36000)),
    RAND(@seed),
    RAND(@seed),
    DATE_ADD('2020-01-01', INTERVAL FLOOR(RAND(@seed)*(5000+5000+1)-5000) DAY),
    DATE_ADD('2010-03-20', INTERVAL FLOOR(RAND(@seed)*(3600*24*365*20+3600*24*365*20+1)-3600*24*365*20) SECOND),
    SEC_TO_TIME(RAND(@seed) + FLOOR(RAND(@seed)*(839*3600+839*3600+1)-839*3600))
FROM cte;

-- A lightweight table, for the connection_pool benchmarks
CREATE TABLE lightweight_data(
    id INT NOT NULL PRIMARY KEY,
    data VARCHAR(100) NOT NULL
);

INSERT INTO lightweight_data VALUES
    (1, "Data piece 1"),
    (2, "Another data piece"),
    (3, "Final data piece")
;
