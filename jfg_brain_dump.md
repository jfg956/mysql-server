
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

We would like to fix [Bug#91737](https://bugs.mysql.com/bug.php?id=91737):
Please log GTID_EXECUTED when running RESET MASTER.

...

When doing:
- `RESET MASTER;`
- `SET GLOBAL GTID_PURGED = "+00016745-1111-1111-1111-111111111111:1-20";`

I get tbelow in the logs.

```
2024-02-21T21:21:14.075552Z 9 [System] [MY-010916] [Server] @@GLOBAL.GTID_PURGED was changed from '' to '00016745-1111-1111-1111-111111111111:1-20'.
2024-02-21T21:21:14.075644Z 9 [System] [MY-010917] [Server] @@GLOBAL.GTID_EXECUTED was changed from '' to '00016745-1111-1111-1111-111111111111:1-20'.
```

^^ in:
- https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/share/messages_to_clients.txt#L6974
- https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/share/messages_to_error_log.txt#L2807

ER_GTID_PURGED_WAS_UPDATED and ER_GTID_EXECUTED_WAS_UPDATED in:
- https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/sys_vars.cc#L6547

...

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

...

`MYSQL_BIN_LOG::reset_logs`: https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/binlog.cc#L5634

`Gtid_state::clear`: https://github.com/jfg956/mysql-server/blob/mysql-8.3.0/sql/rpl_gtid_state.cc#L57

...
