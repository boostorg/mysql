
DROP DATABASE IF EXISTS mytest;
CREATE DATABASE mytest;
USE mytest;

-- Required for the WITH RECURSIVE and the amount of rows we're generating
SET SESSION cte_max_recursion_depth = 15000;

-- An arbitrary value to pass to RAND(@seed). RAND(@i) generates a
-- deterministic sequence, vs RAND(@seed)
SET @seed = 3;

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
    WHERE num < 10000
) SELECT 
    FLOOR(RAND(@seed)*(0x7f+0x80+1)-0x80),
    FLOOR(RAND(@seed)*(0xff+1)),
    FLOOR(RAND(@seed)*(0x7fff+0x8000+1)-0x8000),
    FLOOR(RAND(@seed)*(0xffff+1)),
    FLOOR(RAND(@seed)*(0x7fffffff+0x80000000+1)-0x80000000),
    FLOOR(RAND(@seed)*(0xffffffff+1)),
    FLOOR(RAND(@seed)*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
    FLOOR(RAND(@seed)*(0xffffffffffffffff)),
    REPEAT(UUID(), 5),
    REPEAT(UUID(), FLOOR(RAND(@seed)*(1500-1000+1)+1000)),
    REPEAT(UUID(), 5),
    REPEAT(UUID(), FLOOR(RAND(@seed)*(1500-1000+1)+1000)),
    RAND(@seed),
    RAND(@seed),
    CURDATE(),
    CURTIME(),
    SEC_TO_TIME(RAND(@seed) + FLOOR(RAND(@seed)*(839*3600+839*3600+1)-839*3600))
FROM cte;
