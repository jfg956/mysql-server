<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

We would like to implement (not fix, because it is a feature request)
[Bug#106645](https://bugs.mysql.com/bug.php?id=106645):
Slow query log is not logging database/schema name.

...

write_slow:
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/log.cc#L349

File_query_log::write_slow:
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/log.cc#L688

"fill database field":
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/log.cc#L1095

...

#### MySQL 8.0.36 slow log:

Defalut (`log_slow_extra = OFF`):
```
# Time: 2024-05-09T20:18:29.041949Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    10
# Query_time: 0.000245  Lock_time: 0.000004 Rows_sent: 1  Rows_examined: 1
SET timestamp=1715285909;
select * from t;
```

With `log_slow_extra = ON`:
```
# Time: 2024-05-09T20:29:14.041492Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    10
# Query_time: 0.000240  Lock_time: 0.000004 Rows_sent: 1  Rows_examined: 1 Thread_id: 10 Errno: 0 Killed: 0 Bytes_received: 22 Bytes_sent: 62 Read_first: 1 Read_last: 0 Read_key: 1 Read_next: 0 Read_prev: 0 Read_rnd: 0 Read_rnd_next: 2 Sort_merge_passes: 0 Sort_range_count: 0 Sort_rows: 0 Sort_scan_count: 0 Created_tmp_disk_tables: 0 Created_tmp_tables: 0 Start: 2024-05-09T20:29:14.041252Z End: 2024-05-09T20:29:14.041492Z
SET timestamp=1715286554;
select * from t;
```

#### PS 8.0.36 slow log

Default (see additional line with `Schema: ...`):
```
# Time: 2024-05-09T20:19:03.921749Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    10
# Schema: test_jfg  Last_errno: 0  Killed: 0
# Query_time: 0.000245  Lock_time: 0.000004  Rows_sent: 0  Rows_examined: 0  Rows_affected: 0  Bytes_sent: 56
SET timestamp=1715285943;
select * from t;
```

With `log_slow_verbosity = full`:
```
# Time: 2024-05-09T20:34:11.698019Z
# User@Host: msandbox[msandbox] @ localhost []  Id:    13
# Schema: test_jfg  Last_errno: 0  Killed: 0
# Query_time: 0.000268  Lock_time: 0.000004  Rows_sent: 0  Rows_examined: 0  Rows_affected: 0  Bytes_sent: 56
# Tmp_tables: 0  Tmp_disk_tables: 0  Tmp_table_sizes: 0
# InnoDB_trx_id: 0
# Full_scan: Yes  Full_join: No  Tmp_table: No  Tmp_table_on_disk: No
# Filesort: No  Filesort_on_disk: No  Merge_passes: 0
#   InnoDB_IO_r_ops: 0  InnoDB_IO_r_bytes: 0  InnoDB_IO_r_wait: 0.000000
#   InnoDB_rec_lock_wait: 0.000000  InnoDB_queue_wait: 0.000000
#   InnoDB_pages_distinct: 1
SET timestamp=1715286851;
select * from t;
```

...
