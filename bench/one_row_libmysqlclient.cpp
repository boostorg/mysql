#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mysql/field_types.h>
#include <mysql/mysql.h>
#include <mysql/mysql_com.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

using namespace std;

constexpr const char* create_table = R"SQL(
CREATE TEMPORARY TABLE myt(
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
    t_0 TIME,
    s8_1 TINYINT NOT NULL,
    u8_1 TINYINT UNSIGNED NOT NULL,
    s16_1 SMALLINT NOT NULL,
    u16_1 SMALLINT UNSIGNED NOT NULL,
    s32_1 INT NOT NULL,
    u32_1 INT UNSIGNED NOT NULL,
    s64_1 BIGINT NOT NULL,
    u64_1 BIGINT UNSIGNED NOT NULL,
    s1_1 VARCHAR(256),
    s2_1 TEXT,
    b1_1 VARBINARY(256),
    b2_1 BLOB,
    flt_1 FLOAT,
    dbl_1 DOUBLE,
    dt_1 DATE,
    dtime_1 DATETIME,
    t_1 TIME,
    s8_2 TINYINT NOT NULL,
    u8_2 TINYINT UNSIGNED NOT NULL,
    s16_2 SMALLINT NOT NULL,
    u16_2 SMALLINT UNSIGNED NOT NULL,
    s32_2 INT NOT NULL,
    u32_2 INT UNSIGNED NOT NULL,
    s64_2 BIGINT NOT NULL,
    u64_2 BIGINT UNSIGNED NOT NULL,
    s1_2 VARCHAR(256),
    s2_2 TEXT,
    b1_2 VARBINARY(256),
    b2_2 BLOB,
    flt_2 FLOAT,
    dbl_2 DOUBLE,
    dt_2 DATE,
    dtime_2 DATETIME,
    t_2 TIME,
    s8_3 TINYINT NOT NULL,
    u8_3 TINYINT UNSIGNED NOT NULL,
    s16_3 SMALLINT NOT NULL,
    u16_3 SMALLINT UNSIGNED NOT NULL,
    s32_3 INT NOT NULL,
    u32_3 INT UNSIGNED NOT NULL,
    s64_3 BIGINT NOT NULL,
    u64_3 BIGINT UNSIGNED NOT NULL,
    s1_3 VARCHAR(256),
    s2_3 TEXT,
    b1_3 VARBINARY(256),
    b2_3 BLOB,
    flt_3 FLOAT,
    dbl_3 DOUBLE,
    dt_3 DATE,
    dtime_3 DATETIME,
    t_3 TIME,
    s8_4 TINYINT NOT NULL,
    u8_4 TINYINT UNSIGNED NOT NULL,
    s16_4 SMALLINT NOT NULL,
    u16_4 SMALLINT UNSIGNED NOT NULL,
    s32_4 INT NOT NULL,
    u32_4 INT UNSIGNED NOT NULL,
    s64_4 BIGINT NOT NULL,
    u64_4 BIGINT UNSIGNED NOT NULL,
    s1_4 VARCHAR(256),
    s2_4 TEXT,
    b1_4 VARBINARY(256),
    b2_4 BLOB,
    flt_4 FLOAT,
    dbl_4 DOUBLE,
    dt_4 DATE,
    dtime_4 DATETIME,
    t_4 TIME,
    s8_5 TINYINT NOT NULL,
    u8_5 TINYINT UNSIGNED NOT NULL,
    s16_5 SMALLINT NOT NULL,
    u16_5 SMALLINT UNSIGNED NOT NULL,
    s32_5 INT NOT NULL,
    u32_5 INT UNSIGNED NOT NULL,
    s64_5 BIGINT NOT NULL,
    u64_5 BIGINT UNSIGNED NOT NULL,
    s1_5 VARCHAR(256),
    s2_5 TEXT,
    b1_5 VARBINARY(256),
    b2_5 BLOB,
    flt_5 FLOAT,
    dbl_5 DOUBLE,
    dt_5 DATE,
    dtime_5 DATETIME,
    t_5 TIME,
    s8_6 TINYINT NOT NULL,
    u8_6 TINYINT UNSIGNED NOT NULL,
    s16_6 SMALLINT NOT NULL,
    u16_6 SMALLINT UNSIGNED NOT NULL,
    s32_6 INT NOT NULL,
    u32_6 INT UNSIGNED NOT NULL,
    s64_6 BIGINT NOT NULL,
    u64_6 BIGINT UNSIGNED NOT NULL,
    s1_6 VARCHAR(256),
    s2_6 TEXT,
    b1_6 VARBINARY(256),
    b2_6 BLOB,
    flt_6 FLOAT,
    dbl_6 DOUBLE,
    dt_6 DATE,
    dtime_6 DATETIME,
    t_6 TIME,
    s8_7 TINYINT NOT NULL,
    u8_7 TINYINT UNSIGNED NOT NULL,
    s16_7 SMALLINT NOT NULL,
    u16_7 SMALLINT UNSIGNED NOT NULL,
    s32_7 INT NOT NULL,
    u32_7 INT UNSIGNED NOT NULL,
    s64_7 BIGINT NOT NULL,
    u64_7 BIGINT UNSIGNED NOT NULL,
    s1_7 VARCHAR(256),
    s2_7 TEXT,
    b1_7 VARBINARY(256),
    b2_7 BLOB,
    flt_7 FLOAT,
    dbl_7 DOUBLE,
    dt_7 DATE,
    dtime_7 DATETIME,
    t_7 TIME,
    s8_8 TINYINT NOT NULL,
    u8_8 TINYINT UNSIGNED NOT NULL,
    s16_8 SMALLINT NOT NULL,
    u16_8 SMALLINT UNSIGNED NOT NULL,
    s32_8 INT NOT NULL,
    u32_8 INT UNSIGNED NOT NULL,
    s64_8 BIGINT NOT NULL,
    u64_8 BIGINT UNSIGNED NOT NULL,
    s1_8 VARCHAR(256),
    s2_8 TEXT,
    b1_8 VARBINARY(256),
    b2_8 BLOB,
    flt_8 FLOAT,
    dbl_8 DOUBLE,
    dt_8 DATE,
    dtime_8 DATETIME,
    t_8 TIME,
    s8_9 TINYINT NOT NULL,
    u8_9 TINYINT UNSIGNED NOT NULL,
    s16_9 SMALLINT NOT NULL,
    u16_9 SMALLINT UNSIGNED NOT NULL,
    s32_9 INT NOT NULL,
    u32_9 INT UNSIGNED NOT NULL,
    s64_9 BIGINT NOT NULL,
    u64_9 BIGINT UNSIGNED NOT NULL,
    s1_9 VARCHAR(256),
    s2_9 TEXT,
    b1_9 VARBINARY(256),
    b2_9 BLOB,
    flt_9 FLOAT,
    dbl_9 DOUBLE,
    dt_9 DATE,
    dtime_9 DATETIME,
    t_9 TIME,
    s8_10 TINYINT NOT NULL,
    u8_10 TINYINT UNSIGNED NOT NULL,
    s16_10 SMALLINT NOT NULL,
    u16_10 SMALLINT UNSIGNED NOT NULL,
    s32_10 INT NOT NULL,
    u32_10 INT UNSIGNED NOT NULL,
    s64_10 BIGINT NOT NULL,
    u64_10 BIGINT UNSIGNED NOT NULL,
    s1_10 VARCHAR(256),
    s2_10 TEXT,
    b1_10 VARBINARY(256),
    b2_10 BLOB,
    flt_10 FLOAT,
    dbl_10 DOUBLE,
    dt_10 DATE,
    dtime_10 DATETIME,
    t_10 TIME,
    s8_11 TINYINT NOT NULL,
    u8_11 TINYINT UNSIGNED NOT NULL,
    s16_11 SMALLINT NOT NULL,
    u16_11 SMALLINT UNSIGNED NOT NULL,
    s32_11 INT NOT NULL,
    u32_11 INT UNSIGNED NOT NULL,
    s64_11 BIGINT NOT NULL,
    u64_11 BIGINT UNSIGNED NOT NULL,
    s1_11 VARCHAR(256),
    s2_11 TEXT,
    b1_11 VARBINARY(256),
    b2_11 BLOB,
    flt_11 FLOAT,
    dbl_11 DOUBLE,
    dt_11 DATE,
    dtime_11 DATETIME,
    t_11 TIME,
    s8_12 TINYINT NOT NULL,
    u8_12 TINYINT UNSIGNED NOT NULL,
    s16_12 SMALLINT NOT NULL,
    u16_12 SMALLINT UNSIGNED NOT NULL,
    s32_12 INT NOT NULL,
    u32_12 INT UNSIGNED NOT NULL,
    s64_12 BIGINT NOT NULL,
    u64_12 BIGINT UNSIGNED NOT NULL,
    s1_12 VARCHAR(256),
    s2_12 TEXT,
    b1_12 VARBINARY(256),
    b2_12 BLOB,
    flt_12 FLOAT,
    dbl_12 DOUBLE,
    dt_12 DATE,
    dtime_12 DATETIME,
    t_12 TIME,
    s8_13 TINYINT NOT NULL,
    u8_13 TINYINT UNSIGNED NOT NULL,
    s16_13 SMALLINT NOT NULL,
    u16_13 SMALLINT UNSIGNED NOT NULL,
    s32_13 INT NOT NULL,
    u32_13 INT UNSIGNED NOT NULL,
    s64_13 BIGINT NOT NULL,
    u64_13 BIGINT UNSIGNED NOT NULL,
    s1_13 VARCHAR(256),
    s2_13 TEXT,
    b1_13 VARBINARY(256),
    b2_13 BLOB,
    flt_13 FLOAT,
    dbl_13 DOUBLE,
    dt_13 DATE,
    dtime_13 DATETIME,
    t_13 TIME,
    s8_14 TINYINT NOT NULL,
    u8_14 TINYINT UNSIGNED NOT NULL,
    s16_14 SMALLINT NOT NULL,
    u16_14 SMALLINT UNSIGNED NOT NULL,
    s32_14 INT NOT NULL,
    u32_14 INT UNSIGNED NOT NULL,
    s64_14 BIGINT NOT NULL,
    u64_14 BIGINT UNSIGNED NOT NULL,
    s1_14 VARCHAR(256),
    s2_14 TEXT,
    b1_14 VARBINARY(256),
    b2_14 BLOB,
    flt_14 FLOAT,
    dbl_14 DOUBLE,
    dt_14 DATE,
    dtime_14 DATETIME,
    t_14 TIME,
    s8_15 TINYINT NOT NULL,
    u8_15 TINYINT UNSIGNED NOT NULL,
    s16_15 SMALLINT NOT NULL,
    u16_15 SMALLINT UNSIGNED NOT NULL,
    s32_15 INT NOT NULL,
    u32_15 INT UNSIGNED NOT NULL,
    s64_15 BIGINT NOT NULL,
    u64_15 BIGINT UNSIGNED NOT NULL,
    s1_15 VARCHAR(256),
    s2_15 TEXT,
    b1_15 VARBINARY(256),
    b2_15 BLOB,
    flt_15 FLOAT,
    dbl_15 DOUBLE,
    dt_15 DATE,
    dtime_15 DATETIME,
    t_15 TIME,
    s8_16 TINYINT NOT NULL,
    u8_16 TINYINT UNSIGNED NOT NULL,
    s16_16 SMALLINT NOT NULL,
    u16_16 SMALLINT UNSIGNED NOT NULL,
    s32_16 INT NOT NULL,
    u32_16 INT UNSIGNED NOT NULL,
    s64_16 BIGINT NOT NULL,
    u64_16 BIGINT UNSIGNED NOT NULL,
    s1_16 VARCHAR(256),
    s2_16 TEXT,
    b1_16 VARBINARY(256),
    b2_16 BLOB,
    flt_16 FLOAT,
    dbl_16 DOUBLE,
    dt_16 DATE,
    dtime_16 DATETIME,
    t_16 TIME,
    s8_17 TINYINT NOT NULL,
    u8_17 TINYINT UNSIGNED NOT NULL,
    s16_17 SMALLINT NOT NULL,
    u16_17 SMALLINT UNSIGNED NOT NULL,
    s32_17 INT NOT NULL,
    u32_17 INT UNSIGNED NOT NULL,
    s64_17 BIGINT NOT NULL,
    u64_17 BIGINT UNSIGNED NOT NULL,
    s1_17 VARCHAR(256),
    s2_17 TEXT,
    b1_17 VARBINARY(256),
    b2_17 BLOB,
    flt_17 FLOAT,
    dbl_17 DOUBLE,
    dt_17 DATE,
    dtime_17 DATETIME,
    t_17 TIME,
    s8_18 TINYINT NOT NULL,
    u8_18 TINYINT UNSIGNED NOT NULL,
    s16_18 SMALLINT NOT NULL,
    u16_18 SMALLINT UNSIGNED NOT NULL,
    s32_18 INT NOT NULL,
    u32_18 INT UNSIGNED NOT NULL,
    s64_18 BIGINT NOT NULL,
    u64_18 BIGINT UNSIGNED NOT NULL,
    s1_18 VARCHAR(256),
    s2_18 TEXT,
    b1_18 VARBINARY(256),
    b2_18 BLOB,
    flt_18 FLOAT,
    dbl_18 DOUBLE,
    dt_18 DATE,
    dtime_18 DATETIME,
    t_18 TIME,
    s8_19 TINYINT NOT NULL,
    u8_19 TINYINT UNSIGNED NOT NULL,
    s16_19 SMALLINT NOT NULL,
    u16_19 SMALLINT UNSIGNED NOT NULL,
    s32_19 INT NOT NULL,
    u32_19 INT UNSIGNED NOT NULL,
    s64_19 BIGINT NOT NULL,
    u64_19 BIGINT UNSIGNED NOT NULL,
    s1_19 VARCHAR(256),
    s2_19 TEXT,
    b1_19 VARBINARY(256),
    b2_19 BLOB,
    flt_19 FLOAT,
    dbl_19 DOUBLE,
    dt_19 DATE,
    dtime_19 DATETIME,
    t_19 TIME
)
)SQL";

