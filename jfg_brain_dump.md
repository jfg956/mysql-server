
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

I would like to implement a way to easily get these informations:
1. number of bytes written to the binlogs;
2. number of transactions written to the binlogs;
3. average size of trx written to the binlogs;
4. number of trx larger than X bytes written to the binlogs;
5. maybe more...

No 1. and 2. are needs I have for a long time, they have been recently brought back to
my attention with something that happened recently, link to internal Slack below.
- https://aiven-io.slack.com/archives/C01U0HU5UQZ/p1722462974667259?thread_ts=1722461294.026169&cid=C01U0HU5UQZ

No 3. is something I thought about just now, while doing initial reserach on this
subject, link to inspiration below.  This might need an additionnal counter for
bytes written to the binlogs which are for trx, not header, footer, gtid_purged,
...
- https://lefred.be/content/query-and-transaction-size-in-mysql/

No 4. is an "old" wish, being able to hunt "big" trx in the binlogs, the first step
is probably to monitor this, and a counter looks like the right thing to do
(this would also need a global variable to be able to "set" the size at which this
counter is incremented).

No 5. includes other things that might be easy to add to this work, including
counters to monitor parallel information quality, link below.
- https://bugs.mysql.com/bug.php?id=85965


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Note that some binlogs status already exist, links to some below.
- https://dev.mysql.com/doc/refman/8.4/en/server-status-variables.html#statvar_Binlog_cache_use
- https://dev.mysql.com/doc/refman/8.4/en/server-status-variables.html#statvar_Binlog_cache_disk_use

There is some inspiration from MariaDB in this need.  Indeed, MariaDB has two
counters which matches 1. and 2. above: `BINLOG_BYTES_WRITTEN` and
`BINLOG_COMMITS`, links to MariaDB docs below.  I will not check MariaDB code
to do this work as the submission needs to be my original work, but I will allow
myself to read MariaDB docs and test with MariaDB.
- https://mariadb.com/kb/en/replication-and-binary-log-status-variables/#binlog_bytes_written
- https://mariadb.com/kb/en/replication-and-binary-log-status-variables/#binlog_commits

I am just realizing that `BINLOG_COMMITS` might be redundant with the sum of below
but it would still be good to have a "unified" counter on this.
- https://dev.mysql.com/doc/refman/8.4/en/server-status-variables.html#statvar_Binlog_cache_disk_use
- https://dev.mysql.com/doc/refman/8.4/en/server-status-variables.html#statvar_Binlog_cache_use
- https://dev.mysql.com/doc/refman/8.4/en/server-status-variables.html#statvar_Binlog_stmt_cache_disk_use
- https://dev.mysql.com/doc/refman/8.4/en/server-status-variables.html#statvar_Binlog_stmt_cache_use


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

