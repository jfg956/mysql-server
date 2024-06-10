
let $_INTERNAL_MYSQLD_DATADIR = `select @@datadir`;
let $_INTERNAL_MYSQLD_SLOW_QUERY_LOG_FILE = `select @@slow_query_log_file`;

--echo
--echo ## Resetting the slow query log file.
--remove_file $MYSQLD_DATADIR/$_INTERNAL_MYSQLD_SLOW_QUERY_LOG_FILE
FLUSH SLOW LOGS;
