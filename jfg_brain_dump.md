
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

