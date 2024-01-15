
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

Update: below was a mistake, until next "Update".

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

When checking `enum_gtid_type`, I saw that one a trx is assigned a GTID, this
is indicated in thd:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/sql/rpl_gtid.h#L3707

So we could use `current_thd` to get the GTID of the trx:
- https://github.com/jfg956/mysql-server/blob/mysql-8.2.0/plugin/semisync/semisync_source_plugin.cc#L591

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


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
<!-- EOF -->
