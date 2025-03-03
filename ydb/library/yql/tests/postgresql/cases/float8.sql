--
-- FLOAT8
--
CREATE TABLE FLOAT8_TBL(f1 float8);
INSERT INTO FLOAT8_TBL(f1) VALUES ('    0.0   ');
INSERT INTO FLOAT8_TBL(f1) VALUES ('1004.30  ');
INSERT INTO FLOAT8_TBL(f1) VALUES ('   -34.84');
INSERT INTO FLOAT8_TBL(f1) VALUES ('1.2345678901234e+200');
INSERT INTO FLOAT8_TBL(f1) VALUES ('1.2345678901234e-200');
-- test for underflow and overflow handling
SELECT '10e400'::float8;
SELECT '-10e400'::float8;
SELECT '10e-400'::float8;
SELECT '-10e-400'::float8;
-- bad input
INSERT INTO FLOAT8_TBL(f1) VALUES ('');
INSERT INTO FLOAT8_TBL(f1) VALUES ('     ');
INSERT INTO FLOAT8_TBL(f1) VALUES ('xyz');
INSERT INTO FLOAT8_TBL(f1) VALUES ('5.0.0');
INSERT INTO FLOAT8_TBL(f1) VALUES ('5 . 0');
INSERT INTO FLOAT8_TBL(f1) VALUES ('5.   0');
INSERT INTO FLOAT8_TBL(f1) VALUES ('    - 3');
INSERT INTO FLOAT8_TBL(f1) VALUES ('123           5');
-- special inputs
SELECT 'NaN'::float8;
SELECT 'nan'::float8;
SELECT '   NAN  '::float8;
SELECT 'infinity'::float8;
SELECT '          -INFINiTY   '::float8;
-- bad special inputs
SELECT 'N A N'::float8;
SELECT 'NaN x'::float8;
SELECT ' INFINITY    x'::float8;
SELECT 'Infinity'::float8 + 100.0;
SELECT 'Infinity'::float8 / 'Infinity'::float8;
SELECT '42'::float8 / 'Infinity'::float8;
SELECT 'nan'::float8 / 'nan'::float8;
SELECT 'nan'::float8 / '0'::float8;
SELECT 'nan'::numeric::float8;
SELECT * FROM FLOAT8_TBL;
SELECT f.* FROM FLOAT8_TBL f WHERE f.f1 <> '1004.3';
SELECT f.* FROM FLOAT8_TBL f WHERE f.f1 = '1004.3';
SELECT f.* FROM FLOAT8_TBL f WHERE '1004.3' > f.f1;
SELECT f.* FROM FLOAT8_TBL f WHERE  f.f1 < '1004.3';
SELECT f.* FROM FLOAT8_TBL f WHERE '1004.3' >= f.f1;
SELECT f.* FROM FLOAT8_TBL f WHERE  f.f1 <= '1004.3';
SELECT f.f1, f.f1 * '-10' AS x
   FROM FLOAT8_TBL f
   WHERE f.f1 > '0.0';
SELECT f.f1, f.f1 + '-10' AS x
   FROM FLOAT8_TBL f
   WHERE f.f1 > '0.0';
SELECT f.f1, f.f1 / '-10' AS x
   FROM FLOAT8_TBL f
   WHERE f.f1 > '0.0';
SELECT f.f1, f.f1 - '-10' AS x
   FROM FLOAT8_TBL f
   WHERE f.f1 > '0.0';
SELECT f.f1 ^ '2.0' AS square_f1
   FROM FLOAT8_TBL f where f.f1 = '1004.3';
-- absolute value
SELECT f.f1, @f.f1 AS abs_f1
   FROM FLOAT8_TBL f;
-- truncate
SELECT f.f1, trunc(f.f1) AS trunc_f1
   FROM FLOAT8_TBL f;
-- round
SELECT f.f1, round(f.f1) AS round_f1
   FROM FLOAT8_TBL f;
