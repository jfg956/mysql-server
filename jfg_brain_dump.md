
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

...

... Internal Slack:
- https://aiven-io.slack.com/archives/C01U0HU5UQZ/p1712852364336069?thread_ts=1712690974.151329&cid=C01U0HU5UQZ

...

MySQL 8.0.39 with 1M tables takes 0:13:57 to start (it remember correctly, this
is on a default gp3 EBS volume --> 3k iops and 125 mbps).

Below is the log with `log_error_verbosity = 3` (I will call this info logging
in this doc; I do not have one with 2 as I am writting this):
- https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err_8.0.39_restart_1m_info

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

I ran below command to simulate `log_error_verbosity = 2` (I will call this warning
logging in this doc), and got the below:
- `cat ./msandbox.err_8.0.39_restart_1m_info | awk '$3 != "[Note]"'`
- https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err_8.0.39_restart_1m_info_notes_filtered

In filtered above, we have:
```
2024-08-04T16:56:14.149844Z mysqld_safe Logging to '/home/jgagne/sandboxes/msb_mysql_8_0_39/data/msandbox.err'.
[...]
2024-08-04T16:56:18.716424Z 1 [System] [MY-013576] [InnoDB] InnoDB initialization has started.
2024-08-04T17:08:12.882285Z 1 [System] [MY-013577] [InnoDB] InnoDB initialization has ended.
2024-08-04T17:10:09.190339Z 0 [Warning] [MY-010068] [Server] CA certificate ca.pem is self signed.
[...]
2024-08-04T17:10:09.259879Z 0 [System] [MY-010931] [Server] /home/jgagne/opt/mysql/mysql_8.0.39/bin/mysqld: ready for connections. [...]
```

In above, we have 2 main delays:
- from 16:56:18 to 17:08:12: let's call this delay #1 (d1),
- from 17:08:12 to 17:10:09: let's call this delay #2 (d2).

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

About d1, in info logging:
- it starts here: https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err_8.0.39_restart_1m_info#L20
- it ends here: https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err_8.0.39_restart_1m_info#L563C18-L563C21

Let's call d1 InnoDB Tablespace Duplicate Check (Duplicate Check for short).

About d2, in info logging, it is more complicated:
- from 17:08:13 to 17:09:11: "Reading DD tablespace files",
- ^^: https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err_8.0.39_restart_1m_info#L592
- from 17:09:11 to 17:10:04: validating tablespaces,
- ^^ starts here: https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err_8.0.39_restart_1m_info#L593
- ^^ ends here: https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err_8.0.39_restart_1m_info#L599C12-L599C20

The "validating" part is called [InnoDB] Tablespace Path Validation (I use
Path Validation for short):
- https://dev.mysql.com/doc/refman/8.0/en/innodb-disabling-tablespace-path-validation.html

There is a way to disable Path Validation:
- `innodb_validate_tablespace_paths = OFF`
- link to ^^: https://dev.mysql.com/doc/refman/8.0/en/innodb-parameters.html#sysvar_innodb_validate_tablespace_paths

Info logging with Path Validation disabled:
- ...

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Code Notes

...

In `Tablespace_dirs::scan`, `size_t n_threads = fil_get_scan_threads(ibd_files.size())`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/storage/innobase/fil/fil0fil.cc#L11449

"scan threads" are calling `duplicate_check`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/storage/innobase/fil/fil0fil.cc#L11471

`par_for`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/storage/innobase/include/os0thread-create.h#L330

...

Tablespace Path Validation, `Validate_files::validate`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/storage/innobase/handler/ha_innodb.cc#L3787

^^ call `Validate_files::check` via par_for:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/storage/innobase/handler/ha_innodb.cc#L3425

`Validate_files::validate` called here:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/storage/innobase/handler/ha_innodb.cc#L3879

"Reading DD tablespace files", which can take time, in this:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/storage/innobase/handler/ha_innodb.cc#L3872

^^ calls `Dictionary_client::fetch_global_components`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/sql/dd/impl/cache/dictionary_client.cc#L2250

^^ calls `Dictionary_client::fetch`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/sql/dd/impl/cache/dictionary_client.cc#L2033

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Other Notes

...

#### Modified files:

```
fs="$(echo storage/innobase/{fil/fil0fil.cc,handler/ha_innodb.cc,include/srv0srv.h,srv/srv0srv.cc})"

```

...


<!-- EOF -->
