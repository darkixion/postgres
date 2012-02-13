--
-- Test inheritance features
--
CREATE TABLE a (aa TEXT);
CREATE TABLE b (bb TEXT) INHERITS (a);
CREATE TABLE c (cc TEXT) INHERITS (a);
CREATE TABLE d (dd TEXT) INHERITS (b,c,a);

INSERT INTO a(aa) VALUES('aaa');
INSERT INTO a(aa) VALUES('aaaa');
INSERT INTO a(aa) VALUES('aaaaa');
INSERT INTO a(aa) VALUES('aaaaaa');
INSERT INTO a(aa) VALUES('aaaaaaa');
INSERT INTO a(aa) VALUES('aaaaaaaa');

INSERT INTO b(aa) VALUES('bbb');
INSERT INTO b(aa) VALUES('bbbb');
INSERT INTO b(aa) VALUES('bbbbb');
INSERT INTO b(aa) VALUES('bbbbbb');
INSERT INTO b(aa) VALUES('bbbbbbb');
INSERT INTO b(aa) VALUES('bbbbbbbb');

INSERT INTO c(aa) VALUES('ccc');
INSERT INTO c(aa) VALUES('cccc');
INSERT INTO c(aa) VALUES('ccccc');
INSERT INTO c(aa) VALUES('cccccc');
INSERT INTO c(aa) VALUES('ccccccc');
INSERT INTO c(aa) VALUES('cccccccc');

INSERT INTO d(aa) VALUES('ddd');
INSERT INTO d(aa) VALUES('dddd');
INSERT INTO d(aa) VALUES('ddddd');
INSERT INTO d(aa) VALUES('dddddd');
INSERT INTO d(aa) VALUES('ddddddd');
INSERT INTO d(aa) VALUES('dddddddd');

SELECT relname, a.* FROM a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM d, pg_class where d.tableoid = pg_class.oid;
SELECT relname, a.* FROM ONLY a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM ONLY b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM ONLY c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM ONLY d, pg_class where d.tableoid = pg_class.oid;

UPDATE a SET aa='zzzz' WHERE aa='aaaa';
UPDATE ONLY a SET aa='zzzzz' WHERE aa='aaaaa';
UPDATE b SET aa='zzz' WHERE aa='aaa';
UPDATE ONLY b SET aa='zzz' WHERE aa='aaa';
UPDATE a SET aa='zzzzzz' WHERE aa LIKE 'aaa%';

SELECT relname, a.* FROM a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM d, pg_class where d.tableoid = pg_class.oid;
SELECT relname, a.* FROM ONLY a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM ONLY b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM ONLY c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM ONLY d, pg_class where d.tableoid = pg_class.oid;

UPDATE b SET aa='new';

SELECT relname, a.* FROM a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM d, pg_class where d.tableoid = pg_class.oid;
SELECT relname, a.* FROM ONLY a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM ONLY b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM ONLY c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM ONLY d, pg_class where d.tableoid = pg_class.oid;

UPDATE a SET aa='new';

DELETE FROM ONLY c WHERE aa='new';

SELECT relname, a.* FROM a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM d, pg_class where d.tableoid = pg_class.oid;
SELECT relname, a.* FROM ONLY a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM ONLY b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM ONLY c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM ONLY d, pg_class where d.tableoid = pg_class.oid;

DELETE FROM a;

SELECT relname, a.* FROM a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM d, pg_class where d.tableoid = pg_class.oid;
SELECT relname, a.* FROM ONLY a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM ONLY b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM ONLY c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM ONLY d, pg_class where d.tableoid = pg_class.oid;

-- Confirm PRIMARY KEY adds NOT NULL constraint to child table
CREATE TEMP TABLE z (b TEXT, PRIMARY KEY(aa, b)) inherits (a);
INSERT INTO z VALUES (NULL, 'text'); -- should fail

-- Check UPDATE with inherited target and an inherited source table
create temp table foo(f1 int, f2 int);
create temp table foo2(f3 int) inherits (foo);
create temp table bar(f1 int, f2 int);
create temp table bar2(f3 int) inherits (bar);

insert into foo values(1,1);
insert into foo values(3,3);
insert into foo2 values(2,2,2);
insert into foo2 values(3,3,3);
insert into bar values(1,1);
insert into bar values(2,2);
insert into bar values(3,3);
insert into bar values(4,4);
insert into bar2 values(1,1,1);
insert into bar2 values(2,2,2);
insert into bar2 values(3,3,3);
insert into bar2 values(4,4,4);

