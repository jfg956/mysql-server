
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


...


<!-- EOF -->

