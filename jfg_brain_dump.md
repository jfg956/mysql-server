
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

...

... DBA Slack message:
- https://dbachat.slack.com/archives/C027R4PCV/p1723215439235329

... enforce_gtid_consistency:
- https://dev.mysql.com/doc/refman/8.4/en/replication-options-gtids.html#sysvar_enforce_gtid_consistency

... Replication Compatibility Between MySQL Versions:
- 8.0: https://dev.mysql.com/doc/refman/8.0/en/replication-compatibility.html
- 8.4: https://dev.mysql.com/doc/refman/8.4/en/replication-compatibility.html
- 9.0: https://dev.mysql.com/doc/refman/9.0/en/replication-compatibility.html

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Code Notes

...

Vector in 9.0.0 is WL #16081, matching commit, some significant files:
- https://github.com/jfg956/mysql-server/commit/8cd51511de7db36971954326af6d10eb7ac5476c
- `sql/create_field.cc`: https://github.com/jfg956/mysql-server/commit/8cd51511de7db36971954326af6d10eb7ac5476c#diff-062276b39881595abb6408d5e3a3b349a3153e2b927fbe2aef8f10edb0716d58
- `sql/item_create.cc`: https://github.com/jfg956/mysql-server/commit/8cd51511de7db36971954326af6d10eb7ac5476c#diff-6acfef85770840dcf22f62758e4451441e28958601d7dcf9af17df72ccd9e838
- `sql/lex.h`: https://github.com/jfg956/mysql-server/commit/8cd51511de7db36971954326af6d10eb7ac5476c#diff-2486c00e4dfc245762ed8143b2a9eb08d0471defd2da3ba99f26d7310dc01501
- `sql/server_component/mysql_stored_program_imp.cc`: https://github.com/jfg956/mysql-server/commit/8cd51511de7db36971954326af6d10eb7ac5476c#diff-b91d093cae227a055dfb06a22bd361fe182cfb016ff81484df3c77c33ba9da76
- `sql/sql_table.cc`: https://github.com/jfg956/mysql-server/commit/8cd51511de7db36971954326af6d10eb7ac5476c#diff-f223b918b8e982bb3edaed26dc567ac653c0cf35f5ca624e2e3b664d4be5d49d

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### First patch...

...

Probably many things missing, including view and stored proc, but first shot !

```
mysql [localhost:9001] {msandbox} ((none)) > show global variables like 'enforce_replication_compatibility_previous_major_version';
+----------------------------------------------------------+-------+
| Variable_name                                            | Value |
+----------------------------------------------------------+-------+
| enforce_replication_compatibility_previous_major_version | OFF   |
+----------------------------------------------------------+-------+
1 row in set (0.02 sec)

mysql [localhost:9001] {msandbox} ((none)) > creatE TABLE test_jfg.t2(id int, v vector);
Query OK, 0 rows affected (0.09 sec)

mysql [localhost:9001] {msandbox} ((none)) > set global enforce_replication_compatibility_previous_major_version = true;
Query OK, 0 rows affected (0.00 sec)

mysql [localhost:9001] {msandbox} ((none)) > creatE TABLE test_jfg.t3(id int, v vector);
ERROR 4167 (HY000): the 'vector' feature is disabled when enforce_replication_compatibility_previous_major_version = TRUE.
mysql [localhost:9001] {msandbox} ((none)) > creatE TABLE test_jfg.t3 like test_jfg.t2;
ERROR 4167 (HY000): the 'vector' feature is disabled when enforce_replication_compatibility_previous_major_version = TRUE.
mysql [localhost:9001] {msandbox} ((none)) > show global variables like 'enforce_replication_compatibility_previous_major_version';
+----------------------------------------------------------+-------+
| Variable_name                                            | Value |
+----------------------------------------------------------+-------+
| enforce_replication_compatibility_previous_major_version | ON    |
+----------------------------------------------------------+-------+
1 row in set (0.00 sec)
```

TODO: s/TRUE/ON/ in error message...

...


<!-- EOF -->