update bar set f2 = f2 + 100 where f1 in (select f1 from foo);

SELECT relname, bar.* FROM bar, pg_class where bar.tableoid = pg_class.oid
order by 1,2;

/* Test multiple inheritance of column defaults */

CREATE TABLE firstparent (tomorrow date default now()::date + 1);
CREATE TABLE secondparent (tomorrow date default  now() :: date  +  1);
CREATE TABLE jointchild () INHERITS (firstparent, secondparent);  -- ok
CREATE TABLE thirdparent (tomorrow date default now()::date - 1);
CREATE TABLE otherchild () INHERITS (firstparent, thirdparent);  -- not ok
CREATE TABLE otherchild (tomorrow date default now())
  INHERITS (firstparent, thirdparent);  -- ok, child resolves ambiguous default

DROP TABLE firstparent, secondparent, jointchild, thirdparent, otherchild;

-- Test changing the type of inherited columns
insert into d values('test','one','two','three');
alter table a alter column aa type integer using bit_length(aa);
select * from d;

-- Test non-inheritable parent constraints
create table p1(ff1 int);
alter table only p1 add constraint p1chk check (ff1 > 0);
alter table p1 add constraint p2chk check (ff1 > 10);
-- conisonly should be true for ONLY constraint
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pgc.conisonly from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname = 'p1' order by 1,2;

-- Test that child does not inherit ONLY constraints
create table c1 () inherits (p1);
\d p1
\d c1

drop table p1 cascade;

-- Tests for casting between the rowtypes of parent and child
-- tables. See the pgsql-hackers thread beginning Dec. 4/04
create table base (i integer);
create table derived () inherits (base);
insert into derived (i) values (0);
select derived::base from derived;
drop table derived;
drop table base;

create table p1(ff1 int);
create table p2(f1 text);
create function p2text(p2) returns text as 'select $1.f1' language sql;
create table c1(f3 int) inherits(p1,p2);
insert into c1 values(123456789, 'hi', 42);
select p2text(c1.*) from c1;
drop function p2text(p2);
drop table c1;
drop table p2;
drop table p1;

CREATE TABLE ac (aa TEXT);
alter table ac add constraint ac_check check (aa is not null);
CREATE TABLE bc (bb TEXT) INHERITS (ac);
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pgc.consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;

insert into ac (aa) values (NULL);
insert into bc (aa) values (NULL);

alter table bc drop constraint ac_check;  -- fail, disallowed
alter table ac drop constraint ac_check;
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pgc.consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;

-- try the unnamed-constraint case
alter table ac add check (aa is not null);
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pgc.consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;

insert into ac (aa) values (NULL);
insert into bc (aa) values (NULL);

alter table bc drop constraint ac_aa_check;  -- fail, disallowed
alter table ac drop constraint ac_aa_check;
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pgc.consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;

alter table ac add constraint ac_check check (aa is not null);
alter table bc no inherit ac;
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pgc.consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;
alter table bc drop constraint ac_check;
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pgc.consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;
alter table ac drop constraint ac_check;
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pgc.consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;

drop table bc;
drop table ac;

create table ac (a int constraint check_a check (a <> 0));
create table bc (a int constraint check_a check (a <> 0), b int constraint check_b check (b <> 0)) inherits (ac);
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pgc.consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;

drop table bc;
drop table ac;

create table ac (a int constraint check_a check (a <> 0));
create table bc (b int constraint check_b check (b <> 0));
create table cc (c int constraint check_c check (c <> 0)) inherits (ac, bc);
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pgc.consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc', 'cc') order by 1,2;

alter table cc no inherit bc;
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pgc.consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc', 'cc') order by 1,2;

drop table cc;
drop table bc;
drop table ac;

create table p1(f1 int);
create table p2(f2 int);
create table c1(f3 int) inherits(p1,p2);
insert into c1 values(1,-1,2);
alter table p2 add constraint cc check (f2>0);  -- fail
alter table p2 add check (f2>0);  -- check it without a name, too
delete from c1;
insert into c1 values(1,1,2);
alter table p2 add check (f2>0);
insert into c1 values(1,-1,2);  -- fail
create table c2(f3 int) inherits(p1,p2);
\d c2
create table c3 (f4 int) inherits(c1,c2);
\d c3
drop table p1 cascade;
drop table p2 cascade;

