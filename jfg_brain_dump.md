
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

I would like to implement InnoDB Read IO Tail Latency Monitoring.

When running MySQL on AWS EBS, GCP PV, or other complex network block device
back-end, MySQL performance can degrade if there are increased tail latencies on
read IOs because of an impaired network block device.  In an HA environment, such
degraded instance should quickly be replaced (failover to a replica if impaired
primary, and stop sending reads to it if impaired replica).  However, detecting
such impaired MySQL is not easy.  I want to make this easier by adding "InnoDB
Read IO Tail Latency Monitoring" to MySQL.

The idea is to add a threshold (global variable) and all Read IOs longer than it
(suggested name `innodb_io_read_slow_threshold`) would increment a counter.
This counter could be a global status or an InnoDB Metric.

- https://dev.mysql.com/doc/refman/9.0/en/server-status-variables.html

- https://dev.mysql.com/doc/refman/9.0/en/innodb-information-schema-metrics-table.html

...

TBC: discussion about my implementation:
- ugly carrying info to `buf_read_page` from `os_aio_func` via `innodb_session_t`...
- always timing `os_aio_func`, overhead should be small compared to doing an IO...
- counters in buf instead of os, but could be convinced otherwise...
- SET PERSIST weirdness...
- ...

TBC: add test...
- ...

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Notes on Global Statuses and InnoDB Metrics

