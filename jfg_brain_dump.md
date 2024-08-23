
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
- ^^ ends here: https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err_8.0.39_restart_1m_info#L599

The "validating" part is called [InnoDB] Tablespace Path Validation (I use
Path Validation for short):
- https://dev.mysql.com/doc/refman/8.0/en/innodb-disabling-tablespace-path-validation.html

There is a way to disable Path Validation:
- `innodb_validate_tablespace_paths = OFF`
- link to ^^: https://dev.mysql.com/doc/refman/8.0/en/innodb-parameters.html#sysvar_innodb_validate_tablespace_paths

Info logging with Path Validation disabled:
- https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err_8.0.39_restart_1m_info_wo_valid

And even in above, we have a sub-delay of d2, let's call it delay #2.1 (d2.1):
- in "Reading DD tablespace files"
- from 17:22:29 to 17:23:18
- direct link: https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err_8.0.39_restart_1m_info_wo_valid#L592

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Unrelated, weird crash messages:
- https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err_8.0.39_restart_1m_info#L605

Bug report for above: https://bugs.mysql.com/bug.php?id=115886

While doing startup tests with 8.0.39 and 1M tables on a 4 GB vm, oom !
More about this in the section [RAM Consumption and Many Tables](#ram-consumption-and-many-tables}.

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

### Historical Notes

#### History of Duplicate Check

...

https://bugs.mysql.com/bug.php?id=96340

in ^^:
- [13 Aug 2019 17:33] Sunny Bains: The 5.7 startup can be faster because 8.x reverts the 5.7 changes where we wrote the open file descriptors in  the redo log on checkpoint. The scheme introduced in 5.7 was very buggy and it has a runtime cost (and lots of edge case bugs during recovery) and if you have lots of tables open and written to since the last checkpoint well good luck.

Parallel heuristic from 50k to 8k tables...
- in 8.0.19: Bug #30108154, Bug #96340
- https://dev.mysql.com/doc/relnotes/mysql/8.0/en/news-8-0-19.html
- ...

...

https://bugs.mysql.com/bug.php?id=103271

in ^^:
- Reading DD tablespace files / Thread# 16 - Validated 10/10  tablespaces !

...


#### History of Path Validation

`innodb_validate_tablespace_paths` introduced in 8.0.21:
- https://dev.mysql.com/doc/refman/8.0/en/innodb-parameters.html#sysvar_innodb_validate_tablespace_paths

In 8.0.24, from no validation to validating undo and loading tablespaces:
- https://github.com/mysql/mysql-server/commit/eef88fb2565a0fd9d9b123ce4c7c969f678f6831

In 8.0.36, adding if Change Buffer not empty:
- https://github.com/mysql/mysql-server/commit/2646b4bfc100616ba48712dfc15ca9038baf274e

Crash if more than 8k tables of 8.0.38, 8.4.1 and 9.0.0:
- https://github.com/mysql/mysql-server/commit/28eb1ff112777406cd6587231341b9b47167f9f1
- caused by using `current_thd` in a sub-thread in `storage/innobase/handler/ha_innodb.cc`.

Fix crash:
- https://github.com/mysql/mysql-server/commit/19ff2707c448efc6b85429b42e5eabe242ddd0a6

Making multi-threading actually working:
- https://github.com/mysql/mysql-server/commit/8bc6454bfd8fc676aa047332b6c41c76a89c4357

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Other Notes

...

#### RAM Consumption and Many Tables

Moved to `~/Documents/tech/mysql/2024-08_ram_consumption_and_many_tables/notes.txt`.


#### Modified files:

```
fs="$(echo storage/innobase/{fil/fil0fil.cc,handler/ha_innodb.cc,include/srv0srv.h,srv/srv0srv.cc})"

```

...


<!-- EOF -->