create table pp1 (f1 int);
create table cc1 (f2 text, f3 int) inherits (pp1);
alter table pp1 add column a1 int check (a1 > 0);
\d cc1
create table cc2(f4 float) inherits(pp1,cc1);
\d cc2
alter table pp1 add column a2 int check (a2 > 0);
\d cc2
drop table pp1 cascade;

-- Test for renaming in simple multiple inheritance
CREATE TABLE inht1 (a int, b int);
CREATE TABLE inhs1 (b int, c int);
CREATE TABLE inhts (d int) INHERITS (inht1, inhs1);

ALTER TABLE inht1 RENAME a TO aa;
ALTER TABLE inht1 RENAME b TO bb;                -- to be failed
ALTER TABLE inhts RENAME aa TO aaa;      -- to be failed
ALTER TABLE inhts RENAME d TO dd;
\d+ inhts

DROP TABLE inhts;

-- Test for renaming in diamond inheritance
CREATE TABLE inht2 (x int) INHERITS (inht1);
CREATE TABLE inht3 (y int) INHERITS (inht1);
CREATE TABLE inht4 (z int) INHERITS (inht2, inht3);

ALTER TABLE inht1 RENAME aa TO aaa;
\d+ inht4

CREATE TABLE inhts (d int) INHERITS (inht2, inhs1);
ALTER TABLE inht1 RENAME aaa TO aaaa;
ALTER TABLE inht1 RENAME b TO bb;                -- to be failed
\d+ inhts

WITH RECURSIVE r AS (
  SELECT 'inht1'::regclass AS inhrelid
UNION ALL
  SELECT c.inhrelid FROM pg_inherits c, r WHERE r.inhrelid = c.inhparent
)
SELECT a.attrelid::regclass, a.attname, a.attinhcount, e.expected
  FROM (SELECT inhrelid, count(*) AS expected FROM pg_inherits
        WHERE inhparent IN (SELECT inhrelid FROM r) GROUP BY inhrelid) e
  JOIN pg_attribute a ON e.inhrelid = a.attrelid WHERE NOT attislocal
  ORDER BY a.attrelid::regclass::name, a.attnum;

DROP TABLE inht1, inhs1 CASCADE;

--
-- Test merge-append plans for inheritance trees
--

create table matest0 (id serial primary key, name text);
create table matest1 (id integer primary key) inherits (matest0);
create table matest2 (id integer primary key) inherits (matest0);
create table matest3 (id integer primary key) inherits (matest0);

create index matest0i on matest0 ((1-id));
create index matest1i on matest1 ((1-id));
-- create index matest2i on matest2 ((1-id));  -- intentionally missing
create index matest3i on matest3 ((1-id));

insert into matest1 (name) values ('Test 1');
insert into matest1 (name) values ('Test 2');
insert into matest2 (name) values ('Test 3');
insert into matest2 (name) values ('Test 4');
insert into matest3 (name) values ('Test 5');
insert into matest3 (name) values ('Test 6');

set enable_indexscan = off;  -- force use of seqscan/sort, so no merge
explain (verbose, costs off) select * from matest0 order by 1-id;
select * from matest0 order by 1-id;
reset enable_indexscan;

set enable_seqscan = off;  -- plan with fewest seqscans should be merge
explain (verbose, costs off) select * from matest0 order by 1-id;
select * from matest0 order by 1-id;
reset enable_seqscan;

drop table matest0 cascade;

--
-- Test merge-append for UNION ALL append relations
-- Check handling of duplicated, constant, or volatile targetlist items
--

set enable_seqscan = off;
set enable_indexscan = on;
set enable_bitmapscan = off;

explain (costs off)
SELECT thousand, tenthous FROM tenk1
UNION ALL
SELECT thousand, thousand FROM tenk1
ORDER BY thousand, tenthous;

explain (costs off)
SELECT thousand, tenthous, thousand+tenthous AS x FROM tenk1
UNION ALL
SELECT 42, 42, hundred FROM tenk1
ORDER BY thousand, tenthous;

explain (costs off)
SELECT thousand, tenthous FROM tenk1
UNION ALL
SELECT thousand, random()::integer FROM tenk1
ORDER BY thousand, tenthous;

reset enable_seqscan;
reset enable_indexscan;
reset enable_bitmapscan;