constexpr const char* insert_data = R"SQL(
INSERT INTO myt
    (
        s8_0, u8_0, s16_0, u16_0, s32_0, u32_0, s64_0, u64_0, s1_0, s2_0, b1_0, b2_0, flt_0, dbl_0, dt_0, dtime_0, t_0,
        s8_1, u8_1, s16_1, u16_1, s32_1, u32_1, s64_1, u64_1, s1_1, s2_1, b1_1, b2_1, flt_1, dbl_1, dt_1, dtime_1, t_1,
        s8_2, u8_2, s16_2, u16_2, s32_2, u32_2, s64_2, u64_2, s1_2, s2_2, b1_2, b2_2, flt_2, dbl_2, dt_2, dtime_2, t_2,
        s8_3, u8_3, s16_3, u16_3, s32_3, u32_3, s64_3, u64_3, s1_3, s2_3, b1_3, b2_3, flt_3, dbl_3, dt_3, dtime_3, t_3,
        s8_4, u8_4, s16_4, u16_4, s32_4, u32_4, s64_4, u64_4, s1_4, s2_4, b1_4, b2_4, flt_4, dbl_4, dt_4, dtime_4, t_4,
        s8_5, u8_5, s16_5, u16_5, s32_5, u32_5, s64_5, u64_5, s1_5, s2_5, b1_5, b2_5, flt_5, dbl_5, dt_5, dtime_5, t_5,
        s8_6, u8_6, s16_6, u16_6, s32_6, u32_6, s64_6, u64_6, s1_6, s2_6, b1_6, b2_6, flt_6, dbl_6, dt_6, dtime_6, t_6,
        s8_7, u8_7, s16_7, u16_7, s32_7, u32_7, s64_7, u64_7, s1_7, s2_7, b1_7, b2_7, flt_7, dbl_7, dt_7, dtime_7, t_7,
        s8_8, u8_8, s16_8, u16_8, s32_8, u32_8, s64_8, u64_8, s1_8, s2_8, b1_8, b2_8, flt_8, dbl_8, dt_8, dtime_8, t_8,
        s8_9, u8_9, s16_9, u16_9, s32_9, u32_9, s64_9, u64_9, s1_9, s2_9, b1_9, b2_9, flt_9, dbl_9, dt_9, dtime_9, t_9,
        s8_10, u8_10, s16_10, u16_10, s32_10, u32_10, s64_10, u64_10, s1_10, s2_10, b1_10, b2_10, flt_10, dbl_10, dt_10, dtime_10, t_10,
        s8_11, u8_11, s16_11, u16_11, s32_11, u32_11, s64_11, u64_11, s1_11, s2_11, b1_11, b2_11, flt_11, dbl_11, dt_11, dtime_11, t_11,
        s8_12, u8_12, s16_12, u16_12, s32_12, u32_12, s64_12, u64_12, s1_12, s2_12, b1_12, b2_12, flt_12, dbl_12, dt_12, dtime_12, t_12,
        s8_13, u8_13, s16_13, u16_13, s32_13, u32_13, s64_13, u64_13, s1_13, s2_13, b1_13, b2_13, flt_13, dbl_13, dt_13, dtime_13, t_13,
        s8_14, u8_14, s16_14, u16_14, s32_14, u32_14, s64_14, u64_14, s1_14, s2_14, b1_14, b2_14, flt_14, dbl_14, dt_14, dtime_14, t_14,
        s8_15, u8_15, s16_15, u16_15, s32_15, u32_15, s64_15, u64_15, s1_15, s2_15, b1_15, b2_15, flt_15, dbl_15, dt_15, dtime_15, t_15,
        s8_16, u8_16, s16_16, u16_16, s32_16, u32_16, s64_16, u64_16, s1_16, s2_16, b1_16, b2_16, flt_16, dbl_16, dt_16, dtime_16, t_16,
        s8_17, u8_17, s16_17, u16_17, s32_17, u32_17, s64_17, u64_17, s1_17, s2_17, b1_17, b2_17, flt_17, dbl_17, dt_17, dtime_17, t_17,
        s8_18, u8_18, s16_18, u16_18, s32_18, u32_18, s64_18, u64_18, s1_18, s2_18, b1_18, b2_18, flt_18, dbl_18, dt_18, dtime_18, t_18,
        s8_19, u8_19, s16_19, u16_19, s32_19, u32_19, s64_19, u64_19, s1_19, s2_19, b1_19, b2_19, flt_19, dbl_19, dt_19, dtime_19, t_19
    )
    VALUES (
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600)),
        FLOOR(RAND()*(0x7f+0x80+1)-0x80),
        FLOOR(RAND()*(0xff+1)),
        FLOOR(RAND()*(0x7fff+0x8000+1)-0x8000),
        FLOOR(RAND()*(0xffff+1)),
        FLOOR(RAND()*(0x7fffffff+0x80000000+1)-0x80000000),
        FLOOR(RAND()*(0xffffffff+1)),
        FLOOR(RAND()*(0x7fffffffffffffff+0x8000000000000000)-0x7fffffffffffffff),
        FLOOR(RAND()*(0xffffffffffffffff)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        REPEAT(UUID(), 5),
        REPEAT(UUID(), FLOOR(RAND()*(1500-1000+1)+1000)),
        RAND(),
        RAND(),
        CURDATE(),
        CURTIME(),
        SEC_TO_TIME(RAND() + FLOOR(RAND()*(839*3600+839*3600+1)-839*3600))
    )
)SQL";

