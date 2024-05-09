<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

We would like to implement (not fix, because it is a feature request)
[Bug#106645](https://bugs.mysql.com/bug.php?id=106645):
Slow query log is not logging database/schema name.

...

write_slow:
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/log.cc#L349

File_query_log::write_slow:
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/log.cc#L688

"fill database field":
- https://github.com/jfg956/mysql-server/blob/mysql-8.4.0/sql/log.cc#L1095

...

