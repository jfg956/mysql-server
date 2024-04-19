
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
in which [`ReplSemiSyncMaster:commitTrx`](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source.cc#L635) is called.

Update 2024-01-15: below was a mistake, goto next "Update 2024-01-15" below to
skip.  Note to self: this was a short-lived mistake as it was only from
2024-01-10.

If we are able to get a GTID from the parameter of [`repl_semi_report_commit`](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc#L109)
(`Trans_param *param`), we will be able to push this information to `commitTrx`.
More about this parameter in the section [Trans_param](#trans_param).

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
But `ReplSemiSyncMaster:commitTrx` is also called in
[`repl_semi_report_binlog_sync`](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc#L95)
where the transaction information is not available.  This code-path is used for
`WAIT_AFTER_SYNC` ([rpl_semi_sync_master_wait_point](https://dev.mysql.com/doc/refman/8.0/en/replication-options-source.html#sysvar_rpl_semi_sync_master_wait_point)),
~~which is not the lossless semi-sync of 5.7~~ (it is, I was mistaken).
These days, almost ~~everyone should be using `WAIT_AFTER_COMMIT`~~
(no, everyone should be using `WAIT_AFTER_SYNC`)
(more about the difference in [Question about Semi-Synchronous Replication: the Answer with All the Details](https://percona.community/blog/2018/08/23/question-about-semi-synchronous-replication-answer-with-all-the-details/)).
We could could compromise in fixing Bug#113598 only for lossless semi-sync.
Or if we want the GTID also for the legacy semi-sync (~~`WAIT_AFTER_SYNC`~~
- no, the legacy semi=sync is `WAIT_AFTER_COMMIT`), we could 
save the GTID in the `repl_semi_report_commit` function for usage in
`repl_semi_report_binlog_sync`.

Update 2024-01-15: above was a mistake.  The `repl_semi_report_commit` observer
is for 5.5 semi-sync, and the lossless semi-sync of 5.7 uses the
`repl_semi_report_binlog_sync` observer (I confused `WAIT_AFTER_COMMIT` and
`WAIT_AFTER_SYNC`, probably hoping things would be simple and I would only have to
reference `Trans_gtid_info` from `Trans_param` to fix this).  I realized this
doing tests and need to pivot.


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### GTID Explorations

When checking `enum_gtid_type`, I saw that once a trx is assigned a GTID, this
is indicated in `thd->variables.gtid_next`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/rpl_gtid.h#L3707

(Update 2024-04-16: ...above probably a mistake...)

Reference to `thd->variables`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/sql_class.h#L1116

Reference to `thd->variables.gtid_next`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/system_variables.h#L355

So we could use `current_thd` to get the GTID of the trx:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc#L591

But when testing this, `current_thd->variables.gtid_next` is `AUTOMATIC`, arg !

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
2024-02-14: Starting back on this after almost a month...

The last tests were on using `thd->variables.gtid_next` to get the gtid, but
these showed this was still `AUTOMATIC` / not updated when calling 
`repl_semi_report_binlog_sync`.  This lead to digging how GTIDs are assigned,
details in the section [GTID Assignment](#gtid-assignment).  The TL&DR is that
at this point in the code, assignment has been done, but not in
`current_thd->variables.gtid_next`, only temporarily in
`current_thd->owned_gtid`.

(Update 2024-04-16: ...many errors in above, ...)

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
2024-04-09: Starting back on this after two months...

Previous work was in 8.2, but 8.3 released on 2024-01-16, so should rebase.

But dbdeployer not working well with gtids in 8.3.0, so new hurdle.  This is solved
with playing with dbdeployer options, see tests below.

Arg: global_sid_map was removed in 8.3.0, it is now global_tsid_map.
- 8.2.0: https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/rpl_gtid.h#L902
- 8.3.0: https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/rpl_gtid.h#L924

Impact is not too big, just s/global_sid_map/global_tsid_map/.

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
On 2024-04-11, all tests were working, I was ready to contrib.

But on 2024-04-12, I tested with AFTER_COMMIT, and crash !  Logs below.

This prompted me to check [mysql-test](#mysql-test) !

```
2024-04-15T18:21:59Z UTC - mysqld got signal 11 ;
Most likely, you have hit a bug, but this error can also be caused by malfunctioning hardware.
BuildID[sha1]=76de265dc2bffe92ec5dd17300a9bf57f11b90a1
Thread pointer: 0x7f89ac014180
Attempting backtrace. You can use the following information to find out
where mysqld died. If you see no messages after this, something went
terribly wrong...
stack_bottom = 7f8a086f6bf0 thread_stack 0x100000
 #0 0x55de2fe845d1 _Z18print_fatal_signali at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/signal_handler.cc:154
 #1 0x55de2fe84784 handle_fatal_signal at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/signal_handler.cc:230
 #2 0x7f8a55c5b04f <unknown> at sysdeps/unix/sysv/linux/x86_64/libc_sigaction.c:0
 #3 0x55de30c48693 _ZN5mysql4gtid4TsidC4ERKS1_ at gtid/tsid.h:63
 #4 0x55de30c48693 _ZNK18Gtid_specification9to_stringEPK8Tsid_mapPcb at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/rpl_gtid_specification.cc:180
 #5 0x7f8a4800fa0b _ZN18ReplSemiSyncMaster9commitTrxEPKcy at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/plugin/semisync/semisync_source.cc:805
 #6 0x55de30c71cab _ZN14Trans_delegate12after_commitEP3THDb at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/rpl_handler.cc:833
 #7 0x55de30bea926 _ZN13MYSQL_BIN_LOG32process_after_commit_stage_queueEP3THDS1_ at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/binlog.cc:8683
 #8 0x55de30bff6cd _ZN13MYSQL_BIN_LOG14ordered_commitEP3THDbb at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/binlog.cc:9179
 #9 0x55de30c00b39 _ZN13MYSQL_BIN_LOG6commitEP3THDb at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/binlog.cc:8403
 #10 0x55de2ff989f1 _Z15ha_commit_transP3THDbb at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/handler.cc:1808
 #11 0x55de2fe32aea _Z12trans_commitP3THDb at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/transaction.cc:246
 #12 0x55de2fcc1f23 _Z15mysql_create_dbP3THDPKcP14HA_CREATE_INFO at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/sql_db.cc:504
 #13 0x55de2fd13205 _Z21mysql_execute_commandP3THDb at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/sql_parse.cc:3764
 #14 0x55de2fd154dc _Z20dispatch_sql_commandP3THDP12Parser_state at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/sql_parse.cc:5301
 #15 0x55de2fd173a3 _Z16dispatch_commandP3THDPK8COM_DATA19enum_server_command at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/sql_parse.cc:2133
 #16 0x55de2fd17e16 _Z10do_commandP3THD at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/sql_parse.cc:1462
 #17 0x55de2fe7491f handle_connection at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/conn_handler/connection_handler_per_thread.cc:303
 #18 0x55de3172328f pfs_spawn_thread at /mnt/jgagne_src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/storage/perfschema/pfs.cc:3050
 #19 0x7f8a55ca8133 start_thread at ./nptl/pthread_create.c:442
 #20 0x7f8a55d287db clone3 at sysdeps/unix/sysv/linux/x86_64/clone3.S:81
 #21 0xffffffffffffffff <unknown>

Trying to get some variables.
Some pointers may be invalid and cause the dump to abort.
Query (7f89ac007470): CREATE DATABASE test_jfg
Connection ID (thread ID): 11
Status: NOT_KILLED

The manual page at http://dev.mysql.com/doc/mysql/en/crashing.html contains
information that should help you find out what is causing the crash.
```

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
I might need [Trans_param](#trans_param) after all...

Or get inspiration from how Trans_param is set:
- https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/rpl_handler.cc#L811

Above was naive, it only takes into account AUTOMATIC...

Which might mean there is a bug in the way `trans_param.gtid_info` is set...

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### Tests

```
export mysql_v=8.3.0
export gtid='-c gtid_mode=ON -c enforce-gtid-consistency -c relay-log-recovery=on'
export type="" # "" or "-debug".

(
export bin_dir="$HOME/opt/mysql/mysql_$mysql_v"
build_dir="$HOME/src/github/https/jfg956/mysql-server/worktrees/mysql-${mysql_v}_bug113598/build"
sb_dir="$HOME/sandboxes/rsandbox_mysql_${mysql_v//./_}"

f=$bin_dir/bin/mysqld
test -e ${f}_org || mv ${f}{,_org}
rsync $build_dir/default/bin/mysqld ${f}_bug113598
rsync $build_dir/debug/bin/mysqld ${f}-debug_bug113598

f=$bin_dir/lib/plugin/semisync_master.so
test -e ${f}_org || mv ${f}{,_org}
rsync $build_dir/default/plugin_output_directory/semisync_master.so ${f}_bug113598
rsync $build_dir/debug/plugin_output_directory/semisync_master.so ${f}-debug_bug113598

function set_v() {
  ( cd $bin_dir/bin; rm -f mysqld; ln -s mysqld$1 mysqld; )
  ( cd $bin_dir/lib/plugin; rm -f semisync_master.so; ln -s semisync_master.so$1 semisync_master.so; )
}

function create_sb() {
  cd; rm -rf $sb_dir; set_v _org
  dbdeployer deploy replication mysql_$mysql_v $1 --semi-sync > /dev/null
  cd $sb_dir
  ( ./node1/stop& ./node2/stop& wait; ./stop_all; ) > /dev/null
}

function run_test() {
  local wp=$1
  ( ./master/start; ./node1/start& ./node2/start& wait; )  > /dev/null
  ( s="SET GLOBAL rpl_semi_sync_master_"; ./m <<< "DROP DATABASE IF EXISTS test_jfg; ${s}timeout=1000; ${s}wait_point=$wp"; )
  ( ./node1/stop& ./node2/stop& wait; ) > /dev/null
  ./m -N <<< "CREATE DATABASE test_jfg; SELECT now(), @@GLOBAL.gtid_executed"
  sleep 1; grep -e "Timeout waiting for reply of binlog" master/data/msandbox.err | tail -n 1
  ./stop_all > /dev/null
}

#create_sb "$gtid"; set_v ${type}_bug113598; run_test AFTER_SYNC

is="AFTER_SYNC AFTER_COMMIT"
bs="_org ${type}_bug113598"

create_sb "$gtid"
for i in $is; do for b in $bs; do set_v $b; echo; echo "# With GTIDs, $i, $b:"; run_test $i; done; done

# Add test for tags !
# Add test for setting gtid_next !

create_sb ""
for i in $is; do for b in $bs; do set_v $b; echo; echo "# Without GTIDs, $i, $b:"; run_test $i; done; done

set_v _org; rm -rf $sb_dir
)

# With GTIDs, org, AFTER_SYNC:
2024-04-12 21:21:14     00019301-1111-1111-1111-111111111111:1-35
2024-04-12T21:21:14.678170Z 11 [Warning] [MY-011153] [Repl] Timeout waiting for reply of binlog (file: mysql-bin.000002, pos: 589), semi-sync up to file mysql-bin.000002, position 392.

# With GTIDs, bug113598, AFTER_SYNC:
2024-04-12 21:21:38     00019301-1111-1111-1111-111111111111:1-37
2024-04-12T21:21:38.002242Z 11 [Warning] [MY-014071] [Repl] Timeout waiting for reply of binlog (file: mysql-bin.000003, pos: 598, gtid: 00019301-1111-1111-1111-111111111111:37), semi-sync up to file mysql-bin.000003, position 401.

# Without GTIDs, org, AFTER_SYNC:
2024-04-12 21:23:29
2024-04-12T21:23:29.933685Z 11 [Warning] [MY-011153] [Repl] Timeout waiting for reply of binlog (file: mysql-bin.000002, pos: 549), semi-sync up to file mysql-bin.000002, position 352.

# Without GTIDs, bug113598, AFTER_SYNC:
2024-04-12 21:23:54
2024-04-12T21:23:54.310762Z 11 [Warning] [MY-014071] [Repl] Timeout waiting for reply of binlog (file: mysql-bin.000003, pos: 558, gtid: ANONYMOUS), semi-sync up to file mysql-bin.000003, position 361.
```


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### GTID to_string

A little complicated, not fully remembering how I gathered this...
- Gtid: https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/rpl_gtid.h#L1136
- Gtid_specification: https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/rpl_gtid.h#L3866

Example in 8.2.0:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/rpl_rli_pdb.cc#L1543
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/sys_vars.h#L2443
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/sql_db.cc#L916
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/rpl_gtid_execution.cc#L503
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/item_gtid_func.cc#L110

From 8.2.0 to 8.3.0, global_sid_map became global_tsid_map:
- https://github.com/jfg956/mysql-server/commit/b6776e970248445d78da62e11b7dffd0a7e79668

Above in 8.3.0:
- https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/rpl_rli_pdb.cc#L1553
- https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/sys_vars.h#L2443
- https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/sql_db.cc#L916
- https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/rpl_gtid_execution.cc#L529
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/item_gtid_func.cc#L110

Remembering: because I first used `Trans_param`, I needed to manually `to_string`
`Trans_gtid_info` (in this [commit](https://github.com/jfg956/mysql-server/commit/beb69daa22711c9792cc54645a1e4938e6a89339)).


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### Trans_param

`Trans_param` is a parameter to [`repl_semi_report_commit`](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc#L109).

`repl_semi_report_commit` is the [`after_commit`](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc#L409C43-L409C55)
hook of the pluggin.

`Trans_param` definition:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/replication.h#L132

It contains a `Trans_gtid_info` (`gtid_info`):
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

`sidno` is not fully understood

`gno` is the numeric part of the gtid

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
Kind of weird that `Trans_param` does not "just" contain a
[`Gtid_specification`](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/rpl_gtid.h#L3803),
and that `Trans_gtid_info` almost re-invents `Gtid_specification`.

`gtid_info` is set in `Trans_delegate::after_commit`:
- 8.2.0: https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/rpl_handler.cc#L816
- 8.0.36: https://github.com/jfg956/mysql-server/blob/mysql-8.0.36/sql/rpl_handler.cc#L812
- 8.3.0: https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/rpl_handler.cc#L813

`Trans_gtid_info` was introduced in 5.7 for GR (in file `sql/replication.h`):
- https://github.com/jfg956/mysql-server/commit/22c27ae3aca2865be2f41381f80e91637c7403b7#diff-0195e233afcb258d67aa9c68f0810adea5733c6ce785869ce37455c9c7539cc9

But it looks like `Trans_gtid_info` is not used in 5.7, at least not in
`Trans_delegate::after_commit` (no `gtid_info` in below):
- https://github.com/jfg956/mysql-server/blob/mysql-5.7.44/sql/rpl_handler.cc#L856

But it is used in `Trans_delegate::before_commit`: 
- https://github.com/jfg956/mysql-server/blob/mysql-5.7.44/sql/rpl_handler.cc#L643

In all of 8.3.0, 8.2.0 and 8.0.36 (links above) and in
`Trans_delegate::after_commit`, `gtid_info` is set from
`thd->rpl_thd_ctx.last_used_gtid_tracker_ctx()`.  Considering we could access
this via `current_thd` (`#include "sql/current_thd.h"`), `gtid_info` looks like
a waste in `Trans_param`.

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### GTID Assignment

In full release binaries (full as opposed to minimal), below is the stack trace
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
- before calling [`call_after_sync_hook`](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/binlog.cc#L9137), `ordered_commit` calls [`process_flush_stage_queue`](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/binlog.cc#L9009),
- `process_flush_stage_queue` calls [`assign_automatic_gtids_to_flush_group`](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/binlog.cc#L8549),
- `assign_automatic_gtids_to_flush_group` calls [`generate_automatic_gtid`](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/binlog.cc#L1582),
- `generate_automatic_gtid` calls [`acquire_ownership`](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/rpl_gtid_state.cc#L77),
- `acquire_ownership` sets [`thd->owned_gtid`](https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/sql_class.h#L3723).

--> `thd->owned_gtid` is what should be used for logging.

2024-04-15: above was naive (using `thd->owned_gtid`) because this only works when
`gtid_next` is AUTOMATIC.  All other cases also need to be taken into account.
The best course of action is probably to take inspiration on
`assign_automatic_gtids_to_flush_group` where all the cases are taken into
account (`head->variables.gtid_next.type`).

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
### mysql-test

Example of running mysql-test in below.
- https://www.fromdual.com/building-mariadb-server-from-the-sources

Laurynas blogged about script for running mtr:
- https://of-code.blogspot.com/2024/01/introducing-patch2testlist-for-mysql.html

Trying to check mysql-test doc, but it ended-up as an archeology expedition:
- https://bugs.mysql.com/bug.php?id=114671

...

Below, example of a mysql-test semi-sync test, both working (mysql-8.3.0)
and failing (mysql-8.3.0_bug113598 [commit link](https://github.com/jfg956/mysql-server/commit/06d50228147b534fd5d193e7072b892cb6a5ac74)).

```
mysql-8.3.0/build/debug/mysql-test$ ./mysql-test-run.pl rpl_nogtid.rpl_semi_sync
Logging: /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0/mysql-test/mysql-test-run.pl  rpl_nogtid.rpl_semi_sync
MySQL Version 8.3.0
Path length (117) is longer than maximum supported length (108) and will be truncated at /usr/lib/x86_64-linux-gnu/perl-base/Socket.pm line 193.
Too long tmpdir path '/home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0/build/debug/mysql-test/var/tmp'  creating a shorter one
 - Using tmpdir: '/tmp/QXQV1md6K2'

Checking supported features
 - Binaries are debug compiled
Using 'all' suites
Collecting tests
 - Adding combinations for rpl_nogtid
Checking leftover processes
Removing old var directory
Creating var directory '/home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0/build/debug/mysql-test/var'
Installing system database
Using parallel: 1

==============================================================================
                  TEST NAME                       RESULT  TIME (ms) COMMENT
------------------------------------------------------------------------------
[ 25%] rpl_nogtid.rpl_semi_sync 'mix'            [ pass ]  212678
[ 50%] rpl_nogtid.rpl_semi_sync 'row'            [ pass ]  210683
[ 75%] rpl_nogtid.rpl_semi_sync 'stmt'           [ pass ]  211838
[100%] shutdown_report                           [ pass ]
------------------------------------------------------------------------------
The servers were restarted 2 times
The servers were reinitialized 0 times
Spent 635.199 of 753 seconds executing testcases

Completed: All 4 tests were successful.
```

```
mysql-8.3.0_bug113598/build/debug/mysql-test$ ./mysql-test-run.pl rpl_nogtid.rpl_semi_sync
Logging: /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/mysql-test/mysql-test-run.pl  rpl_nogtid.rpl_semi_sync
MySQL Version 8.3.0
Path length (127) is longer than maximum supported length (108) and will be truncated at /usr/lib/x86_64-linux-gnu/perl-base/Socket.pm line 193.
Too long tmpdir path '/home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/build/debug/mysql-test/var/tmp'  creating a shorter one
 - Using tmpdir: '/tmp/uTpLF67dnn'

Checking supported features
 - Binaries are debug compiled
Using 'all' suites
Collecting tests
 - Adding combinations for rpl_nogtid
Checking leftover processes
Removing old var directory
Creating var directory '/home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/build/debug/mysql-test/var'
Installing system database
Using parallel: 1

==============================================================================
                  TEST NAME                       RESULT  TIME (ms) COMMENT
------------------------------------------------------------------------------
[ 25%] rpl_nogtid.rpl_semi_sync 'mix'            [ fail ]
        Test ended at 2024-04-17 18:51:04

CURRENT_TEST: rpl_nogtid.rpl_semi_sync
mysqltest: At line 109: Query 'create table t1 (a int) engine=$engine_type' failed.
ERROR 2013 (HY000): Lost connection to MySQL server during query

The result from queries just before the failure was:
rpl_semi_sync_source_enabled    OFF
[ enable semi-sync on source ]
set global rpl_semi_sync_source_enabled = 1;
show variables like 'rpl_semi_sync_source_enabled';
Variable_name   Value
rpl_semi_sync_source_enabled    ON
[ status of semi-sync on source should be ON even without any semi-sync slaves ]
show status like 'Rpl_semi_sync_source_clients';
Variable_name   Value
Rpl_semi_sync_source_clients    0
show status like 'Rpl_semi_sync_source_status';
Variable_name   Value
Rpl_semi_sync_source_status     ON
show status like 'Rpl_semi_sync_source_yes_tx';
Variable_name   Value
Rpl_semi_sync_source_yes_tx     0
#
# BUG#45672 Semisync repl: ActiveTranx:insert_tranx_node: transaction node allocation failed
# BUG#45673 Semisync reports correct operation even if no slave is connected
#
safe_process[4603]: Child process: 4604, exit: 1


Server [mysqld.1 - pid: 4509, winpid: 4509, exit: 256] failed during test run
Server log from this test:
----------SERVER LOG START-----------
[...]
mysqld: /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/rpl_gtid.h:1114: void Gtid::set(rpl_sidno, rpl_gno): Assertion `sidno_arg > 0' failed.
[...]
----------SERVER LOG END-------------


 - the logfile can be found in '/home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/build/debug/mysql-test/var/log/rpl_nogtid.rpl_semi_sync-mix/rpl_semi_sync.log'

 - found 'core' (0/5)

Trying 'dbx' to get a backtrace

Trying 'gdb' to get a backtrace
Guessing that core was generated by '/home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/build/debug/runtime_output_directory/mysqld'
Output from gdb follows. The first stack trace is from the failing thread.
The following stack traces are from all threads (so the failing one is
duplicated).
--------------------------
[...]
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
Core was generated by `/home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug1135'.
Program terminated with signal SIGABRT, Aborted.
#0  __pthread_kill_implementation (threadid=<optimized out>, signo=6, no_tid=<optimized out>) at ./nptl/pthread_kill.c:44
44      ./nptl/pthread_kill.c: No such file or directory.
[Current thread is 1 (Thread 0x7f1ad03f16c0 (LWP 4615))]
#0  __pthread_kill_implementation (threadid=<optimized out>, signo=6, no_tid=<optimized out>) at ./nptl/pthread_kill.c:44
#1  0x000055f915137762 in my_write_core (sig=6) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/mysys/stacktrace.cc:339
#2  0x000055f913c72899 in handle_fatal_signal (sig=6) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/signal_handler.cc:235
#3  <signal handler called>
#4  __pthread_kill_implementation (threadid=<optimized out>, signo=signo@entry=6, no_tid=no_tid@entry=0) at ./nptl/pthread_kill.c:44
#5  0x00007f1ae6fcae8f in __pthread_kill_internal (signo=6, threadid=<optimized out>) at ./nptl/pthread_kill.c:78
#6  0x00007f1ae6f7bfb2 in __GI_raise (sig=sig@entry=6) at ../sysdeps/posix/raise.c:26
#7  0x00007f1ae6f66472 in __GI_abort () at ./stdlib/abort.c:79
#8  0x00007f1ae6f66395 in __assert_fail_base (fmt=0x7f1ae70daa90 "%s%s%s:%u: %s%sAssertion `%s' failed.\n%n", assertion=assertion@entry=0x7f1ae00a6a79 "sidno_arg > 0", file=file@entry=0x7f1ae00a6a18 "/home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/rpl_gtid.h", line=line@entry=1114, function=function@entry=0x7f1ae00a69f0 "void Gtid::set(rpl_sidno, rpl_gno)") at ./assert/assert.c:92
#9  0x00007f1ae6f74eb2 in __GI___assert_fail (assertion=0x7f1ae00a6a79 "sidno_arg > 0", file=0x7f1ae00a6a18 "/home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/rpl_gtid.h", line=1114, function=0x7f1ae00a69f0 "void Gtid::set(rpl_sidno, rpl_gno)") at ./assert/assert.c:101
#10 0x00007f1ae009d9c7 in Gtid::set (this=0x7f1ad03eb788, sidno_arg=0, gno_arg=0) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/rpl_gtid.h:1114
#11 0x00007f1ae009da72 in Gtid_specification::set (this=0x7f1ad03eb780, sidno=0, gno=0) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/rpl_gtid.h:4004
#12 0x00007f1ae009dabc in Gtid_specification::set (this=0x7f1ad03eb780, gtid_param=...) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/rpl_gtid.h:4041
#13 0x00007f1ae009b414 in ReplSemiSyncMaster::commitTrx (this=0x7f1a3804acf0, trx_wait_binlog_name=0x7f1a3806ed30 "master-bin.000001", trx_wait_binlog_pos=360) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/plugin/semisync/semisync_source.cc:800
#14 0x00007f1ae00a3d72 in repl_semi_report_commit (param=0x7f1ad03ebae0) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/plugin/semisync/semisync_source_plugin.cc:115
#15 0x000055f914d68d33 in Trans_delegate::after_commit (this=0x55f9193024b8 <delegates_init()::place_trans_mem>, thd=0x7f1a38001050, all=true) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/rpl_handler.cc:833
#16 0x000055f914c9cb96 in MYSQL_BIN_LOG::process_after_commit_stage_queue (this=0x55f919300ce0 <mysql_bin_log>, thd=0x7f1a38001050, first=0x7f1a38001050) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/binlog.cc:8683
#17 0x000055f914c9e707 in MYSQL_BIN_LOG::ordered_commit (this=0x55f919300ce0 <mysql_bin_log>, thd=0x7f1a38001050, all=true, skip_commit=false) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/binlog.cc:9179
#18 0x000055f914c9bf20 in MYSQL_BIN_LOG::commit (this=0x55f919300ce0 <mysql_bin_log>, thd=0x7f1a38001050, all=true) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/binlog.cc:8403
#19 0x000055f913e1a60d in ha_commit_trans (thd=0x7f1a38001050, all=true, ignore_global_read_lock=false) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/handler.cc:1808
#20 0x000055f913bde2a1 in trans_commit_implicit (thd=0x7f1a38001050, ignore_global_read_lock=false) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/transaction.cc:339
#21 0x000055f913b01a8e in mysql_create_table (thd=0x7f1a38001050, create_table=0x7f1a38023078, create_info=0x7f1ad03edfb0, alter_info=0x7f1ad03ede40) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/sql_table.cc:10345
#22 0x000055f9141a0053 in Sql_cmd_create_table::execute (this=0x7f1a38024010, thd=0x7f1a38001050) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/sql_cmd_ddl_table.cc:459
#23 0x000055f913a269b8 in mysql_execute_command (thd=0x7f1a38001050, first_level=true) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/sql_parse.cc:3579
#24 0x000055f913a2c279 in dispatch_sql_command (thd=0x7f1a38001050, parser_state=0x7f1ad03efa00) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/sql_parse.cc:5301
#25 0x000055f913a2295a in dispatch_command (thd=0x7f1a38001050, com_data=0x7f1ad03f0a80, command=COM_QUERY) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/sql_parse.cc:2133
#26 0x000055f913a207a4 in do_command (thd=0x7f1a38001050) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/sql_parse.cc:1462
#27 0x000055f913c5cfa0 in handle_connection (arg=0x55f91c584210) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/sql/conn_handler/connection_handler_per_thread.cc:303
#28 0x000055f915d770e9 in pfs_spawn_thread (arg=0x55f91c2fc8a0) at /home/jgagne/src/github/https/jfg956/mysql-server/worktrees/mysql-8.3.0_bug113598/storage/perfschema/pfs.cc:3050
#29 0x00007f1ae6fc9134 in start_thread (arg=<optimized out>) at ./nptl/pthread_create.c:442
#30 0x00007f1ae70497dc in clone3 () at ../sysdeps/unix/sysv/linux/x86_64/clone3.S:81
[...]
------------------------------------------------------------------------------
1  of 3 test(s) completed.
Not all tests completed: rpl_nogtid.rpl_semi_sync rpl_nogtid.rpl_semi_sync

The servers were restarted 0 times
The servers were reinitialized 0 times
Spent 0.000 of 181 seconds executing testcases

In completed tests: Failed 1/1 tests, 0.00% were successful.

Failing test(s): rpl_nogtid.rpl_semi_sync

The log files in var/log may give you some hint of what went wrong.

If you want to report this error, please read first the documentation
at http://dev.mysql.com/doc/mysql/en/mysql-test-suite.html

mysql-test-run: *** ERROR: there were failing test cases
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
# Old tests with 8.2.0.
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
