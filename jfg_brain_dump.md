
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

I would like to implement (not fix, because it is a feature request)
[Bug#106645: low query log is not logging database/schema name](https://bugs.mysql.com/bug.php?id=106645).

This improvement was suggested to me (JFG) by a colleague (voluntarily
holding details to avoid exposing internal Aiven politics in public GitHub).
While starting to work on this, I found there was a public FR on the subject.

This colleague told me Percona Server has this feature.  I will refrain from
checking PS code, but I will allow myself to check PS behavior to make sure
this change lands in PS without too much complication for Percona.
The section on [Slow Query Log File Examples](#slow-query-log-file-examples)
contains MySQL, Percona Server and MariaDB samples.

Note: [Bug#106645](https://bugs.mysql.com/bug.php?id=106645)
was open in 2022, but people have been asking for
this feature for a long time.  This is from 2006:
[Bug#19046: slow query log should include the affected database](https://bugs.mysql.com/bug.php?id=19046).

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

As mentioned in the public bug report / feature request
([Bug#106645](https://bugs.mysql.com/bug.php?id=106645)), the `mysql.slow_log`
table already includes a `db` column, so it to the file should
not be too complicated.  The marginal additional complexity will be in
making sure this change does not break existing tooling parsing the file.  A way
to achieve this is to put the change behind a feature flag / global
variable.  This also allows back-porting this change in 8.0 and 8.4 (with
the default to OFF).  Eventually, this global variable could be deprecated if
the change is considered good for everyone (IMHO, it is), but the removal should
wait to leave time to adjust slow query log file parsing.  To speed up removal,
the variable could be introduced as deprecated with the default to ON.
The name I have in mind for this variable is
`log_slow_extra_db = { OFF | ON }`.

Update: the `db` column of the `mysql.slow_log` table is a `VARCHAR NOT NULL`
while `SCHEMA_NAME` in `p_s.events_statements_summary_by_digest` is a
`VARCHAR DEFAULT NULL`.  So at some places,  no schema is stored as `NULL` and
at others as the empty-string.  This leads to weird situations.

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Update bis: another weird situation for no schema is that `thd->db().str` is
sometimes `NULL` and sometime not with `thd->db().length == 0`.  This makes
no schema represented as 2 states: `NULL` and the empty-string.  An example
is that, for a `CREATE TABLE` done in no schema, it is `NULL` on the primary
and the empty-string on replicas (see
[Bug#115203](https://bugs.mysql.com/bug.php?id=115203) for details).

Note that a variable to control the output of the slow log is not unheard
of, we already have:
- [log_slow_admin_statements](https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html#sysvar_log_slow_admin_statements)
- [log_slow_extra](https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html#sysvar_log_slow_extra)
- [log_slow_replica_statements](https://dev.mysql.com/doc/refman/8.4/en/replication-options-replica.html#sysvar_log_slow_replica_statements)
- [long_query_time](https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html#sysvar_long_query_time)
- [min_examined_row_limit](https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html#sysvar_min_examined_row_limit)

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Currently, MySQL logs a line like below in the slow query log file.
```
# User@Host: msandbox[msandbox] @ localhost []  Id:    12
```

What I have in mind, when `log_slow_extra_db` is `ON`, is to change above for below
(see `Db: ...` at the end of the line).
```
# User@Host: msandbox[msandbox] @ localhost []  Id:    12  Db: test_jfg
```

Update: another option that I did not consider initially is to always have
`use ...` in the slow query log file, see below (TL&DR: I will probably not do
this).

I could use `Schema:` instead of `Db:`, but this would not match the content
of the `mysql.slow_log` table, even though it would partially match
`performance_schema.events_statements_summary_by_digest` (`SCHEMA_NAME`).  Not
matching `mysql.slow_log` is not unheard of, `mysql.slow_log.thread_id` is logged
as `Id:`.  So I could change my mind about the name.

Addition: there is the case where no Db is selected.  In this case, having
`Db: ...` does not work.  For this, I log `NoDb` instead of `Db: ...`.

Update: instead of using `NoDb`, I could log `Db: ` (with the empty-string
db).  This is what Percona Server and MariaDB are doing, see
[No Db with PS and MariaDB](#no-db-with-ps-and-mariadb).

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Update 2024-07-09: I decided to replace `NoDb` by `Db: `.  According to
[the votes on the RFC PR](https://github.com/jfg956/mysql-server/pull/9#issuecomment-2160862534),
it is what people want.  Also, Tomer gives a good argument for this in
[his comment on the RFC PR](https://github.com/jfg956/mysql-server/pull/9#issuecomment-2164757435):
easier REGEXP matching.  This has been announced in a
[comment on the RFC PR](https://github.com/jfg956/mysql-server/pull/9#issuecomment-2218161395).

Also update 2024-07-09 / lol note: good thing I am not using `NoDb` anymore,
it is not very grep-able because InnoDB.

Interestingly, while doing this work, I saw that there is a `use ...` logged
with the 1st slow log entry in a different db, link to the code below.
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/log.cc#L805

This `use ...` looks redundant with `log_slow_extra_db = ON`, so I removed it.
It led to some rabbit-holing, see details in
[Slow Query Log File contains use](#slow-query-log-file-contains-use).

Update: instead of removing the `use ...`, we could embrace it and always log it.
This has the advantage of not changing the format of the log, but makes it
inconvenient to indicate queries not run in a database (`NoDb`).  From what I
know, there is no way, via a `use` command, to reset the current schema.  This
might be missing, but I will not, at least for now, start developing this
feature.  The absence of
a `use ...` could be a synonym for `NoDb`, but I am not sure I like this.
Anyhow, I might go down the road of always logging `use ...`, my mind is not
made yet.  Update: I prefer `NoDb` for indicating no schema/database is selected
(of the empty-string `Db: `), so not going down the `use ...` road.

While working on this, I discovered the
[mysqldumpslow](https://dev.mysql.com/doc/refman/8.4/en/mysqldumpslow.html)
utility.  Unclear what the impact of my work will be on this utility.
Update: none, see the section on [Testing mysqldumpslow](#testing-mysqldumpslow).

I am not updating mysqldumpslow to add the feature of parsing database / schema
in the slow query log file.  The current behavior of the tool is to aggregate
similar queries in different schemas, so making the tool schema-aware would
be a bigger change than what I think should be allocated to this.

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

While looking a mysqldumpslow, I saw it is broken with "administrator command",
which I reported in below.
- [Bug#115084: mysqldumpslow breaks on "administrator command"](https://bugs.mysql.com/bug.php?id=115084)

Percona notified in below (suggested by Daniel Black in his
[comments](https://github.com/jfg956/mysql-server/pull/8#pullrequestreview-2087006510)
on the work PR).
- https://perconadev.atlassian.net/browse/PT-2370

While doing this work, I "played with fire" and tried improving on the way the
function `File_query_log::write_slow` is implemented.  This leads to four
improvements in below commits:
- [Remove buff_len for administrator command](https://github.com/jfg956/mysql-server/pull/8/commits/cc3cbb3a85e2bd5dea97279c9c9782665ef92b8f);
- [With "Db:", make "Id:" format consistent with "Thread_id:"](https://github.com/jfg956/mysql-server/pull/8/commits/4899363462a638e5b3ded4f40de5cd50e45ff0d4);
- [Inline the declaration of buff_len](https://github.com/jfg956/mysql-server/pull/8/commits/a36225c5fb806b695ab3e19c4286158e21807ac8);
- [Making usage of end clearer with better assignments](https://github.com/jfg956/mysql-server/pull/8/commits/8d44f3090d86ebf3f5acb35143994c5e6e6231b5);
- "End of list".

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Doc Links

MySQL - The Slow Query log:
- 8.0: https://dev.mysql.com/doc/refman/8.0/en/slow-query-log.html
- 8.4: https://dev.mysql.com/doc/refman/8.4/en/slow-query-log.html

Percona - Slow query log:
- https://docs.percona.com/percona-server/8.0/slow-extended.html

MariaDB - Slow Query Log Overview:
- https://mariadb.com/kb/en/slow-query-log-overview/


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Code Notes

write_slow:
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/log.cc#L349

File_query_log::write_slow:
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/log.cc#L688

"fill database field":
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/log.cc#L1095

log_slow_extra (Sys_slow_log_extra):
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/sys_vars.cc#L5867


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Slow Query Log File Examples

Doing this in 8.0 because I want to compare with Percona Server (PS 8.4.0 is not
out at the time of doing this), and in .36 because PS .37 is not out (well, was
not when I did these tests).

From what I see, no change from 8.0 to 8.4.


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### MySQL 8.0.36

Defalut (`log_slow_extra = OFF`):
```
# Time: 2024-05-10T17:44:36.400424Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    12
# Query_time: 0.000237  Lock_time: 0.000004 Rows_sent: 0  Rows_examined: 0
SET timestamp=1715363076;
select * from t;
```

With `log_slow_extra = ON`:
```
# Time: 2024-05-10T17:45:09.474988Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    12
# Query_time: 0.000244  Lock_time: 0.000003 Rows_sent: 0  Rows_examined: 0 Thread_id: 12 Errno: 0 Killed: 0 Bytes_received: 22 Bytes_sent: 56 Read_first: 1 Read_last: 0 Read_key: 1 Read_next: 0 Read_prev: 0 Read_rnd: 0 Read_rnd_next: 1 Sort_merge_passes: 0 Sort_range_count: 0 Sort_rows: 0 Sort_scan_count: 0 Created_tmp_disk_tables: 0 Created_tmp_tables: 0 Start: 2024-05-10T17:45:09.474744Z End: 2024-05-10T17:45:09.474988Z
SET timestamp=1715363109;
select * from t;
```


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### PS 8.0.36

Default (`log_slow_extra = OFF` and `log_slow_verbosity = ''`): see line with
`Schema: ...` which is not in MySQL.
```
# Time: 2024-05-10T17:46:20.595386Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    12
# Schema: test_jfg  Last_errno: 0  Killed: 0
# Query_time: 0.000221  Lock_time: 0.000004  Rows_sent: 0  Rows_examined: 0  Rows_affected: 0  Bytes_sent: 56
SET timestamp=1715363180;
select * from t;
```

With `log_slow_extra = ON` (and `log_slow_verbosity = ''`): `Schema: ...` is gone,
looks like 100% MySQL compatible !
```
# Time: 2024-05-10T17:46:53.147579Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    12
# Query_time: 0.000259  Lock_time: 0.000004 Rows_sent: 0  Rows_examined: 0 Thread_id: 12 Errno: 0 Killed: 0 Bytes_received: 22 Bytes_sent: 56 Read_first: 1 Read_last: 0 Read_key: 1 Read_next: 0 Read_prev: 0 Read_rnd: 0 Read_rnd_next: 1 Sort_merge_passes: 0 Sort_range_count: 0 Sort_rows: 0 Sort_scan_count: 0 Created_tmp_disk_tables: 0 Created_tmp_tables: 0 Start: 2024-05-10T17:46:53.147320Z End: 2024-05-10T17:46:53.147579Z Schema: test_jfg Rows_affected: 0
SET timestamp=1715363213;
select * from t;
```

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

With `log_slow_verbosity = full` and `log_slow_extra = OFF`: back at PS style
(with `Schema: ...`).
```
# Time: 2024-05-10T17:52:26.336958Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    13
# Schema: test_jfg  Last_errno: 0  Killed: 0
# Query_time: 0.000218  Lock_time: 0.000004  Rows_sent: 0  Rows_examined: 0  Rows_affected: 0  Bytes_sent: 56
# Tmp_tables: 0  Tmp_disk_tables: 0  Tmp_table_sizes: 0
# InnoDB_trx_id: 0
# Full_scan: Yes  Full_join: No  Tmp_table: No  Tmp_table_on_disk: No
# Filesort: No  Filesort_on_disk: No  Merge_passes: 0
#   InnoDB_IO_r_ops: 0  InnoDB_IO_r_bytes: 0  InnoDB_IO_r_wait: 0.000000
#   InnoDB_rec_lock_wait: 0.000000  InnoDB_queue_wait: 0.000000
#   InnoDB_pages_distinct: 1
SET timestamp=1715363546;
select * from t;
```

With `log_slow_verbosity = full` and `log_slow_extra = ON`: back at MySQL
style (`Schema: ...` gone).
```
# Time: 2024-05-10T17:53:53.210758Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    14
# Query_time: 0.000248  Lock_time: 0.000005 Rows_sent: 0  Rows_examined: 0 Thread_id: 14 Errno: 0 Killed: 0 Bytes_received: 22 Bytes_sent: 56 Read_first: 1 Read_last: 0 Read_key: 1 Read_next: 0 Read_prev: 0 Read_rnd: 0 Read_rnd_next: 1 Sort_merge_passes: 0 Sort_range_count: 0 Sort_rows: 0 Sort_scan_count: 0 Created_tmp_disk_tables: 0 Created_tmp_tables: 0 Start: 2024-05-10T17:53:53.210510Z End: 2024-05-10T17:53:53.210758Z Schema: test_jfg Rows_affected: 0
# Tmp_tables: 0  Tmp_disk_tables: 0  Tmp_table_sizes: 0
# InnoDB_trx_id: 0
# Full_scan: Yes  Full_join: No  Tmp_table: No  Tmp_table_on_disk: No
# Filesort: No  Filesort_on_disk: No  Merge_passes: 0
#   InnoDB_IO_r_ops: 0  InnoDB_IO_r_bytes: 0  InnoDB_IO_r_wait: 0.000000
#   InnoDB_rec_lock_wait: 0.000000  InnoDB_queue_wait: 0.000000
#   InnoDB_pages_distinct: 1
SET timestamp=1715363633;
select * from t;
```


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### MySQL 8.4.0

Defalut (`log_slow_extra = OFF`):
```
# Time: 2024-05-10T19:40:45.931162Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    15
# Query_time: 0.000262  Lock_time: 0.000004 Rows_sent: 0  Rows_examined: 0
SET timestamp=1715370045;
select * from t;
```

With `log_slow_extra = ON`:
```
# Time: 2024-05-10T19:41:28.293662Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    15
# Query_time: 0.000256  Lock_time: 0.000005 Rows_sent: 0  Rows_examined: 0 Thread_id: 15 Errno: 0 Killed: 0 Bytes_received: 22 Bytes_sent: 56 Read_first: 1 Read_last: 0 Read_key: 1 Read_next: 0 Read_prev: 0 Read_rnd: 0 Read_rnd_next: 1 Sort_merge_passes: 0 Sort_range_count: 0 Sort_rows: 0 Sort_scan_count: 0 Created_tmp_disk_tables: 0 Created_tmp_tables: 0 Start: 2024-05-10T19:41:28.293406Z End: 2024-05-10T19:41:28.293662Z
SET timestamp=1715370088;
select * from t;
```


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### MariaDB 11.3.2

Suggested by Arjen Lentz on 
[LinkedIn](https://www.linkedin.com/feed/update/urn:li:activity:7195819671365771264?commentUrn=urn%3Ali%3Acomment%3A%28activity%3A7195819671365771264%2C7195994602758189058%29&dashCommentUrn=urn%3Ali%3Afsd_comment%3A%287195994602758189058%2Curn%3Ali%3Aactivity%3A7195819671365771264%29).

```
# Time: 240515 17:02:39
# User@Host: msandbox[msandbox] @ localhost []
# Thread_id: 8  Schema: test_jfg  QC_hit: No
# Query_time: 0.000166  Lock_time: 0.000069  Rows_sent: 0  Rows_examined: 0
# Rows_affected: 0  Bytes_sent: 65
SET timestamp=1715792559;
select * from t;
```


#### No Db with PS and MariaDB

```
## PS 8.0.36:
# Time: 2024-06-03T19:33:49.052850Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    10
# Schema:   Last_errno: 0  Killed: 0
# Query_time: 0.000057  Lock_time: 0.000000  Rows_sent: 1  Rows_examined: 1  Rows_affected: 0  Bytes_sent: 57
SET timestamp=1717443229;
select database();

## MariaDB 11.3.2:
# Time: 240603 19:37:17
# User@Host: msandbox[msandbox] @ localhost []
# Thread_id: 11  Schema:   QC_hit: No
# Query_time: 0.000074  Lock_time: 0.000000  Rows_sent: 1  Rows_examined: 0
# Rows_affected: 0  Bytes_sent: 66
SET timestamp=1717443437;
select database();
```


#### Weird db name with MySQL, PS and MariaDB

Bug about Invalid "use" with multi-line schema.
- https://bugs.mysql.com/bug.php?id=115526
- https://perconadev.atlassian.net/browse/PS-9298
- https://jira.mariadb.org/browse/MDEV-34539

```
dbdeployer deploy single $version -c slow_query_log_file=slow.log -c slow_query_log=ON

db_name='test
jfg'
./use <<< "create database \`$db_name\`; create table \`$db_name\`.t(id int); insert into \`$db_name\`.t values (1);"
./use "$db_name" <<< "set session long_query_time=0.5; select sleep(1), t.* from t;" > /dev/null

## --== version=mysql_8_4_0:
# Time: 2024-07-04T17:33:27.081804Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    11
# Query_time: 1.004775  Lock_time: 0.000001 Rows_sent: 1  Rows_examined: 1
use test
jfg;
SET timestamp=1720114406;
select sleep(1), t.* from t;

## --== version=ps_8_0_36:
# Time: 2024-07-04T17:28:15.724610Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    10
# Schema: test
jfg  Last_errno: 0  Killed: 0
# Query_time: 1.000422  Lock_time: 0.000003  Rows_sent: 1  Rows_examined: 1  Rows_affected: 0  Bytes_sent: 98
use test
jfg;
SET timestamp=1720114094;
select sleep(1), t.* from t;

## --== version=mariadb_11_3_2:
# Time: 240704 17:29:56
# User@Host: msandbox[msandbox] @ localhost []
# Thread_id: 5  Schema: test
jfg  QC_hit: No
# Query_time: 1.000279  Lock_time: 0.000061  Rows_sent: 1  Rows_examined: 1
# Rows_affected: 0  Bytes_sent: 108
use test
jfg;
SET timestamp=1720114196;
select sleep(1), t.* from t;
```


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Slow Query Log File contains use

Update: I can get rid of the `use ...`, below analysis was wrong, goto next
Update for details.

Because I am adding `Db: ...` to the logs, I thought I could get rid of the
`use ...` line, but this was naive.  Without it, a `use <db>` ends-up as a weird
line in the slow query log file (with `long_query_time = 0`):
```
# Time: 2024-05-13T20:22:39.590264Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    15  Db: test_jfg
# Query_time: 0.000238  Lock_time: 0.000000 Rows_sent: 1  Rows_examined: 1
SET timestamp=1715631759;
SELECT DATABASE();
# Time: 2024-05-13T20:22:39.590633Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    15  Db: test
# Query_time: 0.000070  Lock_time: 0.000000 Rows_sent: 1  Rows_examined: 1
SET timestamp=1715631759;
```

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Normally it would be as below (see `use test_jfg;` below while no `use` in 
above).
```
# Time: 2024-05-13T20:23:11.736782Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    15
# Query_time: 0.000210  Lock_time: 0.000000 Rows_sent: 1  Rows_examined: 1
SET timestamp=1715631791;
SELECT DATABASE();
# Time: 2024-05-13T20:23:11.737096Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    15
# Query_time: 0.000100  Lock_time: 0.000000 Rows_sent: 1  Rows_examined: 1
use test_jfg;
SET timestamp=1715631791;
```

Note: a `use <db>` in the client is below in the general log.
```
2024-05-13T20:17:42.072665Z        14 Init DB   test_jfg
```

This `Init DB` is probably coming from here:
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/sql_parse.cc#L227

Related to above, list of Server Commands:
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/include/my_command.h#L55

Doc on COM_INIT_DB:
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/protocol_classic.cc#L1440

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Update: above was a wrong analysis, I missed that there was another important line
in the logs after the use: `# administrator command: ...`.  Below are complete
slow query log file examples.

With `log_slow_extra_db=on`:
```
# Time: 2024-05-15T18:20:52.787991Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    11  Db: test_jfg1
# Query_time: 0.000150  Lock_time: 0.000000 Rows_sent: 1  Rows_examined: 1
SET timestamp=1715797252;
SELECT DATABASE();
# Time: 2024-05-15T18:20:52.788153Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    11  Db: test_jfg2
# Query_time: 0.000058  Lock_time: 0.000000 Rows_sent: 1  Rows_examined: 1
SET timestamp=1715797252;
# administrator command: Init DB;
# Time: 2024-05-15T18:20:52.789712Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    11  Db: test_jfg2
# Query_time: 0.000644  Lock_time: 0.000005 Rows_sent: 7  Rows_examined: 29
SET timestamp=1715797252;
show databases;
```

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

With `log_slow_extra_db=off`:
```
# Time: 2024-05-15T18:21:48.514700Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    11
# Query_time: 0.000162  Lock_time: 0.000000 Rows_sent: 1  Rows_examined: 1
SET timestamp=1715797308;
SELECT DATABASE();
# Time: 2024-05-15T18:21:48.514842Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    11
# Query_time: 0.000060  Lock_time: 0.000000 Rows_sent: 1  Rows_examined: 1
use test_jfg1;
SET timestamp=1715797308;
# administrator command: Init DB;
# Time: 2024-05-15T18:21:48.517127Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    11
# Query_time: 0.000650  Lock_time: 0.000004 Rows_sent: 7  Rows_examined: 29
SET timestamp=1715797308;
show databases;
```

Out of curiosity, I did some code archeology about this `use ...` in the slow query
log file.  It dates back from at least MySQL 3.23 and Jul 31, 2000 (I cannot date
this exactly as the matching commit is "Import changeset").
- https://github.com/jfg956/mysql-server/blob/f4c589ff6c653d1d2a09c26e46ead3c8a15655d8/sql/log.cc#L547


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Testing:

mtr tests found related to the Slow Query Log:
- [log_slow](https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/mysql-test/t/log_slow.test)
- [log_tables](https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/mysql-test/t/log_tables.test)
- [slow_log](https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/mysql-test/t/slow_log.test) (flaky: failed once, then succeeded many times)
- [sys_vars.slow_query_log_basic](https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/mysql-test/suite/sys_vars/t/slow_query_log_basic.test)
- [sys_vars.slow_query_log_func](https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/mysql-test/suite/sys_vars/t/slow_query_log_func.test)
- [sys_vars.slow_query_log_func_myisam](https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/mysql-test/suite/sys_vars/t/slow_query_log_func_myisam.test)
- [rpl.rpl_slow_query_log](https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/mysql-test/suite/rpl/t/rpl_slow_query_log.test) (flaky: succeeded a few times, failed once, then succeeded)

mtr tests found related to the Slow Query Log *file*:
- [sys_vars.slow_query_log_file_basic](https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/mysql-test/suite/sys_vars/t/slow_query_log_file_basic.test)
- [sys_vars.slow_query_log_file_func](https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/mysql-test/suite/sys_vars/t/slow_query_log_file_func.test)

mtr test found related to things adjacent to this work:
- [sys_vars.log_slow_extra_basic](https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/mysql-test/suite/sys_vars/t/log_slow_extra_basic.test)

mtr tests added for this work:
- [sys_vars.log_slow_extra_db_basic](https://github.com/jfg956/mysql-server/blob/mysql-8.4.0_bug106645/mysql-test/suite/sys_vars/t/log_slow_extra_db_basic.test)
- [sys_vars.log_slow_extra_db_func](https://github.com/jfg956/mysql-server/blob/mysql-8.4.0_bug106645/mysql-test/suite/sys_vars/t/log_slow_extra_db_basic.test)

mtr test added adjacent to this work (it looks like the right thing to do):
- [sys_vars.log_slow_extra_func](https://github.com/jfg956/mysql-server/blob/mysql-8.4.0_bug106645/mysql-test/suite/sys_vars/t/log_slow_extra_db_basic.test)

mtr referencing the slow query log, but not applicable here:
- [persisted_variables_extended](https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/mysql-test/t/persisted_variables_extended.test) (not applicable because no reference to log_slow_extra)


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### Testing script with mtr

```
sql="" # sql: slow query log, naming is sometimes confusing ! :-)
sql="$sql log_slow"
sql="$sql log_tables"
sql="$sql slow_log"
sql="$sql sys_vars.slow_query_log_basic"
sql="$sql sys_vars.slow_query_log_func"
sql="$sql sys_vars.slow_query_log_func_myisam"
sql="$sql "rpl.rpl_slow_query_log

sqlf="" # sqlf: slow query log file.
sqlf="$sqlf sys_vars.slow_query_log_file_basic"
sqlf="$sqlf sys_vars.slow_query_log_file_func"

# lsed: log_slow_extra_db.
lsed="sys_vars.log_slow_extra_db_basic sys_vars.log_slow_extra_db_func"

# lse: log_slow_extra.
lse="sys_vars.log_slow_extra_basic sys_vars.log_slow_extra_func"

./mtr $sql $sqlf $lse $lsed
[...]
==============================================================================
                  TEST NAME                       RESULT  TIME (ms) COMMENT
------------------------------------------------------------------------------
[  6%] rpl.rpl_slow_query_log 'mix'              [ skipped ]  Doesn't support --binlog-format = 'mixed'
[ 12%] rpl.rpl_slow_query_log 'row'              [ skipped ]  Doesn't support --binlog-format = 'row'
[ 18%] rpl.rpl_slow_query_log 'stmt'             [ pass ]  46205
[ 25%] sys_vars.slow_query_log_func_myisam       [ pass ]  13883
[ 31%] sys_vars.slow_query_log_file_func         [ pass ]      1
[ 37%] sys_vars.slow_query_log_file_basic        [ pass ]     30
[ 43%] main.log_tables                           [ pass ]  62093
[ 50%] main.log_slow                             [ pass ]     13
[ 56%] sys_vars.slow_query_log_basic             [ pass ]     44
[ 62%] sys_vars.slow_query_log_func              [ pass ]   6295
[ 68%] sys_vars.log_slow_extra_basic             [ pass ]     20
[ 75%] sys_vars.log_slow_extra_func              [ pass ]   1089
[ 81%] sys_vars.log_slow_extra_db_basic          [ pass ]     89
[ 87%] sys_vars.log_slow_extra_db_func           [ pass ]   1097
[ 93%] main.slow_log                             [ pass ]   2402
[100%] shutdown_report                           [ pass ]
------------------------------------------------------------------------------
[...]
```

Above was done before replacing `NoDb` by `Db: `, after that change, I added a
test for "no selected database".


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### Testing obstacles

- dbdeployer does not work with replication and 8.4 ([Slack](https://mysqlcommunity.slack.com/archives/C06SQ27S26A/p1717097231526739));
- [Bug#115179: Replication Setup Documentation missing SOURCE_SSL=1](https://bugs.mysql.com/bug.php?id=115179);
- [Bug#115187: Doc do not mention replacement for RESET MASTER](https://bugs.mysql.com/bug.php?id=115187);
- "End of list".


#### Testing surprises
- [Bug#115189: P_S Digest table unexpectedly reports created database on replica](https://bugs.mysql.com/bug.php?id=115189);
- [Bug#115203: Empty "use ;" on replica in slow query log file](https://bugs.mysql.com/bug.php?id=115203);
- [Bug#115526: Invalid multi-line "use" in slow query log file with multi-line schema](https://bugs.mysql.com/bug.php?id=115526);
- "End of list".

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Testing manually

This section was added after changing `NoDb` to `Db: `, so no test with `NoDb`.

```
dbdeployer deploy single mysql_8.4.0 -c slow_query_log_file=slow.log -c slow_query_log=ON

./use <<< "create database test_jfg; create table test_jfg.t(id int); insert into test_jfg.t values (1);"
./use test_jfg <<< "set session long_query_time=0.5; select sleep(1), t.* from test_jfg.t;" > /dev/null
./use <<< "set session long_query_time=0.5; select sleep(1), t.* from test_jfg.t;" > /dev/null

cat data/slow.log
/home/jgagne/opt/mysql/mysql_8.4.0/bin/mysqld, Version: 8.4.0 (Source distribution). started with:
Tcp port: 8400  Unix socket: /tmp/mysql_sandbox8400.sock
Time                 Id Command    Argument
# Time: 2024-07-10T15:05:50.899926Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    11  Db: test_jfg
# Query_time: 1.000448  Lock_time: 0.000003 Rows_sent: 1  Rows_examined: 1
SET timestamp=1720623949;
select sleep(1), t.* from test_jfg.t;
# Time: 2024-07-10T15:06:12.386104Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    12  Db: 
# Query_time: 1.000766  Lock_time: 0.000003 Rows_sent: 1  Rows_examined: 1
SET timestamp=1720623971;
select sleep(1), t.* from test_jfg.t;

cat -E data/slow.log
/home/jgagne/opt/mysql/mysql_8.4.0/bin/mysqld, Version: 8.4.0 (Source distribution). started with:$
Tcp port: 8400  Unix socket: /tmp/mysql_sandbox8400.sock$
Time                 Id Command    Argument$
# Time: 2024-07-10T15:05:50.899926Z$
# User@Host: msandbox[msandbox] @ localhost []  Id:    11  Db: test_jfg$
# Query_time: 1.000448  Lock_time: 0.000003 Rows_sent: 1  Rows_examined: 1$
SET timestamp=1720623949;$
select sleep(1), t.* from test_jfg.t;$
# Time: 2024-07-10T15:06:12.386104Z$
# User@Host: msandbox[msandbox] @ localhost []  Id:    12  Db: $
# Query_time: 1.000766  Lock_time: 0.000003 Rows_sent: 1  Rows_examined: 1$
SET timestamp=1720623971;$
select sleep(1), t.* from test_jfg.t;$
```


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### Testing with replication

I could try writting a mtr test inspired on
`rpl.rpl_slow_query_log`, but it would additional work and as there does not look
to be mtr tests in the `rpl*` suites on the Slow Query Log **File**, it would
be a big gap to fill.

```
dbdeployer deploy multiple mysql_8.4.0 -c slow_query_log_file=slow.log -c slow_query_log=ON

./n1 <<< "RESET BINARY LOGS AND GTIDS"
./n1 <<< "CREATE USER 'repl'@'%' IDENTIFIED BY 'password'"
./n1 -u root <<< "GRANT REPLICATION SLAVE ON *.* TO 'repl'@'%'"

port=$(./n1 -N <<< "select @@global.port")
sql="change replication source to  SOURCE_HOST='127.0.0.1', SOURCE_PORT=$port"
sql="$sql, SOURCE_USER='repl', SOURCE_PASSWORD='password', SOURCE_SSL=1"
sql="$sql; start replica"
./n2 <<< "$sql"
./n3 <<< "$sql"

./n1 <<< "create database test_jfg"

./n2 <<< "stop replica; set global long_query_time = 0; start replica"

# Below leads to no log in n2 because log_slow_replica_statements = OFF.
./n1 <<< "create database test_jfg2"

./n2 <<< "stop replica; set global long_query_time = 0, log_slow_replica_statements = ON; start replica"

# Now, interestingly, in n2's log, we have below as "Db: test_jfg3", which is surprising.
# I opened https://bugs.mysql.com/bug.php?id=115189 for this.
./n1 <<< "create database test_jfg3"
# Time: 2024-05-31T18:44:45.949441Z
# User@Host: skip-grants user[] @  []  Id:    28  Db: test_jfg3
# Query_time: 0.025230  Lock_time: 0.000001 Rows_sent: 0  Rows_examined: 0
SET timestamp=1717181085;
create database test_jfg3;

# By checking both primary and replica log for below...
./n1 <<< "set session long_query_time = 0; create database test_jfg4"

# Primary (see "User@Host: msandbox[msandbox] @ localhost []"):
# Time: 2024-05-31T18:46:25.545533Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    23  NoDb
# Query_time: 0.026993  Lock_time: 0.000003 Rows_sent: 0  Rows_examined: 0
SET timestamp=1717181185;
create database test_jfg4;

# Replica (see "User@Host: skip-grants user[] @  []"):
# Time: 2024-05-31T18:46:25.568899Z
# User@Host: skip-grants user[] @  []  Id:    28  Db: test_jfg4
# Query_time: 0.032825  Lock_time: 0.000003 Rows_sent: 0  Rows_examined: 0
SET timestamp=1717181185;
create database test_jfg4;

./n1 test_jfg <<< "set session long_query_time = 0; create table t(id int)"

# Time: 2024-05-31T18:49:13.686657Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    24  Db: test_jfg
# Query_time: 0.105483  Lock_time: 0.000020 Rows_sent: 0  Rows_examined: 0
SET timestamp=1717181353;
create table t(id int);

# Time: 2024-05-31T18:49:13.796389Z
# User@Host: skip-grants user[] @  []  Id:    28  Db: test_jfg
# Query_time: 0.114452  Lock_time: 0.000022 Rows_sent: 0  Rows_examined: 0
SET timestamp=1717181353;
create table t(id int);

# This led me to open below.
# Bug#115189: P_S Digest table unexpectedly reports created database on replica: https://bugs.mysql.com/bug.php?id=115189.

# Below leads to no log in n1 nor n2 because log_slow_admin_statements = OFF.
./n1 test_jfg <<< "set session long_query_time = 0; alter table t add column v int"

./n1 <<< "set global log_slow_admin_statements = ON"
./n2 <<< "stop replica; set global log_slow_admin_statements = ON; start replica"

./n1 test_jfg <<< "set session long_query_time = 0; alter table t add column v2 int"

# Time: 2024-05-31T18:57:03.095269Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    29  Db: test_jfg
# Query_time: 0.060710  Lock_time: 0.000032 Rows_sent: 0  Rows_examined: 0
SET timestamp=1717181823;
alter table t add column v2 int;

# Time: 2024-05-31T18:57:03.163122Z
# User@Host: skip-grants user[] @  []  Id:    43  Db: test_jfg
# Query_time: 0.087499  Lock_time: 0.000032 Rows_sent: 0  Rows_examined: 0
SET timestamp=1717181823;
alter table t add column v2 int;

# In a different db.
./n1 test_jfg2 <<< "set session long_query_time = 0; alter table test_jfg.t add column v3 int"

# Time: 2024-05-31T19:01:48.260787Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    31  Db: test_jfg2
# Query_time: 0.050964  Lock_time: 0.000029 Rows_sent: 0  Rows_examined: 0
SET timestamp=1717182108;
alter table test_jfg.t add column v3 int;

# Time: 2024-05-31T19:01:48.330789Z
# User@Host: skip-grants user[] @  []  Id:    43  Db: test_jfg2
# Query_time: 0.082760  Lock_time: 0.000029 Rows_sent: 0  Rows_examined: 0
SET timestamp=1717182108;
alter table test_jfg.t add column v3 int;

# NoDb expected on replica, but fail !
./n1 <<< "set session long_query_time = 0; create table test_jfg.t2(id int)"

# Time: 2024-05-31T19:56:52.940164Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    32  NoDb
# Query_time: 0.108686  Lock_time: 0.000019 Rows_sent: 0  Rows_examined: 0
SET timestamp=1717185412;
create table test_jfg.t2(id int);

# Time: 2024-05-31T19:56:53.100035Z
# User@Host: skip-grants user[] @  []  Id:    43  Db:
# Query_time: 0.164766  Lock_time: 0.000021 Rows_sent: 0  Rows_examined: 0
SET timestamp=1717185412;
create table test_jfg.t2(id int);

# With fix, starting back from empty db, expected result !
[...]
./n1 <<< "create database test_jfg; create table test_jfg.t(id int)"
./n2 <<< "stop replica; set global long_query_time = 0, log_slow_replica_statements = ON, log_slow_admin_statements = ON; start replica"
./n1 <<< "alter table test_jfg.t add column v int"

# Time: 2024-06-03T19:25:21.968235Z
# User@Host: skip-grants user[] @  []  Id:    27  NoDb
# Query_time: 0.177160  Lock_time: 0.000090 Rows_sent: 0  Rows_examined: 0
SET timestamp=1717442721;
alter table test_jfg.t add column v int;

# But this gives me the idea of removing NoDb and putting "Db: " in the file,
#   not writing anything after "Db: ", which might be nicer.
# Checking Percona Server and MariaDB, it is how this is set.
```


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### Testing mysqldumpslow

Existing test ([mysqldumpslow](https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/mysql-test/t/mysqldumpslow.test)) ok:
```
./mtr mysqldumpslow.test
[...]
==============================================================================
                  TEST NAME                       RESULT  TIME (ms) COMMENT
------------------------------------------------------------------------------
[ 50%] main.mysqldumpslow                        [ pass ]     53
[100%] shutdown_report                           [ pass ]       
------------------------------------------------------------------------------
[...]
```

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Manual test:
```
dbdeployer deploy single mysql_8.4.0 -c slow_query_log_file=slow.log -c slow_query_log=ON

./use <<< "create database test_jfg; create table test_jfg.t(id int); insert into test_jfg.t values (1);"
./use test_jfg <<< "set session long_query_time=0.5; select sleep(1), t.* from test_jfg.t;" > /dev/null

. sb_include && cat data/slow.log && $BASEDIR/bin/mysqldumpslow data/slow.log
/home/jgagne/opt/mysql/mysql_8.4.0/bin/mysqld, Version: 8.4.0 (Source distribution). started with:
Tcp port: 8400  Unix socket: /tmp/mysql_sandbox8400.sock
Time                 Id Command    Argument
# Time: 2024-05-23T18:45:31.526893Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    12  Db: test_jfg
# Query_time: 1.000486  Lock_time: 0.000003 Rows_sent: 1  Rows_examined: 1
SET timestamp=1716489930;
select sleep(1), t.* from test_jfg.t;

Reading mysql slow query log from data/slow.log
Count: 1  Time=1.00s (1s)  Lock=0.00s (0s)  Rows=1.0 (1), msandbox[msandbox]@localhost
  select sleep(N), t.* from test_jfg.t

./use test_jfg <<< "set global log_slow_extra_db=OFF; set session long_query_time=0.5; select sleep(1), t.* from test_jfg.t;"

. sb_include && cat data/slow.log && $BASEDIR/bin/mysqldumpslow data/slow.log
/home/jgagne/opt/mysql/mysql_8.4.0/bin/mysqld, Version: 8.4.0 (Source distribution). started with:
Tcp port: 8400  Unix socket: /tmp/mysql_sandbox8400.sock
Time                 Id Command    Argument
# Time: 2024-05-23T18:45:31.526893Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    12  Db: test_jfg
# Query_time: 1.000486  Lock_time: 0.000003 Rows_sent: 1  Rows_examined: 1
SET timestamp=1716489930;
select sleep(1), t.* from test_jfg.t;
# Time: 2024-05-23T18:48:06.989986Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    13
# Query_time: 1.000381  Lock_time: 0.000003 Rows_sent: 1  Rows_examined: 1
use test_jfg;
SET timestamp=1716490085;
select sleep(1), t.* from test_jfg.t;

Reading mysql slow query log from data/slow.log
Count: 2  Time=1.00s (2s)  Lock=0.00s (0s)  Rows=1.0 (2), msandbox[msandbox]@localhost
  select sleep(N), t.* from test_jfg.t
```

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

This working was expected as extra data at the end of the `User` line is ignored:
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/scripts/mysqldumpslow.pl.in#L104


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### Testing with weird db name

Manual test on 2024-07-04, not working as expected, not yet sure what to do...
Note that this is a larger problem: [Weird db name with MySQL, PS and MariaDB](#weird-db-name-with-mysql-ps-and-mariadb).

Update 2024-07-09: I decided to keep this bug and wait for Oracle to address
[Bug#115526](https://bugs.mysql.com/bug.php?id=115526).  Decision traced in a
comment in the [RFC PR](https://github.com/jfg956/mysql-server/pull/9#issuecomment-2218161395).

```
dbdeployer deploy single mysql_8.4.0 -c slow_query_log_file=slow.log -c slow_query_log=ON

db_name='test
jfg'
./use <<< "create database \`$db_name\`; create table \`$db_name\`.t(id int); insert into \`$db_name\`.t values (1);"
./use "$db_name" <<< "set session long_query_time=0.5; select sleep(1), t.* from t;" > /dev/null
./use "$db_name" <<< "set global log_output = 'TABLE'; set session long_query_time=0.5; select sleep(1), t.* from t;" > /dev/null
./use mysql <<< "select db, sql_text from slow_log"
db      sql_text
test\njfg       select sleep(1), t.* from t
./use mysql <<< "select db, sql_text from slow_log\G"
*************************** 1. row ***************************
      db: test
jfg
sql_text: select sleep(1), t.* from t
cat data/slow.log 
/home/jgagne/opt/mysql/mysql_8.4.0/bin/mysqld, Version: 8.4.0 (Source distribution). started with:
Tcp port: 8400  Unix socket: /tmp/mysql_sandbox8400.sock
Time                 Id Command    Argument
# Time: 2024-07-04T17:10:16.901679Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    11  Db: test
jfg
# Query_time: 1.000402  Lock_time: 0.000004 Rows_sent: 1  Rows_examined: 1
SET timestamp=1720113015;
select sleep(1), t.* from t;
```


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Merge-ability

#### Merge-ability 8.0.37

This work merged on 8.0.37 (tested before replacing `NoDb` by `Db: `).

I did a Fake PR for this: https://github.com/jfg956/mysql-server/pull/10

(I will not do fake PR anymore, I will just test cherry-picking or merging a patch
in detached HEAD)


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### Merge-ability 8.0.38 8.4.1 9.0.0

```
for i in 8.0.38 8.4.1 9.0.0; do
  v=mysql-$i; echo; echo $v
  git worktree add worktrees/${v}_bug106645_merge tags/$v
done

wget https://github.com/jfg956/mysql-server/pull/11.patch -O - > 11.patch
commit=3dc7526ea386f950359d7a4d2e67e69478927dab
( i=8.0.38; v=mysql-$i; echo; echo $v
  cd worktrees/${v}_bug106645_merge
  git cherry-pick --no-commit $commit
); for i in 8.4.1 9.0.0; do
( v=mysql-$i; echo $v
  cd worktrees/${v}_bug106645_merge
  git apply --verbose ../../11.patch
); done
rm 11.patch

for i in 8.0.38 8.4.1 9.0.0; do
( v=mysql-$i; echo; echo $v
  cd worktrees/${v}_bug106645_merge
  p=build/default; mkdir -p $p; cd $p
  test $i == "8.0.38" && cmake ../.. -DWITH_BOOST=~/src/auto-download/boost
  test $i == "8.0.38" || cmake ../..
  cmake --build . -j 6
); done

# variables in below are in section "Testing script with mtr".
for i in 8.0.38 8.4.1 9.0.0; do
( v=mysql-$i; echo; echo $v
  cd worktrees/${v}_bug106645_merge/build/default/mysql-test
  set -u; ./mtr $sql $sqlf $lse $lsed
); done

# Not putting logs of above, but all passed.
```


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Other Notes

While preparing this work, I saw that Percona Server has `Rows_affected` in its
slow query log file (MySQL does not).  I opened a feature request for this,
framing it in such a way that it does not read "please implement this feature
from Percona".  ;-)
- [Bug#114961: Please consider adding fields to log_slow_extra](https://bugs.mysql.com/bug.php?id=114961).

While doing this work, I realized that log_slow_extra put fields in the slow
query log file that are not in the P_S "digest" table, so I opened below:
- [Bug #115372	Please consider adding fields to P_S "Digest" table](https://bugs.mysql.com/bug.php?id=115372).

Interestingly, while doing this work, I discovered the `log-short-format` option:
- https://dev.mysql.com/doc/refman/8.4/en/server-options.html#option_mysqld_log-short-format
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/log.cc#L702
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/mysqld.cc#L12718

Other interesting notes in [Testing obstacles](#testing-obstacles) and
[Testing surprises](#testing-surprises).

Modified files:
```
f="$(echo sql/{{log,mysqld,sys_vars}.cc,mysqld.h})"
f="$f mysql-test/include/slow_query_log_file_reset.inc"
f="$f $(echo mysql-test/suite/sys_vars/{t/log_slow_extra_db_basic.test,r/log_slow_extra_db_basic.result})"
f="$f $(echo mysql-test/suite/sys_vars/{t/log_slow_extra{,_db}_func.test,r/log_slow_extra{,_db}_func.result})"
```


<!-- EOF -->
