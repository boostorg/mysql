
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
    s8_0 TINYINT NOT NULL,
    u8_0 TINYINT UNSIGNED NOT NULL,
    s16_0 SMALLINT NOT NULL,
    u16_0 SMALLINT UNSIGNED NOT NULL,
    s32_0 INT NOT NULL,
    u32_0 INT UNSIGNED NOT NULL,
    s64_0 BIGINT NOT NULL,
    u64_0 BIGINT UNSIGNED NOT NULL,
    s1_0 VARCHAR(256),
    s2_0 TEXT,
    b1_0 VARBINARY(256),
    b2_0 BLOB,
    flt_0 FLOAT,
    dbl_0 DOUBLE,
    dt_0 DATE,
    dtime_0 DATETIME,
    t_0 TIME
);

INSERT INTO test_data(
    s8_0, u8_0, s16_0, u16_0, s32_0, u32_0, s64_0, u64_0, s1_0, s2_0, b1_0, b2_0, flt_0, dbl_0, dt_0, dtime_0, t_0
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
