
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

We would like to fix [Bug#113598](https://bugs.mysql.com/bug.php?id=113598):
Please consider adding GTID in "Timeout waiting for reply".

The relevant lines of code are:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source.cc#L792
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/share/messages_to_error_log.txt#L3518

This will be a heavier lift than it could initially seem, because the
[`ReplSemiSyncMaster::commitTrx`](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source.cc#L635)
function only has `trx_wait_binlog_name` and `trx_wait_binlog_pos` as arguments,
no GTID information.

Thinking about this a little more, the fact that there is no GTID reference in
the semi-sync code probably comes from the history of the plugin.
[The plugin was introduced in MySQL 5.5.7](https://github.com/jfg956/mysql-server/tree/mysql-5.5.7/plugin/semisync)
and GTIDs are a feature of MySQL 5.6.

More information about the analysis of the change needed for fixing Bug#113598
is available in the section [Semi-Sync Plugin Notes](#semi-sync-plugin-notes).


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### General plugin notes:

This is my first encounter with the code side of the plugin architecture...

There is a "Plugin library descriptor" (`mysql_declare_plugin`):
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc#L705
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_replica_plugin.cc#L337

In above, Init, Check uninstall, and Deinit functions are specified.

Also in above, status and system variables are specified.


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### Semi-Sync Plugin Notes

In the [Init function of the primary](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc#L580),
there is the registration of trans_observer, storage_observer and transmit_observer:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc#L650

One of these observers is [`repl_semi_report_commit`](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc#L109),
in which `ReplSemiSyncMaster:commitTrx` is called.

Update: below was a mistake, goto "Update" to skip.

If we are able to get a GTID from the parameter of `repl_semi_report_commit`
(`Trans_param *param`), we will be able to push this information to `commitTrx`.
More about this in the section [Trans_param](#trans_param)

But `ReplSemiSyncMaster:commitTrx` is also called in
[`repl_semi_report_binlog_sync`](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc#L95)
where the transaction information is not available.  This code-path is used for
`WAIT_AFTER_SYNC` ([rpl_semi_sync_master_wait_point](https://dev.mysql.com/doc/refman/8.0/en/replication-options-source.html#sysvar_rpl_semi_sync_master_wait_point)),
which is not the lossless semi-sync of 5.7.  These days, almost
everyone should be using `WAIT_AFTER_COMMIT`
(more about the difference in [Question about Semi-Synchronous Replication: the Answer with All the Details](https://percona.community/blog/2018/08/23/question-about-semi-synchronous-replication-answer-with-all-the-details/)).
We could could compromise in fixing Bug#113598 only for lossless semi-sync.
Or if we want the GTID also for the legacy semi-sync (`WAIT_AFTER_SYNC`), we could 
save the GTID in the `repl_semi_report_commit` function for usage in
`repl_semi_report_binlog_sync`.

Update: above was a mistake.  The `repl_semi_report_commit` observer is for 5.5
semi-sync, and the lossless semi-sync of 5.7 uses the
`repl_semi_report_binlog_sync` observer (I confused `WAIT_AFTER_COMMIT` and
`WAIT_AFTER_SYNC`, probably hoping things would be simple and I would only have to
reference `Trans_gtid_info` from `Trans_param` to fix this).  I realized this
doing tests and need to pivot.

When checking `enum_gtid_type`, I saw that once a trx is assigned a GTID, this
is indicated in `thd->variables.gtid_next`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/rpl_gtid.h#L3707

Reference to `thd->variables`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/sql_class.h#L1116

Reference to `thd->variables.gtid_next`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/system_variables.h#L355

So we could use `current_thd` to get the GTID of the trx:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc#L591

But when testing this, `current_thd->variables.gtid_next` is `AUTOMATIC`, arg !

Starting back on this after almost a month...

The last tests were on using `thd->variables.gtid_next`to get the gtid, but
these showed this was still `AUTOMATIC` / not updated when calling 
`repl_semi_report_binlog_sync`.  This lead to digging how GTIDs are assigned,
details in the section [GTID Assignment](#gtid-assignment).  The TL&DR is that
at this point in the code, assignment has been done, but not in
`current_thd->variables.gtid_next`, only temporarily in
[`current_thd->owned_gtid`][owned_gtid].

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### Trans_param

`Trans_param` definition:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/replication.h#L132

It contains a `Trans_gtid_info`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/replication.h#L151

`Trans_gtid_info` definition:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/replication.h#L121

This definition is not super clear.  There is a sid, a type, a sidno and a gno.

`sid` (type `rpl_sid`) is the UUID of the gtid:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/rpl_gtid.h#L293

`sid` means Source Id:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/rpl_gtid.h#L99

`type` indicates ANONYMOUS, ...
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/rpl_gtid.h#L3689

`sidno` is nto fully understood

`gno` is the numeric part of the gtid


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### GTID Assignment

In full release binaries (full as opposed to minimal), ibelow is the stack trace
when we get in the semi-sync code.

```
Thread 49 "connection" hit Breakpoint 1.1, repl_semi_report_binlog_sync () at ../../../mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc:98
98      ../../../mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc: No such file or directory.
(gdb) backtrace
#0  repl_semi_report_binlog_sync () at ../../../mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc:98
#1  0x0000000001de8d72 in Binlog_storage_delegate::after_sync () at ../../mysql-8.2.0/sql/rpl_handler.cc:1035
#2  0x0000000001d6ab99 in call_after_sync_hook () at ../../mysql-8.2.0/sql/binlog.cc:8858
#3  0x0000000001d86908 in MYSQL_BIN_LOG::ordered_commit () at ../../mysql-8.2.0/sql/binlog.cc:9137
#4  0x0000000001d87f1a in MYSQL_BIN_LOG::commit () at ../../mysql-8.2.0/sql/binlog.cc:8394
#5  0x0000000001106c56 in ha_commit_trans () at ../../mysql-8.2.0/sql/handler.cc:1794
#6  0x0000000000fa51fb in trans_commit () at ../../mysql-8.2.0/sql/transaction.cc:246
#7  0x0000000000e3136c in mysql_create_db () at ../../mysql-8.2.0/sql/sql_db.cc:504
#8  0x0000000000e83520 in mysql_execute_command () at ../../mysql-8.2.0/sql/sql_parse.cc:3943
#9  0x0000000000e84ed0 in dispatch_sql_command () at ../../mysql-8.2.0/sql/sql_parse.cc:5479
#10 0x0000000000e87b05 in dispatch_command () at ../../mysql-8.2.0/sql/sql_parse.cc:2136
#11 0x0000000000e886c7 in do_command () at ../../mysql-8.2.0/sql/sql_parse.cc:1465
#12 0x0000000000fe4cb8 in handle_connection () at ../../mysql-8.2.0/sql/conn_handler/connection_handler_per_thread.cc:303
#13 0x000000000282bfa5 in pfs_spawn_thread () at ../../../mysql-8.2.0/storage/perfschema/pfs.cc:3049
#14 0x00007ffff72a8134 in start_thread (arg=<optimized out>) at ./nptl/pthread_create.c:442
#15 0x00007ffff73287dc in clone3 () at ../sysdeps/unix/sysv/linux/x86_64/clone3.S:81
```

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
Interestinglky, `repl_semi_report_binlog_sync` "breaks" twice.  I first parked
this, but I later correlated this with the release binaries not working super well
with gdb.  I also got problem with the debug binaries that SIGSEGV in pluggin code
(maybe because the .so was compiled in release, even though my confidence level on
this is low).

Because problems above, pivoting to debug binaries without semi-sync loaded with
breaking before semi-sync is called (`call_after_sync_hook`).

```
Thread 48 "connection" hit Breakpoint 2, call_after_sync_hook (queue_head=queue_head@entry=0x7fff30001050) at ../../mysql-8.2.0/sql/binlog.cc:8846
8846    in ../../mysql-8.2.0/sql/binlog.cc
(gdb) backtrace
#0  call_after_sync_hook (queue_head=queue_head@entry=0x7fff30001050) at ../../mysql-8.2.0/sql/binlog.cc:8846
#1  0x0000000003dcbc49 in MYSQL_BIN_LOG::ordered_commit (this=this@entry=0x6eb2040 <mysql_bin_log>, thd=0x7fff30001050, all=all@entry=true, skip_commit=skip_commit@entry=false) at ../../mysql-8.2.0/sql/binlog.cc:9137
#2  0x0000000003dcd471 in MYSQL_BIN_LOG::commit (this=<optimized out>, thd=<optimized out>, all=<optimized out>) at ../../mysql-8.2.0/sql/binlog.cc:8394
#3  0x0000000003465bdb in ha_commit_trans (thd=thd@entry=0x7fff30001050, all=all@entry=true, ignore_global_read_lock=ignore_global_read_lock@entry=false) at ../../mysql-8.2.0/sql/handler.cc:1794
#4  0x00000000032ef0b4 in trans_commit (thd=thd@entry=0x7fff30001050, ignore_global_read_lock=ignore_global_read_lock@entry=false) at ../../mysql-8.2.0/sql/transaction.cc:246
#5  0x00000000031795e4 in mysql_rm_db (thd=thd@entry=0x7fff30001050, db=..., if_exists=if_exists@entry=false) at ../../mysql-8.2.0/sql/sql_db.cc:859
#6  0x00000000031cf5c6 in mysql_execute_command (thd=thd@entry=0x7fff30001050, first_level=first_level@entry=true) at ../../mysql-8.2.0/sql/sql_parse.cc:3954
#7  0x00000000031d284f in dispatch_sql_command (thd=0x7fff30001050, parser_state=parser_state@entry=0x7fffe014acf0) at ../../mysql-8.2.0/sql/sql_parse.cc:5479
#8  0x00000000031d3f63 in dispatch_command (thd=<optimized out>, thd@entry=0x7fff30001050, com_data=com_data@entry=0x7fffe014bb20, command=COM_QUERY) at ../../mysql-8.2.0/sql/sql_parse.cc:2136
#9  0x00000000031d5c7b in do_command (thd=thd@entry=0x7fff30001050) at ../../mysql-8.2.0/sql/sql_parse.cc:1465
#10 0x000000000333c9b6 in handle_connection (arg=arg@entry=0x9a918a0) at ../../mysql-8.2.0/sql/conn_handler/connection_handler_per_thread.cc:303
#11 0x00000000048a7ae8 in pfs_spawn_thread (arg=0x9b4afe0) at ../../../mysql-8.2.0/storage/perfschema/pfs.cc:3049
#12 0x00007ffff72a8134 in start_thread (arg=<optimized out>) at ./nptl/pthread_create.c:442
#13 0x00007ffff73287dc in clone3 () at ../sysdeps/unix/sysv/linux/x86_64/clone3.S:81
```
 
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
This is what I gathered from exploring above:
- before calling [`call_after_sync_hook`][call_after_sync_hook], `ordered_commit` calls [`process_flush_stage_queue`][process_flush_stage_queue],

- [`process_flush_stage_queue`][process_flush_stage_queue] calls [`assign_automatic_gtids_to_flush_group`][assign_automatic_gtids_to_flush_group],

- [`assign_automatic_gtids_to_flush_group`][assign_automatic_gtids_to_flush_group] calls `generate_automatic_gtid`,

- `generate_automatic_gtid` calls [`acquire_ownership`][acquire_ownership],

- [`acquire_ownership`][acquire_ownership] sets [`thd->owned_gtid`][owned_gtid].

--> [`thd->owned_gtid`][owned_gtid] is what should be used for logging.

[call_after_sync_hook]: https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/binlog.cc#L9137
[process_flush_stage_queue]: https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/binlog.cc#L9009
[assign_automatic_gtids_to_flush_group]: https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/binlog.cc#L8549

[acquire_ownership]: https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/rpl_gtid_state.cc#L77
[owned_gtid]: https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/sql_class.h#L3723

Below is the code-path for reaching `acquire_ownership`.

```
Thread 48 "connection" hit Breakpoint 1, Gtid_state::acquire_ownership (this=this@entry=0x74ae110, thd=thd@entry=0x7fff30001050, gtid=...) at ../../mysql-8.2.0/sql/rpl_gtid_state.cc:77
77      in ../../mysql-8.2.0/sql/rpl_gtid_state.cc
(gdb) backtrace
#0  Gtid_state::acquire_ownership (this=this@entry=0x74ae110, thd=thd@entry=0x7fff30001050, gtid=...) at ../../mysql-8.2.0/sql/rpl_gtid_state.cc:77
#1  0x0000000003e1ecd7 in Gtid_state::generate_automatic_gtid (this=this@entry=0x74ae110, thd=thd@entry=0x7fff30001050, specified_sidno=<optimized out>, specified_gno=specified_gno@entry=0, locked_sidno=locked_sidno@entry=0x7fffe0147dbc)
    at ../../mysql-8.2.0/sql/rpl_gtid_state.cc:525
#2  0x0000000003dbce3e in MYSQL_BIN_LOG::assign_automatic_gtids_to_flush_group (this=this@entry=0x6eb2040 <mysql_bin_log>, first_seen=first_seen@entry=0x7fff30001050) at ../../mysql-8.2.0/sql/binlog.cc:1582
#3  0x0000000003dcb17a in MYSQL_BIN_LOG::process_flush_stage_queue (this=this@entry=0x6eb2040 <mysql_bin_log>, total_bytes_var=total_bytes_var@entry=0x7fffe0147fa8, rotate_var=rotate_var@entry=0x7fffe0147fa7, out_queue_var=out_queue_var@entry=0x7fffe0147f98)
    at ../../mysql-8.2.0/sql/binlog.cc:8549
#4  0x0000000003dcb6d6 in MYSQL_BIN_LOG::ordered_commit (this=this@entry=0x6eb2040 <mysql_bin_log>, thd=0x7fff30001050, all=all@entry=true, skip_commit=skip_commit@entry=false) at ../../mysql-8.2.0/sql/binlog.cc:9008
#5  0x0000000003dcd471 in MYSQL_BIN_LOG::commit (this=<optimized out>, thd=<optimized out>, all=<optimized out>) at ../../mysql-8.2.0/sql/binlog.cc:8394
#6  0x0000000003465bdb in ha_commit_trans (thd=thd@entry=0x7fff30001050, all=all@entry=true, ignore_global_read_lock=ignore_global_read_lock@entry=false) at ../../mysql-8.2.0/sql/handler.cc:1794
#7  0x00000000032ef0b4 in trans_commit (thd=thd@entry=0x7fff30001050, ignore_global_read_lock=ignore_global_read_lock@entry=false) at ../../mysql-8.2.0/sql/transaction.cc:246
#8  0x0000000003178989 in mysql_create_db (thd=thd@entry=0x7fff30001050, db=db@entry=0x7fff3001c9c0 "test_jfg3", create_info=create_info@entry=0x7fffe014a1d0) at ../../mysql-8.2.0/sql/sql_db.cc:504
#9  0x00000000031cf53d in mysql_execute_command (thd=thd@entry=0x7fff30001050, first_level=first_level@entry=true) at ../../mysql-8.2.0/sql/sql_parse.cc:3943
#10 0x00000000031d284f in dispatch_sql_command (thd=0x7fff30001050, parser_state=parser_state@entry=0x7fffe014acf0) at ../../mysql-8.2.0/sql/sql_parse.cc:5479
#11 0x00000000031d3f63 in dispatch_command (thd=<optimized out>, thd@entry=0x7fff30001050, com_data=com_data@entry=0x7fffe014bb20, command=COM_QUERY) at ../../mysql-8.2.0/sql/sql_parse.cc:2136
#12 0x00000000031d5c7b in do_command (thd=thd@entry=0x7fff30001050) at ../../mysql-8.2.0/sql/sql_parse.cc:1465
#13 0x000000000333c9b6 in handle_connection (arg=arg@entry=0x9a918a0) at ../../mysql-8.2.0/sql/conn_handler/connection_handler_per_thread.cc:303
#14 0x00000000048a7ae8 in pfs_spawn_thread (arg=0x9b4afe0) at ../../../mysql-8.2.0/storage/perfschema/pfs.cc:3049
#15 0x00007ffff72a8134 in start_thread (arg=<optimized out>) at ./nptl/pthread_create.c:442
#16 0x00007ffff73287dc in clone3 () at ../sysdeps/unix/sysv/linux/x86_64/clone3.S:81
```


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### Other Notes

Semi-sync doc:
- https://dev.mysql.com/doc/refman/8.0/en/replication-semisync.html

There are two versions of the plugin: the master/slave version and the
source/replica version.  The code is shared, and adapted at compile-time
with the macro `USE_OLD_SEMI_SYNC_TERMINOLOGY`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc#L43
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source_plugin_old.cc#L23
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_replica_plugin.cc#L39
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_replica_plugin_old.cc#L23

These macro usages make the code more complex to understand, and potentially
un-grep-able:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc#L262

```
-- Command for checking if the plugin is loaded, and which version (OLD or NEW):
SELECT PLUGIN_NAME, PLUGIN_STATUS FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME LIKE '%semi%';
```

```
# Test case.
dbdeployer deploy replication mysql_8.2.0 --gtid --semi-sync

(
./node1/stop& ./node2/stop& wait
./m <<< "SET GLOBAL rpl_semi_sync_master_timeout = 1000"
./m <<< "CREATE DATABASE test_jfg"
grep -e "Timeout waiting for reply of binlog" master/data/msandbox.err
)

# Little victory below.
2024-02-14T19:21:23.445062Z 13 [Warning] [MY-014068] [Repl] Timeout waiting for reply of binlog (file: mysql-bin.000002, pos: 397, gtid: 00019201-1111-1111-1111-111111111111:36), semi-sync up to file , position 4.

# Above was short lived...
# wo gitd (dbdeployer deploy replication mysql_8.2.0 --semi-sync):
2024-02-14T19:24:28Z UTC - mysqld got signal 11 ;
Most likely, you have hit a bug, but this error can also be caused by malfunctioning hardware.
BuildID[sha1]=6976d9052513e7097409f4aeaff885724e535403
Thread pointer: 0x7f5bd8000b70
Attempting backtrace. You can use the following information to find out
where mysqld died. If you see no messages after this, something went
terribly wrong...
stack_bottom = 7f5c3c5f6bf0 thread_stack 0x100000
/home/jgagne/opt/mysql/mysql_8.2.0/bin/mysqld(my_print_stacktrace(unsigned char const*, unsigned long)+0x3d) [0x560e17ad6c7d]
/home/jgagne/opt/mysql/mysql_8.2.0/bin/mysqld(print_fatal_signal(int)+0x2a2) [0x560e169ba312]
/home/jgagne/opt/mysql/mysql_8.2.0/bin/mysqld(handle_fatal_signal+0x95) [0x560e169ba4b5]
/lib/x86_64-linux-gnu/libc.so.6(+0x3c050) [0x7f5c8405b050]
/home/jgagne/opt/mysql/mysql_8.2.0/bin/mysqld(mysql::gtid::Uuid::to_string(unsigned char const*, char*)+0x38) [0x560e184cd9f8]
/home/jgagne/opt/mysql/mysql_8.2.0/bin/mysqld(Gtid::to_string(mysql::gtid::Uuid const&, char*) const+0x1e) [0x560e1774c3ce]
/home/jgagne/opt/mysql/mysql_8.2.0/lib/plugin/semisync_master.so(+0x7990) [0x7f5c7582a990]
[...]

# After handling the ANONYMOUS case.
2024-02-14T20:51:52.597375Z 13 [Warning] [MY-014068] [Repl] Timeout waiting for reply of binlog (file: mysql-bin.000003, pos: 357, gtid: ANONYMOUS), semi-sync up to file mysql-bin.000001, position 8328.

# New test.
2024-02-14T20:59:10.465301Z 16 [Warning] [MY-014068] [Repl] Timeout waiting for reply of binlog (file: mysql-bin.000001, pos: 8529, gtid: 00019201-1111-1111-1111-111111111111:34), semi-sync up to file , position 4.

# And binlog content from above.
/home/jgagne/opt/mysql/mysql_8.2.0/bin/mysqlbinlog master/data/mysql-bin.000001
# at 8332
#240214 20:59:09 server id 19201  end_log_pos 8409 CRC32 0x5fc2d7ab     GTID    last_committed=33       sequence_number=34      rbr_only=no     original_committed_timestamp=1707944349454817   immediate_commit_timestamp=1707944349454817     transaction_length=197
# original_commit_timestamp=1707944349454817 (2024-02-14 20:59:09.454817 UTC)
# immediate_commit_timestamp=1707944349454817 (2024-02-14 20:59:09.454817 UTC)
/*!80001 SET @@session.original_commit_timestamp=1707944349454817*//*!*/;
/*!80014 SET @@session.original_server_version=80200*//*!*/;
/*!80014 SET @@session.immediate_server_version=80200*//*!*/;
SET @@SESSION.GTID_NEXT= '00019201-1111-1111-1111-111111111111:34'/*!*/;
# at 8409
#240214 20:59:09 server id 19201  end_log_pos 8529 CRC32 0x913fe291     Query   thread_id=16    exec_time=0     error_code=0    Xid = 101
SET TIMESTAMP=1707944349/*!*/;
/*!80016 SET @@session.default_table_encryption=0*//*!*/;
CREATE DATABASE test_jfg
```

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
<!-- EOF -->
