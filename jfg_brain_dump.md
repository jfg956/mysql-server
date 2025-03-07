
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
- All InnoDB Status defined (extern struct): https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/srv0srv.h#L781
- ... defined (struct): https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/srv0srv.h#L1156
- ... declared: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0srv.cc#L531
- ... array: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/handler/ha_innodb.cc#L1148
- ... array for pluggin: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/handler/ha_innodb.cc#L22142
- ... pluggin: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/handler/ha_innodb.cc#L23630
- `Innodb_buffer_pool_pages_flushed` passed: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0srv.cc#L1613

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
- ...
- `Innodb_buffer_pool_reads` passed: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0srv.cc#L1615
- from ^^, backend variable `srv_stats.buf_pool_reads`
- declared: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/srv0srv.h#L118
- metric 1: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/srv0mon.h#L171
- metric 2: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0mon.cc#L233
- metric 3 (lot of magic here): https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0mon.cc#L1635
- OVLD meaning: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/srv0mon.h#L126

Where `srv_stats.buf_pool_reads` is incremented:
- `buf_read_ahead_random`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/buf/buf0rea.cc#L284
- `buf_read_page`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/buf/buf0rea.cc#L295
- `buf_read_page_background`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/buf/buf0rea.cc#L317

All of above call `buf_read_page_low`:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/buf/buf0rea.cc#L66

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### Rabit Holing in P_S

Re `buf_read_page_low`:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/buf/buf0rea.cc#L66

^^ calls `fil_io`:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/fil/fil0fil.cc#L7909

^^ calls `Fil_shard::do_io`:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/fil/fil0fil.cc#L7534

^^ calls `os_aio`.  But `os_aio` is complicated:
- with P_S: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/os0file.h#L945
- without: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/os0file.h#L1296

Without PS, `os_aio` calls directly `os_aio_func`, but with, it calls
`pfs_os_aio_func` which is is inlining P_S instrumentation around `os_aio`:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/os0file.ic#L162

These instruments are:
- register_pfs_file_io_begin: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/include/os0file.h#L863
- register_pfs_file_io_end: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/include/os0file.h#L875

`register_pfs_file_io_begin` is rewfering to `file.m_psi`, and we will come back to this...

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

In above, there is no trace of the type / class of InnoDB IO (log, data, ...).
These types are defined here:
- key declarations: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/include/os0file.h#L789
- key definition: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/os/os0file.cc#L292
- PSI_file_info from ^^: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/handler/ha_innodb.cc#L888
- PSI_KEY: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/handler/ha_innodb.cc#L638
- registration of PSI_file_info: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/handler/ha_innodb.cc#L5630

(our interest is in the key `innodb_data_file_key` which generates below)

```
performance_schema
file_summary_by_event_name
EVENT_NAME = 'wait/io/file/innodb/innodb_data_file'
```
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

It looks like the type of IO is saved at file opening time...

Below, example:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/fil/fil0fil.cc#L2938

^^ calls `os_file_create` with `innodb_data_file_key`, which when compiled with P_S, lands here:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/os0file.ic#L116

In ^^, the call to `register_pfs_file_open_end` modifies `file.m_psi`...
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/os0file.h#L841

In ^^, `file` is a `pfs_os_file_t` which is:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/os0file.h#L176 

Also in ^^, `PSI_FILE_CALL` is...
- defined here: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/include/pfs_file_provider.h#L55
- ends-up calling: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/perfschema/pfs.cc#L5549

...

Exploring above, I open a bug:
- https://bugs.mysql.com/bug.php?id=117625

...

Ugly mixing or read/write acounting (dbl and unexact accounting):
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/os0file.ic#L260

...

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

After looking at P_S, unclear bhow to hook myself in there...

Maybe I will have better chances with InnoDB Sessions...

...

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### InnoDB Sessions

`thd_to_innodb_session`:
- https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/handler/ha_innodb.cc#L2011

`class innodb_session_t`:
- https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/include/sess0sess.h#L71

In `innodb_session_t`, there is a `trx_t`:
- https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/include/sess0sess.h#L153
- https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/include/trx0trx.h#L675

...

`check_trx_exists(current_thd)`
- https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/fil/fil0fil.cc#L9635

...

I could use the `innodb_session_t` to carry information beween `buf_read_page`
and `os_aio_func`:
- `buf_read_page`: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/buf/buf0rea.cc#L288
- `os_aio_func`: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/os/os0file.cc#L6783

--> os_aio_func could indicate how long the IO took, and buf_read_page could interpret that data.

...

## Notes...

...

```
fs="$(echo storage/innobase/{buf/buf0rea.cc,handler/ha_innodb.cc,include/{sess0sess.h,srv0{mon,srv}.h},os/os0file.cc,srv/srv0{mon,srv}.cc})"
```

...

<!-- EOF -->