While researching this, and thanks to
[LeFred post](https://lefred.be/content/query-and-transaction-size-in-mysql/),
I learned that there is a table in Performance Schema,
[`binary_log_transaction_compression_stats`](https://dev.mysql.com/doc/refman/8.0/en/performance-schema-binary-log-transaction-compression-stats-table.html),
which contains some
of the things I want.  However, needing a dedicated query to get these, in addition
to querying global statuses is sub-optimal monitoring, hence still thinking we
should have global statues for this.  But as this table exists, we can get
inspiration from the code used to populate it to implement this work.

I asked DBA friends about initial feedback on this work, link below.
- https://dbachat.slack.com/archives/C027R4PCV/p1722628897982989


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

<a name="p_s_crash_bltcs"></a>

(Direct link to here: [link](#p_s_crash_bltcs))

It was brought to my attention that it is possible to crash MySQL (at least 8.0.39,
8.4.2 and 9.0.1) by querying `p_s.binary_log_transaction_compression_stats`, more
about this in below messages in DBA Slack (not public).
- https://dbachat.slack.com/archives/D0479SMDS4Q/p1722966419772569
- https://dbachat.slack.com/archives/C027R4PCV/p1722967438113609?thread_ts=1722628897.982989&cid=C027R4PCV

Another query to get the number of bytes written to the binlogs is below, but
unclear if this is "efficient".  Discussed in DBA Slack (not public).
- https://dbachat.slack.com/archives/C027R4PCV/p1722969837544739?thread_ts=1722628897.982989&cid=C027R4PCV
```
select sum(SUM_NUMBER_OF_BYTES_WRITE)
  from performance_schema.file_summary_by_instance
  where event_name='wait/io/file/sql/binlog';
```

According to below, there is an index in above on `EVENT_NAME`.
- https://dev.mysql.com/doc/refman/8.0/en/performance-schema-file-summary-tables.html

But below shows the result is not strictly-increasing, so un-usable
(purging a 1 kb binlog while writing 2 kb will show 1 kb written):
```
t="from performance_schema.file_summary_by_instance"
w="where event_name='wait/io/file/sql/binlog'"
q1="select FILE_NAME $t $w"
q2="select sum(SUM_NUMBER_OF_BYTES_WRITE) $t $w"
./use -N <<< "
  $q1; $q2;
  flush binary logs;
  $q1; $q2;
  do sleep(1); purge binary logs BEFORE now();
  $q1; $q2;"
~/sandboxes/msb_9_0_1/data/binlog.000001
8336
~/sandboxes/msb_9_0_1/data/binlog.000001
~/sandboxes/msb_9_0_1/data/binlog.000002
8539
~/sandboxes/msb_9_0_1/data/binlog.000002
158
```

Realized after code exploration, the content of
`binary_log_transaction_compression_stats` does not match
`file_summary_by_instance`...

```
q1="select UNCOMPRESSED_BYTES_COUNTER from performance_schema.binary_log_transaction_compression_stats"
q2="select sum(SUM_NUMBER_OF_BYTES_WRITE) from performance_schema.file_summary_by_instance where event_name='wait/io/file/sql/binlog'"

dbdeployer deploy single 9.0.1

# We would expect the 2 numbers to be the same !
./use -N <<< "$q1; $q2"
5483
8332

# We would expect both delta to increase by the same amount.
./use -N <<< "
    $q1; $q2;
    drop database if exists test_jfg;
    create database test_jfg;
    $q1; $q2"
7503
11729
7741
12129

# In above, add below to compute the delta.
...  | paste -s -d " \n" - | awk '{print $1-a, $2-b;a=$1;b=$2}' | tail -n 1
238 400

. sb_include 
$BASEDIR/bin/mysqlbinlog data/binlog.000001 
[...]
# at 12129
#240807 15:55:39 server id 1  end_log_pos 12206 CRC32 0x9d6ffbf0 	Anonymous_GTID	last_committed=52	sequence_number=53	rbr_only=no	original_committed_timestamp=1723060539853758	immediate_commit_timestamp=1723060539853758	transaction_length=203
# original_commit_timestamp=1723060539853758 (2024-08-07 15:55:39.853758 EDT)
# immediate_commit_timestamp=1723060539853758 (2024-08-07 15:55:39.853758 EDT)
/*!80001 SET @@session.original_commit_timestamp=1723060539853758*//*!*/;
/*!80014 SET @@session.original_server_version=90001*//*!*/;
/*!80014 SET @@session.immediate_server_version=90001*//*!*/;
SET @@SESSION.GTID_NEXT= 'ANONYMOUS'/*!*/;
# at 12206
#240807 15:55:39 server id 1  end_log_pos 12332 CRC32 0x4ae0ef23 	Query	thread_id=22	exec_time=0	error_code=0	Xid = 156
SET TIMESTAMP=1723060539/*!*/;
drop database if exists test_jfg
/*!*/;
# at 12332
#240807 15:55:39 server id 1  end_log_pos 12409 CRC32 0x75271658 	Anonymous_GTID	last_committed=53	sequence_number=54	rbr_only=no	original_committed_timestamp=1723060539854542	immediate_commit_timestamp=1723060539854542	transaction_length=197
# original_commit_timestamp=1723060539854542 (2024-08-07 15:55:39.854542 EDT)
# immediate_commit_timestamp=1723060539854542 (2024-08-07 15:55:39.854542 EDT)
/*!80001 SET @@session.original_commit_timestamp=1723060539854542*//*!*/;
/*!80014 SET @@session.original_server_version=90001*//*!*/;
/*!80014 SET @@session.immediate_server_version=90001*//*!*/;
SET @@SESSION.GTID_NEXT= 'ANONYMOUS'/*!*/;
# at 12409
#240807 15:55:39 server id 1  end_log_pos 12529 CRC32 0xbb6d178b 	Query	thread_id=22	exec_time=0	error_code=0	Xid = 158
SET TIMESTAMP=1723060539/*!*/;
/*!80016 SET @@session.default_table_encryption=0*//*!*/;
create database test_jfg
[...]

# In above
#   12529-12129=400 --> total trx length
#   12206-12129=77  --> Anonymous_GTID length
#   400-77-77=246   --> not matching 238, 8 bytes missing ! !

...
```

Also, off by one error...
```
q3="select FILE_NAME,SUM_NUMBER_OF_BYTES_WRITE from performance_schema.file_summary_by_instance where event_name='wait/io/file/sql/binlog'"
./use -N <<< "flush binary logs; $q3"; ls -l data/binlog.0*
/Users/jf.gagne/sandboxes/msb_9_0_1/data/binlog.000001	12574
/Users/jf.gagne/sandboxes/msb_9_0_1/data/binlog.000002	203
/Users/jf.gagne/sandboxes/msb_9_0_1/data/binlog.000003	203
/Users/jf.gagne/sandboxes/msb_9_0_1/data/binlog.000004	203
/Users/jf.gagne/sandboxes/msb_9_0_1/data/binlog.000005	158
-rw-r-----  1 jf.gagne  staff  12573  7 Aug 16:08 data/binlog.000001
-rw-r-----  1 jf.gagne  staff    202  7 Aug 16:11 data/binlog.000002
-rw-r-----  1 jf.gagne  staff    202  7 Aug 16:12 data/binlog.000003
-rw-r-----  1 jf.gagne  staff    202  7 Aug 16:14 data/binlog.000004
-rw-r-----  1 jf.gagne  staff    158  7 Aug 16:14 data/binlog.000005

...
```

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Code Notes

The code for the `p_s` table `binary_log_transaction_compression_stats` is in the
files `storage/perfschema/table_binary_log_transaction_compression_stats.{h,cc}`:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/perfschema/table_binary_log_transaction_compression_stats.h
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/perfschema/table_binary_log_transaction_compression_stats.cc

In `.cc` file above, we have `m_rows` as a `struct st_binary_log_transaction_compression_stats`...
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/perfschema/table_binary_log_transaction_compression_stats.cc#L76

This `struct` contains a vector...
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/perfschema/table_binary_log_transaction_compression_stats.cc#L50

This vector is kept in sync via, `update` and `reset` functions, with
`binlog::global_context.monitoring_context().transaction_compression()`...
- `update`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/perfschema/table_binary_log_transaction_compression_stats.cc#L57
- `reset`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/perfschema/table_binary_log_transaction_compression_stats.cc#L66

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

`binlog::global_context` is declared / defined there:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/global.h#L53
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/global.cc#L35

`binlog::global_context.monitoring_context()` is declared / defined there:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/global.h#L50
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/global.cc#L41

The return-type of above (`monitoring::Context`) is declared there:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.h#L292

And `binlog::global_context.monitoring_context().transaction_compression()` is
declared / defined there:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.h#L303
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.cc#L320

The return type of above (`Transaction_compression`) is declared / defined there:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.h#L211
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.cc#L232

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Back at the vector in the p_s table which is sync-ed, these are the two sync
functions:
- `update` calls `get_stats`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.cc#L300
- `reset` calls `reset`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.cc#L270

The way `Transaction_compression` is updated after log writting (binlog or relay
log) is via the `update` method:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.cc#L275

Above method is called here:
- `BINARY`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog.cc#L1764
- `RELAY`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog.cc#L5953

So we could hook ourselved in `BINARY` above to increment the new statuses.

Or instead of calling `update` of `binlog::global_context.monitoring_context().transaction_compression()`
we could create `update_binary` and `update_relay` in `binlog::global_context.monitoring_context()`,
which would "indirect" the calls for `BINARY` and `RELAY` above, and the new
statuses would be incremented in `update_binary`.

...


<!-- EOF -->