Some "interesting" statuses:
- [Innodb_buffer_pool_reads](https://dev.mysql.com/doc/refman/9.0/en/server-status-variables.html#statvar_Innodb_buffer_pool_reads) (this goes to disk)
- [Innodb_buffer_pool_read_requests](https://dev.mysql.com/doc/refman/9.0/en/server-status-variables.html#statvar_Innodb_buffer_pool_read_requests) (this is "logical, might hit cache)
- [Innodb_buffer_pool_write_requests](https://dev.mysql.com/doc/refman/9.0/en/server-status-variables.html#statvar_Innodb_buffer_pool_write_requests) (this is aways logical, the actual write to disk is in flushing)
- [Innodb_buffer_pool_pages_flushed](https://dev.mysql.com/doc/refman/9.0/en/server-status-variables.html#statvar_Innodb_buffer_pool_pages_flushed)

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

### InnoDB Global Statuses and Metrics

`Innodb_buffer_pool_pages_flushed` status is defined / declared / exported / passed here (convoluted, see below):
- All InnoDB Status defined (extern struct): https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/srv0srv.h#L781
- ... defined (struct): https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/srv0srv.h#L1156
- ... declared: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0srv.cc#L531
- ... `innodb_status_variables` array: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/handler/ha_innodb.cc#L1148
- ... ^^ for pluggin: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/handler/ha_innodb.cc#L22142
- ... pluggin: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/handler/ha_innodb.cc#L23630
- `Innodb_buffer_pool_pages_flushed` passed: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0srv.cc#L1613

Arguably, all above is very convoluted.  I guess it is to have a "single" value for
what is both a Global Status and an InnoDB Metric.  We see that all the intricacies
of above ends-up passing a single value `srv_stats.buf_pool_flushed`:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0srv.cc#L1613

Not in scope of this work, the passing of statuses involves taking a lock
(`srv_innodb_monitor_mutex`), it would be interesting to explore the performance
impacts of this (might be minimal as only copying RAM):
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/srv/srv0srv.cc#L1580

Back at `srv_stats.buf_pool_flushed`, it is declared / incremented here:
- declared: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/srv0srv.h#L114
- incremented https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/buf/buf0flu.cc#L1981

So this was a "simple status", without a matching metric, let's explore more
complicated...

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Status `Innodb_buffer_pool_reads` / metric `buffer_pool_reads`:
- (Global Status part of this very similar to above, with backend variable `srv_stats.buf_pool_reads`)
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

...but getting ahead of ourselves rabbitholing in `buf_read_page` and
`buf_read_page_low`, we will come back to that when discussing implementation
details of what we want to do here (monitor slow IOs).

Another place referencing `export_vars.innodb_buffer_pool_reads`:
- reference in `buffer_metrics`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/handler/ha_innodb.cc#L5324
- `buffer_metrics` used in `inno_meter`: https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/handler/ha_innodb.cc#L5386

^^ is also in 8.4, but not in 8.0 (no `inno_meter` in below):
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.41/storage/innobase/handler/ha_innodb.cc

I am guessing this is related to Telemetry:
- https://dev.mysql.com/doc/refman/8.4/en/telemetry.html

I tried OpenTelemetry in Community Edition, but it looks like an Enterprise-only
feature --> bug !
- https://bugs.mysql.com/bug.php?id=117658

And another bug:
- https://bugs.mysql.com/bug.php?id=117659

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

In above, there is no trace of the type / class of InnoDB IO (log, data, ...).
These types are defined here:
- key declarations: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/include/os0file.h#L789
- key definition: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/os/os0file.cc#L292
- PSI_file_info from ^^: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/handler/ha_innodb.cc#L888
- PSI_KEY: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/handler/ha_innodb.cc#L638
- registration of PSI_file_info: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/handler/ha_innodb.cc#L5630

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

(our interest is in the key `innodb_data_file_key` which generates below)

```
performance_schema
file_summary_by_event_name
EVENT_NAME = 'wait/io/file/innodb/innodb_data_file'
```

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

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Exploring above, I open a bug:
- https://bugs.mysql.com/bug.php?id=117625

...

Ugly mixing or read/write acounting (dbl and unexact accounting):
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/include/os0file.ic#L260

...

After looking at P_S, unclear how to hook myself in there...

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

I could use the `innodb_session_t` to carry information between `buf_read_page`
and `os_aio_func`:
- `buf_read_page`: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/buf/buf0rea.cc#L288
- `os_aio_func`: https://github.com/jfg956/mysql-server/blob/mysql-9.2.0/storage/innobase/os/os0file.cc#L6783

--> os_aio_func could indicate how long the IO took, and buf_read_page could interpret that data.

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Tests...

...

```
( cd ~/opt/mysql/mysql_9.2.0/bin
  test -e mysqld_org || cp mysqld{,_org}
  rsync -a ~/src/mysql-server/worktrees/9.2.0_compile/build/default/bin/mysqld ./mysqld_compile
  rsync -a ~/src/mysql-server/worktrees/9.2.0_explo_innodb_read_tail_latencies/build/default/bin/mysqld ./mysqld_explo
  ls -l mysqld_*; )

dbdeployer deploy single mysql_9.2.0

mv=9.2.0
function set_bin() {
  test -e ~/opt/mysql/mysql_$mv/bin/mysqld_$1 || {
    echo "File not found: ~/opt/mysql/mysql_$mv/bin/mysqld_$1"
    return 1
  }

  ( # In a sub-shell to not have to undo cd.
    cd ~/opt/mysql/mysql_$mv/bin
    rm -f mysqld
    ln -s mysqld_$1 mysqld
    ls -l mysqld
  )
}

{
gss="$(echo {count,wait_usec,slow_count,slow_wait_usec})"

gv=innodb_buffer_pool_read_sync_slow_io_threshold_usec
sql1="select count(*) from global_variables where VARIABLE_NAME = '$gv';"

vars2="$(for gs in $gss; do echo -n ",'innodb_buffer_pool_reads_sync_io_$gs'"; done)"
sql2="select count(*) from global_status where VARIABLE_NAME in (${vars2:1});"

vars3="$(for gs in $gss; do echo -n ",'buf_pool_reads_sync_io_$gs'"; done)"
sql3="select count(*) from INNODB_METRICS where NAME in (${vars3:1});"

for bin in org explo; do
  echo; echo $bin; { ./stop; set_bin $bin; ./start; } > /dev/null
  ./use -N performance_schema <<< "$sql1 $sql2"
  ./use -N information_schema <<< "$sql3"
done

sql1="select VARIABLE_NAME, VARIABLE_VALUE from global_variables where VARIABLE_NAME = '$gv';"
sql2="select VARIABLE_NAME, VARIABLE_VALUE from global_status where VARIABLE_NAME in (${vars2:1});"
sql3="select NAME, SUBSYSTEM, COUNT, STATUS, TYPE, COMMENT from INNODB_METRICS where NAME in (${vars3:1});"

echo; echo Values
./use --table performance_schema <<< "$sql1 $sql2"
./use --table information_schema <<< "$sql3"

echo; echo Set Persist
./use <<< "set persist $gv = 0;"
{ ./stop; ./start; } > /dev/null
./use --table performance_schema <<< "$sql2"

echo; echo In conf. File
./use <<< "reset persist $gv;"
echo "$gv = 0" >> my.sandbox.cnf
{ ./stop; ./start; } > /dev/null
./use --table performance_schema <<< "$sql2"
sed -i -e "/$gv/d" my.sandbox.cnf
}

#######################################
### There is a weird behavior in below.
### The SET PERSIST becomes active late in MySQL startup, so initial IOs by InnoDB use the default.
### This is why I added get_server_state ibn buf_read_page in buf0rea.cc.

org
0
0
0

explo
1
4
4

Values
+-----------------------------------------------------+----------------+
| VARIABLE_NAME                                       | VARIABLE_VALUE |
+-----------------------------------------------------+----------------+
| innodb_buffer_pool_read_sync_slow_io_threshold_usec | 3600000000     |
+-----------------------------------------------------+----------------+
+-------------------------------------------------+----------------+
| VARIABLE_NAME                                   | VARIABLE_VALUE |
+-------------------------------------------------+----------------+
| Innodb_buffer_pool_reads_sync_io_count          | 283            |
| Innodb_buffer_pool_reads_sync_io_slow_count     | 0              |
| Innodb_buffer_pool_reads_sync_io_slow_wait_usec | 0              |
| Innodb_buffer_pool_reads_sync_io_wait_usec      | 3017215        |
+-------------------------------------------------+----------------+
[...]

Set Persist
+-------------------------------------------------+----------------+
| VARIABLE_NAME                                   | VARIABLE_VALUE |
+-------------------------------------------------+----------------+
| Innodb_buffer_pool_reads_sync_io_count          | 276            |
| Innodb_buffer_pool_reads_sync_io_slow_count     | 3              |
| Innodb_buffer_pool_reads_sync_io_slow_wait_usec | 31598          |
| Innodb_buffer_pool_reads_sync_io_wait_usec      | 2920372        |
+-------------------------------------------------+----------------+

In conf. File
+-------------------------------------------------+----------------+
| VARIABLE_NAME                                   | VARIABLE_VALUE |
+-------------------------------------------------+----------------+
| Innodb_buffer_pool_reads_sync_io_count          | 274            |
| Innodb_buffer_pool_reads_sync_io_slow_count     | 274            |
| Innodb_buffer_pool_reads_sync_io_slow_wait_usec | 2889765        |
| Innodb_buffer_pool_reads_sync_io_wait_usec      | 2889765        |
+-------------------------------------------------+----------------+


###################
### Below with fix.

Set Persist
+-------------------------------------------------+----------------+
| VARIABLE_NAME                                   | VARIABLE_VALUE |
+-------------------------------------------------+----------------+
| Innodb_buffer_pool_reads_sync_io_count          | 3              |
| Innodb_buffer_pool_reads_sync_io_slow_count     | 3              |
| Innodb_buffer_pool_reads_sync_io_slow_wait_usec | 32142          |
| Innodb_buffer_pool_reads_sync_io_wait_usec      | 32142          |
+-------------------------------------------------+----------------+

In conf. File
+-------------------------------------------------+----------------+
| VARIABLE_NAME                                   | VARIABLE_VALUE |
+-------------------------------------------------+----------------+
| Innodb_buffer_pool_reads_sync_io_count          | 4              |
| Innodb_buffer_pool_reads_sync_io_slow_count     | 4              |
| Innodb_buffer_pool_reads_sync_io_slow_wait_usec | 42548          |
| Innodb_buffer_pool_reads_sync_io_wait_usec      | 42548          |
+-------------------------------------------------+----------------+

...

## ...

echo "innodb_buffer_pool_load_at_startup = 0" >> my.sandbox.cnf

nb_rows=$((3*1024*1024*1024 / (16*1024) * 4))

{
  ./use <<< "
     CREATE DATABASE test_jfg;
     CREATE TABLE test_jfg.t (id INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY)"

 seq 1 $nb_rows |
    awk '{print "(null)"}' |
    tr " " "," | paste -s -d "$(printf ',%.0s' {1..100})\n" |
    sed -e 's/.*/INSERT INTO t values &;/' |
    ./use test_jfg | pv -t

  { echo "ALTER TABLE t ADD COLUMN c0 CHAR(200) DEFAULT ''"
           seq -f " ADD COLUMN c%.0f CHAR(240) DEFAULT ''" 1 15
  } | paste -s -d "," | ./use test_jfg

  ./use test_jfg <<< "ALTER TABLE t FORCE"      | pv -t
  ./use test_jfg <<< "FLUSH TABLE t FOR EXPORT" | pv -t

  ls -lh data/test_jfg/t.ibd
}

# Below, result from above with gp3 and 8.0.41.
0:00:39
0:03:06
0:00:00
-rw-r----- 1 jgagne jgagne 4.7G Mar 12 15:07 data/test_jfg/t.ibd

# Below, result from above with magnetic and 9.2.0.
0:03:20
0:26:24
0:00:00
-rw-r----- 1 jgagne jgagne 4.7G Mar 12 16:13 data/test_jfg/t.ibd



while sleep 0.1; do ./use -N test_jfg <<< "SET @i = ROUND(RAND() * $nb_rows); SELECT * from t where id = @i;"; done


# My stuff...
gss="$(echo {count,wait_usec,slow_count,slow_wait_usec})"
vars3="$(for gs in $gss; do echo -n ",'buf_pool_reads_sync_io_$gs'"; done)"
sql3="select COUNT from INNODB_METRICS where NAME in (${vars3:1});"
while sleep 1; do date; ./use -N information_schema <<< "$sql3"; done |
  stdbuf -oL paste -s -d "    \n" | awk -W interactive '{
    aa=$7-a; bb=$8-b; cc=$9-c; dd=$10-d;
    a =$7;   b =$8;   c =$9;   d =$10;
    NF=6; print $0, aa, bb, cc, dd, bb/aa}' | tail -n +2

# PS...
sql4="select COUNT_STAR, SUM_TIMER_WAIT/1000/1000"
sql4="$sql4 from file_summary_by_event_name where EVENT_NAME = 'wait/io/file/innodb/innodb_data_file'"
while sleep 1; do date; ./use -N performance_schema <<< "$sql4"; done |
  stdbuf -oL paste -s -d " \n" | awk -W interactive '{
    aa=$7-a; bb=$8-b;
    a =$7;   b =$8;
    NF=6; print $0, aa, bb, bb/aa}' | tail -n +2


# I am doing tests on hdd...
./use <<< "set global innodb_buffer_pool_read_sync_slow_io_threshold_usec = 20000"

sudo bash -c "echo 3 > /proc/sys/vm/drop_caches"
pv -etbr data/test_jfg/t.ibd > /dev/null

# Unable to simulate fast IOs in 9.2.0 with ibd file in the Linux Page Cache.
# Same in 8.4.4.
# I am able with 8.0.41...  WTF !

./use <<< "set global innodb_buffer_pool_read_sync_slow_io_threshold_usec = 11000"

Tue Mar 11 20:54:04 UTC 2025 15 158194 1 11041 10546.3
Tue Mar 11 20:54:05 UTC 2025 16 169576 1 11126 10598.5

...
```

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Notes...

...

```
fs="$(echo storage/innobase/{include/{dict0dd,sess0sess,srv0{mon,srv}}.h,{buf/buf0rea,handler/ha_innodb,os/os0file,srv/srv0{mon,srv}}.cc})"

...
```

...

<!-- EOF -->