-- avoid bit-exact output here because operations may not be bit-exact.
SET extra_float_digits = 0;
-- square root
SELECT sqrt(float8 '64') AS eight;
SELECT |/ float8 '64' AS eight;
-- power
SELECT power(float8 '144', float8 '0.5');
SELECT power(float8 'NaN', float8 '0.5');
SELECT power(float8 '144', float8 'NaN');
SELECT power(float8 'NaN', float8 'NaN');
SELECT power(float8 '-1', float8 'NaN');
SELECT power(float8 '1', float8 'NaN');
SELECT power(float8 'NaN', float8 '0');
SELECT power(float8 'inf', float8 '0');
SELECT power(float8 '-inf', float8 '0');
SELECT power(float8 '0', float8 'inf');
SELECT power(float8 '0', float8 '-inf');
SELECT power(float8 '1', float8 'inf');
SELECT power(float8 '1', float8 '-inf');
SELECT power(float8 '-1', float8 'inf');
SELECT power(float8 '-1', float8 '-inf');
SELECT power(float8 '0.1', float8 'inf');
SELECT power(float8 '-0.1', float8 'inf');
SELECT power(float8 '1.1', float8 'inf');
SELECT power(float8 '-1.1', float8 'inf');
SELECT power(float8 '0.1', float8 '-inf');
SELECT power(float8 '-0.1', float8 '-inf');
SELECT power(float8 '1.1', float8 '-inf');
SELECT power(float8 '-1.1', float8 '-inf');
SELECT power(float8 'inf', float8 '-2');
SELECT power(float8 'inf', float8 '2');
SELECT power(float8 'inf', float8 'inf');
SELECT power(float8 'inf', float8 '-inf');
-- Intel's icc misoptimizes the code that controls the sign of this result,
-- even with -mp1.  Pending a fix for that, only test for "is it zero".
SELECT power(float8 '-inf', float8 '-2') = '0';
SELECT power(float8 '-inf', float8 '-3');
SELECT power(float8 '-inf', float8 '2');
SELECT power(float8 '-inf', float8 '3');
SELECT power(float8 '-inf', float8 '3.5');
SELECT power(float8 '-inf', float8 'inf');
SELECT power(float8 '-inf', float8 '-inf');
-- check edge cases for exp
SELECT exp('inf'::float8), exp('-inf'::float8), exp('nan'::float8);
SELECT * FROM FLOAT8_TBL;
SELECT f.f1 * '1e200' from FLOAT8_TBL f;
SELECT f.f1 ^ '1e200' from FLOAT8_TBL f;
SELECT 0 ^ 0 + 0 ^ 1 + 0 ^ 0.0 + 0 ^ 0.5;
SELECT ln(f.f1) from FLOAT8_TBL f where f.f1 = '0.0' ;
SELECT ln(f.f1) from FLOAT8_TBL f where f.f1 < '0.0' ;
SELECT f.f1 / '0.0' from FLOAT8_TBL f;
SELECT asinh(float8 '1');
-- test Inf/NaN cases for hyperbolic functions
SELECT sinh(float8 'infinity');
SELECT sinh(float8 '-infinity');
SELECT sinh(float8 'nan');
SELECT cosh(float8 'infinity');
SELECT cosh(float8 '-infinity');
SELECT cosh(float8 'nan');
SELECT tanh(float8 'infinity');
SELECT tanh(float8 '-infinity');
SELECT tanh(float8 'nan');
SELECT asinh(float8 'infinity');
SELECT asinh(float8 '-infinity');
SELECT asinh(float8 'nan');
-- acosh(Inf) should be Inf, but some mingw versions produce NaN, so skip test
-- SELECT acosh(float8 'infinity');
SELECT acosh(float8 '-infinity');
SELECT acosh(float8 'nan');
SELECT atanh(float8 'infinity');
SELECT atanh(float8 '-infinity');
SELECT atanh(float8 'nan');
RESET extra_float_digits;
-- test for over- and underflow
INSERT INTO FLOAT8_TBL(f1) VALUES ('10e400');
INSERT INTO FLOAT8_TBL(f1) VALUES ('-10e400');
INSERT INTO FLOAT8_TBL(f1) VALUES ('10e-400');
INSERT INTO FLOAT8_TBL(f1) VALUES ('-10e-400');
INSERT INTO FLOAT8_TBL(f1) VALUES ('0.0');
INSERT INTO FLOAT8_TBL(f1) VALUES ('-34.84');
INSERT INTO FLOAT8_TBL(f1) VALUES ('-1004.30');
INSERT INTO FLOAT8_TBL(f1) VALUES ('-1.2345678901234e+200');
INSERT INTO FLOAT8_TBL(f1) VALUES ('-1.2345678901234e-200');
-- test edge-case coercions to integer
SELECT '32767.4'::float8::int2;
SELECT '32767.6'::float8::int2;
SELECT '-32768.4'::float8::int2;
SELECT '-32768.6'::float8::int2;
SELECT '2147483647.4'::float8::int4;
SELECT '2147483647.6'::float8::int4;
SELECT '-2147483648.4'::float8::int4;
SELECT '-2147483648.6'::float8::int4;
SELECT '9223372036854773760'::float8::int8;
SELECT '9223372036854775807'::float8::int8;
SELECT '-9223372036854775808.5'::float8::int8;
SELECT '-9223372036854780000'::float8::int8;
