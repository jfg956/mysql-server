
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

We would like to fix [Bug#91737](https://bugs.mysql.com/bug.php?id=91737):
Please log GTID_EXECUTED when running RESET MASTER.

### Exlploration from Bug Report:

From the bug report, when doing:
- `RESET MASTER;`
- `SET GLOBAL GTID_PURGED = "+00016745-1111-1111-1111-111111111111:1-20";`

I get below in the logs.

```
2024-02-21T21:21:14.075552Z 9 [System] [MY-010916] [Server] @@GLOBAL.GTID_PURGED was changed from '' to '00016745-1111-1111-1111-111111111111:1-20'.
2024-02-21T21:21:14.075644Z 9 [System] [MY-010917] [Server] @@GLOBAL.GTID_EXECUTED was changed from '' to '00016745-1111-1111-1111-111111111111:1-20'.
```

^^ in:
- https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/share/messages_to_clients.txt#L6974
- https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/share/messages_to_error_log.txt#L2807

ER_GTID_PURGED_WAS_UPDATED and ER_GTID_EXECUTED_WAS_UPDATED in:
- https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/sys_vars.cc#L6547

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### Finding where to add logs in RESET MASTER:

I ended-up spellunking in the parser:
- RESET is handled here: https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/sql_yacc.yy#L14451
- In above, there is `reset_options`: https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/sql_yacc.yy#L14468
- These options lead to `reset_option`: https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/sql_yacc.yy#L14510
- One of these option is `master_or_binary_logs_and_gtids`: https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/sql_yacc.yy#L14138

`RESET` sets `lex->sql_command= SQLCOM_RESET; lex->type=0;`.

The `reset_option` `master_or_binary_logs_and_gtids` sets `Lex->type|= REFRESH_SOURCE;`.

`REFRESH_SOURCE` is reference here: https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/sql_reload.cc#L338

This is in `handle_reload_request`  https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/sql_reload.cc#L138

We ended-up in `handle_reload_request` here: https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/sql_parse.cc#L4106

A little above calling `handle_reload_request`, there is a `case SQLCOM_RESET` which falls through `case SQLCOM_FLUSH` which ends-up calling `handle_reload_request`.

Back at `REFRESH_SOURCE`, this ends-up calling `reset_binary_logs_and_gtids` here: https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/sql_reload.cc#L349

So the important function is `reset_binary_logs_and_gtids`: https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/rpl_source.cc#L1199

Not super important in the context of this change, but interesting to note,
the important bits of `reset_binary_logs_and_gtids` are:
- `MYSQL_BIN_LOG::reset_logs`: https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/binlog.cc#L5634
- `Gtid_state::clear`: https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/rpl_gtid_state.cc#L57

The code for this operation changes from 8.0.36 to 8.3.0:
- `reset_master` in 8.0.36: https://github.com/jfg956/mysql-server/blob/mysql-8.0.36/sql/rpl_source.cc#L1195
- `reset_binary_logs_and_gtids` in 8.3.0: https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/rpl_source.cc#L1199

This is because `RESET MASTER` is part of the "offending" language Oracle is
getting rid of, and this was not yet done in 8.0.36.

At 1st, I thought of making this change in such a way that it would be possible
to easily back-port in 8.0 with keeping the backward compatible behavior with
a global variables, but I am giving-up on this.


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### Testing:

Trying to find a test to get inspiration from (`GTID_PURGED`), but finding none:

```
$ grep -r -e 010916 -e 010917 -e GLOBAL.GTID_PURGED.was.changed -e GLOBAL.GTID_EXECUTED.was.changed . | grep test
./mysql-test/suite/collations/r/codepoint_order.result:00010916	F090A496	PHOENICIAN NUMBER ONE
./mysql-test/suite/collations/r/codepoint_order.result:00010917	F090A497	PHOENICIAN NUMBER TEN
./mysql-test/suite/collations/r/root.result:00010917	1B5B	PHOENICIAN NUMBER TEN
./mysql-test/suite/collations/r/root.result:00010916	1C3E	PHOENICIAN NUMBER ONE
./mysql-test/suite/collations/r/root.result:00010917	1B5B00000020	PHOENICIAN NUMBER TEN
./mysql-test/suite/collations/r/root.result:00010916	1C3E00000020	PHOENICIAN NUMBER ONE
./mysql-test/suite/collations/r/root.result:00010917	1B5B0000002000000002	PHOENICIAN NUMBER TEN
./mysql-test/suite/collations/r/root.result:00010916	1C3E0000002000000002	PHOENICIAN NUMBER ONE
$

$ grep -r -e 010916 -e 010917 -e was.changed . | grep test | grep -v -e "^./jfg_brain_dump.md"
./unittest/gunit/keyring/keys_container-t.cc:  // it should not be possible to store_key if the keyring file was changed
./extra/icu/icu-release-73-1/source/common/unicode/bytestream.h:// Assertion-style error handling, not available in ICU, was changed to
./mysql-test/r/grant.result:#          password was changed and implement password rotation.
./mysql-test/t/transactional_acl_tables.test:# Since the definition of mysql.user was changed by the WL#6409 (the column
./mysql-test/t/transactional_acl_tables.test:# 'password' was removed) the number of column Event_priv was changed by 1.
./mysql-test/t/alter_table.test:# The column's character set was changed but the actual data was not
./mysql-test/t/grant.test:--echo #          password was changed and implement password rotation.
./mysql-test/suite/perfschema/r/variables_info.result:# user must be WL9720 since variable was changed globally by wl9720 user
./mysql-test/suite/perfschema/t/variables_info.test:--echo # user must be WL9720 since variable was changed globally by wl9720 user
./mysql-test/suite/group_replication/r/gr_fragmentation_options.result:include/assert.inc [Assert that communication_max_message_size was changed to the default value]
./mysql-test/suite/group_replication/r/gr_fragmentation_options.result:include/assert.inc [Assert that communication_max_message_size was changed]
./mysql-test/suite/group_replication/t/gr_fragmentation_options.test:--let $assert_text= Assert that communication_max_message_size was changed to the default value
./mysql-test/suite/group_replication/t/gr_fragmentation_options.test:--let $assert_text= Assert that communication_max_message_size was changed
./mysql-test/suite/ndb/t/ndbinfo.test:#   for those tabes which was changed to prefix ndb$
./mysql-test/suite/ndb/t/ndb_index_name_format.test:# This was changed to a new index name format in 5.1.12, with this format
./mysql-test/suite/funcs_1/views/func_view.inc:# Set the following to (37,2) since the default was changed to (10,0) - OBN
./mysql-test/suite/binlog/t/binlog_server_start_options.test:# Check if datadir was changed to the correct value via
./mysql-test/suite/parts/inc/partition_check.inc:# - f_charbig is typically used for showing if something was changed.          #
./mysql-test/suite/collations/r/codepoint_order.result:00010916	F090A496	PHOENICIAN NUMBER ONE
./mysql-test/suite/collations/r/codepoint_order.result:00010917	F090A497	PHOENICIAN NUMBER TEN
./mysql-test/suite/collations/r/root.result:00010917	1B5B	PHOENICIAN NUMBER TEN
./mysql-test/suite/collations/r/root.result:00010916	1C3E	PHOENICIAN NUMBER ONE
./mysql-test/suite/collations/r/root.result:00010917	1B5B00000020	PHOENICIAN NUMBER TEN
./mysql-test/suite/collations/r/root.result:00010916	1C3E00000020	PHOENICIAN NUMBER ONE
./mysql-test/suite/collations/r/root.result:00010917	1B5B0000002000000002	PHOENICIAN NUMBER TEN
./mysql-test/suite/collations/r/root.result:00010916	1C3E0000002000000002	PHOENICIAN NUMBER ONE
./mysql-test/suite/sys_vars/r/require_row_format_restrictions.result:include/assert.inc [Table t1 was changed]
./mysql-test/suite/sys_vars/t/require_row_format_restrictions.test:--let $assert_text = Table t1 was changed
$
```

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Manually testing:

```
mysql [localhost:8300] {msandbox} ((none)) > show global variables like "gtid_mode"; flush status; flush status; show binary log status\G RESET BINARY LOGS AND GTIDS; show binary log status\G
+---------------+-------+
| Variable_name | Value |
+---------------+-------+
| gtid_mode     | ON    |
+---------------+-------+
1 row in set (0.01 sec)

Query OK, 0 rows affected (0.01 sec)

Query OK, 0 rows affected (0.01 sec)

*************************** 1. row ***************************
             File: binlog.000001
         Position: 468
     Binlog_Do_DB: 
 Binlog_Ignore_DB: 
Executed_Gtid_Set: 00008300-0000-0000-0000-000000008300:1-2
1 row in set (0.00 sec)

Query OK, 0 rows affected (0.10 sec)

*************************** 1. row ***************************
             File: binlog.000001
         Position: 158
     Binlog_Do_DB: 
 Binlog_Ignore_DB: 
Executed_Gtid_Set: 
1 row in set (0.00 sec)

2024-03-25T18:09:10.077668Z 8 [System] [MY-010917] [Repl] @@GLOBAL.GTID_EXECUTED was changed from '00008300-0000-0000-0000-000000008300:1-2' to ''.
```

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

I also tested with GTID disabled:

```
mysql [localhost:8300] {msandbox} ((none)) > show global variables like "gtid_mode"; flush status; flush status; show binary log status\G RESET BINARY LOGS AND GTIDS; show binary log status\G
+---------------+-------+
| Variable_name | Value |
+---------------+-------+
| gtid_mode     | OFF   |
+---------------+-------+
1 row in set (0.01 sec)

Query OK, 0 rows affected (0.01 sec)

Query OK, 0 rows affected (0.12 sec)

*************************** 1. row ***************************
             File: binlog.000001
         Position: 468
     Binlog_Do_DB: 
 Binlog_Ignore_DB: 
Executed_Gtid_Set: 
1 row in set (0.00 sec)

Query OK, 0 rows affected (0.07 sec)

*************************** 1. row ***************************
             File: binlog.000001
         Position: 158
     Binlog_Do_DB: 
 Binlog_Ignore_DB: 
Executed_Gtid_Set: 
1 row in set (0.01 sec)

2024-03-25T18:10:59.012374Z 8 [System] [MY-010917] [Repl] @@GLOBAL.GTID_EXECUTED was changed from '' to ''.
```

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

It looks weird to have a GTID_EXECUTED line in the logs when gtid is disabled, 
but as it is allowed to set GTID_PURGED with gtid_mode = OFF, I guess it is ok.

```
mysql [localhost:8300] {msandbox} ((none)) > show global variables like "gtid_mode"; show binary log status\G set global gtid_purged="+00008300-0000-0000-0000-000000008301:1-3"; show binary log status\G
+---------------+-------+
| Variable_name | Value |
+---------------+-------+
| gtid_mode     | OFF   |
+---------------+-------+
1 row in set (0.00 sec)

*************************** 1. row ***************************
             File: binlog.000001
         Position: 158
     Binlog_Do_DB: 
 Binlog_Ignore_DB: 
Executed_Gtid_Set: 
1 row in set (0.00 sec)

Query OK, 0 rows affected (0.01 sec)

*************************** 1. row ***************************
             File: binlog.000001
         Position: 158
     Binlog_Do_DB: 
 Binlog_Ignore_DB: 
Executed_Gtid_Set: 00008300-0000-0000-0000-000000008301:1-3
1 row in set (0.00 sec)

2024-03-25T18:12:47.242547Z 8 [System] [MY-010916] [Server] @@GLOBAL.GTID_PURGED was changed from '' to '00008300-0000-0000-0000-000000008301:1-3'.
2024-03-25T18:12:47.242620Z 8 [System] [MY-010917] [Server] @@GLOBAL.GTID_EXECUTED was changed from '' to '00008300-0000-0000-0000-000000008301:1-3'.

mysql [localhost:8300] {msandbox} ((none)) > flush status; flush status; show binary log status\G RESET BINARY LOGS AND GTIDS; show binary log status\G
Query OK, 0 rows affected (0.00 sec)

Query OK, 0 rows affected (0.00 sec)

*************************** 1. row ***************************
             File: binlog.000001
         Position: 468
     Binlog_Do_DB: 
 Binlog_Ignore_DB: 
Executed_Gtid_Set: 00008300-0000-0000-0000-000000008301:1-3
1 row in set (0.00 sec)

Query OK, 0 rows affected (0.02 sec)

*************************** 1. row ***************************
             File: binlog.000001
         Position: 158
     Binlog_Do_DB: 
 Binlog_Ignore_DB: 
Executed_Gtid_Set: 
1 row in set (0.00 sec)

2024-03-25T18:13:39.124002Z 8 [System] [MY-010917] [Repl] @@GLOBAL.GTID_EXECUTED was changed from '00008300-0000-0000-0000-000000008301:1-3' to ''.
```