void test_error(MYSQL* mysql, int status)
{
    if (status)
    {
        fprintf(stderr, "%s\n", mysql_error(mysql));
        mysql_close(mysql);
        exit(1);
    }
}
void test_stmt_error(MYSQL_STMT* mysql, int status)
{
    if (status)
    {
        fprintf(stderr, "%s\n", mysql_stmt_error(mysql));
        exit(1);
    }
}

int main()
{
    if (mysql_library_init(0, NULL, NULL))
    {
        fprintf(stderr, "could not initialize MySQL client library\n");
        exit(1);
    }
    MYSQL* con = mysql_init(NULL);
    printf("Client flag: %lx\n", con->client_flag);
    printf("Client flag option: %lx\n", con->options.client_flag);

    unsigned mode = SSL_MODE_DISABLED;
    if (mysql_options(con, MYSQL_OPT_SSL_MODE, &mode))
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        exit(1);
    }

    if (con == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        exit(1);
    }

    if (mysql_real_connect(con, NULL, "root", "", "mytest", 0, "/var/run/mysqld/mysqld.sock", 0) == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
        exit(1);
    }

    // Run setup data
    if (mysql_real_query(con, create_table, strlen(create_table)))
    {
        fprintf(stderr, "Error running create table: %s\n", mysql_error(con));
        exit(1);
    }
    if (mysql_real_query(con, insert_data, strlen(insert_data)))
    {
        fprintf(stderr, "Error running insert data: %s\n", mysql_error(con));
        exit(1);
    }

    // Prepare stmt
    MYSQL_STMT* stmt;
    stmt = mysql_stmt_init(con);
    if (!stmt)
    {
        printf("Could not initialize statement\n");
        exit(1);
    }
    constexpr const char* stmt_str =
        "SELECT id, s8_0, u8_0, s16_0, u16_0, s32_0, u32_0, s64_0, u64_0, s1_0, s2_0, b1_0, b2_0, flt_0, "
        "dbl_0, dt_0, dtime_0, t_0 FROM myt WHERE id = 0";
    if (mysql_stmt_prepare(stmt, stmt_str, strlen(stmt_str)))
    {
        fprintf(stderr, "Error preparing statement: %s\n", mysql_stmt_error(stmt));
        exit(1);
    }

    // Prepare the bind objects
    long long int out_id = 0;
    MYSQL_BIND binds[21]{};
    binds[0].buffer_type = MYSQL_TYPE_LONGLONG;
    binds[0].buffer = &out_id;
    binds[0].buffer_length = 8;

    signed char s8{};
    binds[1].buffer_type = MYSQL_TYPE_TINY;
    binds[1].buffer = &s8;
    binds[1].buffer_length = 1;
    binds[1].is_unsigned = 0;

    unsigned char u8{};
    binds[2].buffer_type = MYSQL_TYPE_TINY;
    binds[2].buffer = &u8;
    binds[2].buffer_length = 1;
    binds[2].is_unsigned = 1;

    short s16{};
    binds[3].buffer_type = MYSQL_TYPE_SHORT;
    binds[3].buffer = &s16;
    binds[3].buffer_length = 2;
    binds[3].is_unsigned = 0;

    unsigned short u16{};
    binds[4].buffer_type = MYSQL_TYPE_SHORT;
    binds[4].buffer = &u16;
    binds[4].buffer_length = 2;
    binds[4].is_unsigned = 1;

    int s32{};
    binds[5].buffer_type = MYSQL_TYPE_LONG;
    binds[5].buffer = &s32;
    binds[5].buffer_length = 4;
    binds[5].is_unsigned = 0;

    unsigned u32{};
    binds[6].buffer_type = MYSQL_TYPE_LONG;
    binds[6].buffer = &u32;
    binds[6].buffer_length = 4;
    binds[6].is_unsigned = 1;

    long long s64{};
    binds[7].buffer_type = MYSQL_TYPE_LONGLONG;
    binds[7].buffer = &s64;
    binds[7].buffer_length = 8;
    binds[7].is_unsigned = 0;

    unsigned long long u64{};
    binds[8].buffer_type = MYSQL_TYPE_LONGLONG;
    binds[8].buffer = &u64;
    binds[8].buffer_length = 8;
    binds[8].is_unsigned = 1;

    char s1[255]{};
    binds[9].buffer_type = MYSQL_TYPE_STRING;
    binds[9].buffer = s1;
    binds[9].buffer_length = sizeof(s1);

    std::string s2;
    unsigned long s2_length = 0u;
    bool s2_truncated{};
    binds[10].buffer_type = MYSQL_TYPE_STRING;
    binds[10].buffer = s2.data();
    binds[10].buffer_length = s2_length;
    binds[10].length = &s2_length;
    binds[10].error = &s2_truncated;

    char b1[255]{};
    binds[11].buffer_type = MYSQL_TYPE_BLOB;
    binds[11].buffer = b1;
    binds[11].buffer_length = sizeof(b1);

    std::string b2;
    unsigned long b2_length = 0u;
    bool b2_truncated{};
    binds[12].buffer_type = MYSQL_TYPE_BLOB;
    binds[12].buffer = b2.data();
    binds[12].buffer_length = b2_length;
    binds[12].length = &b2_length;
    binds[12].error = &b2_truncated;

    float flt{};
    binds[12].buffer_type = MYSQL_TYPE_FLOAT;
    binds[12].buffer = &flt;
    binds[12].buffer_length = 4;

    double dbl{};
    binds[13].buffer_type = MYSQL_TYPE_DOUBLE;
    binds[13].buffer = &dbl;
    binds[13].buffer_length = 8;

    MYSQL_TIME dt{};
    binds[14].buffer_type = MYSQL_TYPE_DATE;
    binds[14].buffer = &dt;
    binds[14].buffer_length = sizeof(dt);

    MYSQL_TIME dtime{};
    binds[15].buffer_type = MYSQL_TYPE_DATETIME;
    binds[15].buffer = &dtime;
    binds[15].buffer_length = sizeof(dtime);

    MYSQL_TIME t{};
    binds[16].buffer_type = MYSQL_TYPE_TIME;
    binds[16].buffer = &t;
    binds[16].buffer_length = sizeof(t);

    //
    // bench begins here
    //
    if (mysql_stmt_execute(stmt))
    {
        fprintf(stderr, "Error executing statement: %s\n", mysql_stmt_error(stmt));
        exit(1);
    }

    if (mysql_stmt_bind_result(stmt, binds))
    {
        fprintf(stderr, "Error binding result: %s\n", mysql_stmt_error(stmt));
        exit(1);
    }

    while (true)
    {
        auto status = mysql_stmt_fetch(stmt);

        if (status == MYSQL_DATA_TRUNCATED)
        {
            if (s2_length > s2.size())
            {
                s2.resize(s2_length);
                binds[10].buffer = s2.data();
                binds[10].buffer_length = s2_length;
            }

            if (b2_length > b2.size())
            {
                b2.resize(b2_length);
                binds[12].buffer = b2.data();
                binds[12].buffer_length = b2_length;
            }
        }
        else if (status == MYSQL_NO_DATA)
        {
            break;
        }
        else if (status == 1)
        {
            fprintf(stderr, "Error fetching result: %s\n", mysql_stmt_error(stmt));
            exit(1);
        }

        // TODO: remove this
        std::cout << "s8=" << +s8 << ", u8=" << +u8 << ", s16=" << s16 << ", u16=" << u16 << ", s32=" << s32
                  << ", u32=" << u32 << ", s64=" << s64 << ", u64=" << u64 << ", s1=" << s1 << ", s2=" << s2
                  << ", b1=" << b1 << ", b2=" << b2 << std::endl;
    }

    mysql_stmt_close(stmt);
    mysql_close(con);
    exit(0);
}