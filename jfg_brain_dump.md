
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

I would like to implement (not fix, because it is a feature request)
[Bug#106645](https://bugs.mysql.com/bug.php?id=106645):
Slow query log is not logging database/schema name.

This improvement was suggested to me (JFG) by a colleague (voluntarily
holding details to avoid exposing internal Aiven politics in public GitHub).
While starting to work on this, I found there was a public FR on the subject.

This colleague told me Percona Server has this feature.  I will refrain from
checking PS code, but I will allow myself to check PS behavior to make sure
this change lands in PS without too much complication for Percona.

As mentioned in the public FR
([Bug#106645](https://bugs.mysql.com/bug.php?id=106645)), the `mysql.slow_log`
table already includes a `db` column, so adding it to the slow log file should
not be too complicated a change.  The additional complexity is brought by
making sure it does not break existing tooling parsing the slow log file.  A way
to achieve this is to put this change behind a feature flag / global
variable.  This also allows back-porting this change in 8.0 and 8.4 (with
the default to OFF).  Eventually, this global variable could be deprecated if
the change is considered good for everyone (IMHO, it is).
The name I have in mind for above global variable is
`log_slow_extra_db = { OFF | ON }`.

Note that a global variable to control the output of the slow log is not unheard
of, we already have [log_slow_extra](https://dev.mysql.com/doc/refman/8.0/en/server-system-variables.html#sysvar_log_slow_extra).

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Currently, MySQL logs a line like below in the slow log file.
```
# User@Host: msandbox[msandbox] @ localhost []  Id:    12
```

What I have in mind is, when `log_slow_extra_db` is `ON`, to change above for below
(see `Db: ...` at the end of the line).
```
# User@Host: msandbox[msandbox] @ localhost []  Id:    12  Db: test_jfg
```

I could use `Schema:` instead of `Db:`, but this would not match the content
of the `mysql.slow_log` table, even though it would partially match the content of
`performance_schema.events_statements_summary_by_digest` (`SCHEMA_NAME`).  Not
matching `mysql.slow_log` is not unheard of, `mysql.slow_log.thread_id` is logged
as `Id:`.  So I could change my mind about the name.

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

While preparing this work, I saw that Percona Server has `Rows_affected` in its
slow log file output (MySQL does not).  I opened a feature request for this,
framing it in such a way that it does not read "please implement this feature
from Percona".  ;-)
- [Bug#114961: Please consider adding fields to log_slow_extra](https://bugs.mysql.com/bug.php?id=114961)

TODO: open a FR to add fields to
`performance_schema.events_statements_summary_by_digest`:
- Bytes_received and Bytes_sent;
- Read_first, Read_last, Read_key, Read_next, Read_prev, Read_rnd and Read_rnd_next.


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Doc links

MySQL Slow log:
- 8.0: https://dev.mysql.com/doc/refman/8.0/en/slow-query-log.html
- 8.4: https://dev.mysql.com/doc/refman/8.4/en/slow-query-log.html

Percona Slow Log:
- https://docs.percona.com/percona-server/8.0/slow-extended.html


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Code Notes

TBC...

write_slow:
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/log.cc#L349

File_query_log::write_slow:
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/log.cc#L688

"fill database field":
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/log.cc#L1095

log_slow_extra (Sys_slow_log_extra):
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/sys_vars.cc#L5867

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Slow Query Log Examples

Doing this in 8.0 because I want to compare with Percona Server (PS 8.4.0 is not
out), and in .36 because PS .37 is not out.

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
`Schema: ...` not present in MySQL.
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

With `log_slow_verbosity = full` and `log_slow_extra = OFF`: back at PS style,
with `Schema: ...` back !
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
stlye, with `Schema: ...` gone !
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


<!-- EOF -->
