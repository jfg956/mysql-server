
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

I would like to implement a way to easily get these informations:
1. number of bytes written to the binlogs;
2. number of transactions written to the binlogs;
3. average size of trx written to the binlogs;
4. number of trx larger than X bytes written to the binlogs;
5. maybe more...

1. and 2. are needs I have for a long time, they have been recently brought back to
my attention with something that happened recently, link to internal Slack below.
- https://aiven-io.slack.com/archives/C01U0HU5UQZ/p1722462974667259?thread_ts=1722461294.026169&cid=C01U0HU5UQZ

3. is something I thought about just now, while doing initial reserach on this
subject, link to inspiration below.  This might need an additionnal counter for
bytes written to the binlogs which are for trx, not header, footer, gtid_purged,
...
- https://lefred.be/content/query-and-transaction-size-in-mysql/

4. is an "old" wish, being able to hunt "big" trx in the binlogs, the first step is
probably to monitor this, and a counter looks like the right thing to do
(this would also need a global variable to be able to "set" the size at which this
counter is incremented).

5. includes other things that might be easy to add to this work, including counters
to monitor parallel information quality, link below.
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

While researching this, and thanks to LeFred post (link below), I learned that
there is a `p_s.binary_log_transaction_compression_stats` table which contains some
of the things I want.  However, needing a dedicated query to get these, in addition
to querying global statuses is sub-optimal monitoring, hence still thinking we
should have global statues for this.  But as this table exists, we can get
inspiration from the code used to populate it to implement this work.
- https://lefred.be/content/query-and-transaction-size-in-mysql/

I asked DBA friends about initial feedback on this work, link below.
- https://dbachat.slack.com/archives/C027R4PCV/p1722628897982989


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

It was brought to my attention that it is possible to crash MySQL (at least 8.0.39,
8.4.2 and 9.0.1) by querying `p_s.binary_log_transaction_compression_stats`, more
about this in below messages in a DBA Slack.
- https://dbachat.slack.com/archives/D0479SMDS4Q/p1722966419772569
- https://dbachat.slack.com/archives/C027R4PCV/p1722967438113609?thread_ts=1722628897.982989&cid=C027R4PCV

Another query to get the number of bytes written to the binlogs is below, but
unclear if this is "efficient".  Discussed in DBA Slack.
- https://dbachat.slack.com/archives/C027R4PCV/p1722969837544739?thread_ts=1722628897.982989&cid=C027R4PCV
```
select sum(SUM_NUMBER_OF_BYTES_WRITE)
  from performance_schema.file_summary_by_instance
  where event_name='wait/io/file/sql/binlog';
```

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Code Notes

The code for the `p_s` table `binary_log_transaction_compression_stats` is in the
files `storage/perfschema/table_binary_log_transaction_compression_stats.{h,cc}`:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/perfschema/table_binary_log_transaction_compression_stats.h
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/perfschema/table_binary_log_transaction_compression_stats.cc

In `.cc` file above, we have a `struct st_binary_log_transaction_compression_stats m_rows`...
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/perfschema/table_binary_log_transaction_compression_stats.cc#L76

This `struct` contains a vector...
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/perfschema/table_binary_log_transaction_compression_stats.cc#L50C8-L50C14

This vector is kept in sync via `update` and `reset` functions with
`binlog::global_context.monitoring_context().transaction_compression()`...
- `update`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/perfschema/table_binary_log_transaction_compression_stats.cc#L57
- `reset`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/perfschema/table_binary_log_transaction_compression_stats.cc#L66

`binlog::global_context` is declared / defined there:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/global.h#L53
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/global.cc#L35

`binlog::global_context.monitoring_context()` is declared / defined there:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/global.h#L50
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/global.cc#L41

The return type of above (`monitoring::Context`) is declared there:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.h#L292

And `binlog::global_context.monitoring_context().transaction_compression()` is declared / defined there:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.h#L303
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.cc#L320

The return type of above (`Transaction_compression`) is declared / defined there:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.h#L211
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.cc#L232

Back at the vector in the p_s table which is sync-ed, these are the two sync
functions:
- `update`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.cc#L300
- `reset`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.cc#L270

The way `Transaction_compression` is updated is via the `update` method:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog/monitoring/context.cc#L275

Above method is called here:
- `BINARY`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog.cc#L1764
- `RELAY`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/sql/binlog.cc#L5953

So we could hook ourselved in `BINARY` above to increment the new status.

...


<!-- EOF -->

