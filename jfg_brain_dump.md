
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
- [Innodb_buffer_pool_reads](https://dev.mysql.com/doc/refman/9.0/en/server-status-variables.html#statvar_Innodb_buffer_pool_reads)
- [Innodb_buffer_pool_read_requests](https://dev.mysql.com/doc/refman/9.0/en/server-status-variables.html#statvar_Innodb_buffer_pool_read_requests)
- [Innodb_buffer_pool_write_requests](https://dev.mysql.com/doc/refman/9.0/en/server-status-variables.html#statvar_Innodb_buffer_pool_write_requests)
- [Innodb_buffer_pool_pages_flushed](https://dev.mysql.com/doc/refman/9.0/en/server-status-variables.html#statvar_Innodb_buffer_pool_pages_flushed)

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

InnoDB Metric query:
```
select * from INFORMATION_SCHEMA.INNODB_METRICS\G
```

InnoDB Metrics matching above "interesting" statuses:
- buffer_pool_reads
- buffer_pool_read_requests
- buffer_pool_write_requests
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

...


<!-- EOF -->

