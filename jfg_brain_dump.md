
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

I would like to implement InnoDB Read IO Tail Latency Monitoring.

When running MySQL on AWS EBS, GCP PV, or other complex network block device
back-end, MySQL performance can degrade if there are increased tail latencies on
read IOs because of an impaired network block device.  In an HA environment, such
degraded MySQL instance would be replaced (failover to a replica if impaired
primary, and stop sending reads to it if impaired replica).  However, detecting
such impaired MySQL is not easy.  I want to make this easier by adding "InnoDB
Read IO Tail Latency Monitoring" to MySQL.

The idea is to add a threshold (global variable) and all Read IOs longer than it
(suggested name `innodb_io_read_slow_threshold`) would increment a counter.
This counter could be a global status or an InnoDB Metric.

- https://dev.mysql.com/doc/refman/9.0/en/server-status-variables.html

- https://dev.mysql.com/doc/refman/9.0/en/innodb-information-schema-metrics-table.html

Some "interesting" statuses:
- [Innodb_buffer_pool_reads](https://dev.mysql.com/doc/refman/9.0/en/server-status-variables.html#statvar_Innodb_buffer_pool_reads) (this goes to disk)
- [Innodb_buffer_pool_read_requests](https://dev.mysql.com/doc/refman/9.0/en/server-status-variables.html#statvar_Innodb_buffer_pool_read_requests) (this is "logical, might hit cache)
- [Innodb_buffer_pool_write_requests](https://dev.mysql.com/doc/refman/9.0/en/server-status-variables.html#statvar_Innodb_buffer_pool_write_requests) (this is aways logical, the actual write to disk is in flushing)
- [Innodb_buffer_pool_pages_flushed](https://dev.mysql.com/doc/refman/9.0/en/server-status-variables.html#statvar_Innodb_buffer_pool_pages_flushed)

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

InnoDB Metric query:
```
select * from INFORMATION_SCHEMA.INNODB_METRICS\G
```

InnoDB Metrics matching above "interesting" statuses:
- buffer_pool_reads (this goes to disk)
- buffer_pool_read_requests (this is "logical, might hit cache)
- buffer_pool_write_requests (this is aways logical, the actual write to disk is in flushing)
- buffer_flush_batch_total_pages (not strictly matching Innodb_buffer_pool_pages_flushed)
- buffer_flush_n_to_flush_requested (idem above, also buffer_flush_n_to_flush_by_dirty_page and buffer_flush_n_to_flush_by_age)

Other interesting InnoDB Metrics (because ms):
- buffer_flush_adaptive_avg_time_slot (Avg time (ms) spent for adaptive flushing recently per slot)
- buffer_LRU_batch_flush_avg_time_slot (Avg time (ms) spent for LRU batch flushing recently per slot)
- buffer_flush_adaptive_avg_time_thread (Avg time (ms) spent for adaptive flushing recently per thread)
- buffer_LRU_batch_flush_avg_time_thread (Avg time (ms) spent for LRU batch flushing recently per thread)
- buffer_flush_adaptive_avg_time_est (Estimated time (ms) spent for adaptive flushing recently)
- buffer_LRU_batch_flush_avg_time_est (Estimated time (ms) spent for LRU batch flushing recently)

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Code Notes

`Innodb_buffer_pool_pages_flushed` status is defined / declared / exported / passed here (convoluted, see below):
- defined (extern struct): https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/srv0srv.h#L781
- defined (struct): https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/srv0srv.h#L1156
- declared: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0srv.cc#L531
- exported (array 1): https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/handler/ha_innodb.cc#L1148
- exported (array 2): https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/handler/ha_innodb.cc#L22142
- exported (pluggin): https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/handler/ha_innodb.cc#L23630
- passed: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0srv.cc#L1613

Arguably, all above is very convoluted.  I guess it is to have a "single" value for
what is both a Global Status and an InnoDB Metric.  We see that all the intricacies
of above ends-up passing a single value `srv_stats.buf_pool_flushed`:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0srv.cc#L1613

Not in scope of this work, the passing of statuses involves taking a lock
(`srv_innodb_monitor_mutex`), it would be interesting to explore the performance
impacts of this:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0srv.cc#L1580

Back at `srv_stats.buf_pool_flushed`, it is declared / incremented here:
- declared: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/srv0srv.h#L114
- incremented https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/buf/buf0flu.cc#L1981

So this was a "simple status", without a matching metric, let's explore more
complicated...

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Status `Innodb_buffer_pool_reads` / metric `buffer_pool_reads`:
- status passed: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0srv.cc#L1615
- from ^^, backend variable `srv_stats.buf_pool_reads`
- declared: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/srv0srv.h#L118
- metric 1: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/srv0mon.h#L171
- metric 2: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0mon.cc#L233
- metric 3 (lot of magic here): https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0mon.cc#L1635

Where `srv_stats.buf_pool_reads` is incremented:
- `buf_read_ahead_random`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/buf/buf0rea.cc#L284
- `buf_read_page`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/buf/buf0rea.cc#L295
- `buf_read_page_background`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/buf/buf0rea.cc#L317

...


<!-- EOF -->

