
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

...

... Internal Slack:
- https://aiven-io.slack.com/archives/C01U0HU5UQZ/p1712852364336069?thread_ts=1712690974.151329&cid=C01U0HU5UQZ

...

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Analysis

#### V1 - Old and Partial

(Old because probably on t3a, and partical because warn log extrapolated from info)

(This analysis defines 3 names:)
- InnoDB Tablespace Duplicate Check ([Duplicate Check](#name_duplicate_check) for short);
- [InnoDB] Tablespace Path Validation (I use [Path Validation](#name_path_validation) for short);
- Data Dictionnary Tablespace File Reading ([DD Reading](name_dd_reading) for short).

MySQL 8.0.39 with 1M tables takes 0:13:57 to start (it remember correctly, this
is on a default gp3 EBS volume --> 3k iops and 125 mbps).

(Note to delf from the future: probably on a t3a, because on a m6i and gp3
3 kiops and 125 MB/s, I see startup in 0:07:10 for 2 vcup and 0:06:53 with 4)

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

<a name="name_duplicate_check"></a>
Let's call d1 InnoDB Tablespace Duplicate Check (Duplicate Check for short).

About d2, in info logging, it is more complicated:
- from 17:08:13 to 17:09:11: "Reading DD tablespace files",
- ^^: https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err_8.0.39_restart_1m_info#L592
- from 17:09:11 to 17:10:04: validating tablespaces,
- ^^ starts here: https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err_8.0.39_restart_1m_info#L593
- ^^ ends here: https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err_8.0.39_restart_1m_info#L599

<a name="name_path_validation"></a>
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

<a name="name_dd_reading"></a>
Let's call d2.1 Data Dictionnary Tablespace File Reading (DD Reading for short).

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Unrelated, weird crash messages:
- https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err_8.0.39_restart_1m_info#L605

Bug report for above: https://bugs.mysql.com/bug.php?id=115886

While doing startup tests with 8.0.39 and 1M tables on a 1vcup & 4 GB vm, oom !
More about this in the section [RAM Consumption and Many Tables](#ram-consumption-and-many-tables).

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### V2 - More Recent

...

https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/results_m6i_gp3_3kiops_125mbps.txt

https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/results_m6i_gp3_12kiops_500mbps.txt

https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/results_local_ssd.txt

...

```
cat explo_files/results_m6i_gp3_3kiops_125mbps.txt |
  grep "[89].0.*1000000" |
  grep -e default -e info -e no_val |
  grep -v resto
 8.0.39 1000000        2.default      start: 0:07:10
 8.0.39 1000000           2.info      start: 0:07:11
 8.0.39 1000000         2.no_val      start: 0:06:32
  9.0.1 1000000        2.default      start: 0:07:09
  9.0.1 1000000           2.info      start: 0:07:09
  9.0.1 1000000         2.no_val      start: 0:06:32
 8.0.39 1000000        4.default      start: 0:06:53
 8.0.39 1000000           4.info      start: 0:06:53
 8.0.39 1000000         4.no_val      start: 0:06:29
  9.0.1 1000000        4.default      start: 0:06:52
  9.0.1 1000000           4.info      start: 0:06:52
  9.0.1 1000000         4.no_val      start: 0:06:30

# 12kiops_500mbps.
 8.0.39 1000000        2.default      start: 0:04:07
 8.0.39 1000000           2.info      start: 0:04:04
 8.0.39 1000000         2.no_val      start: 0:03:31
  9.0.1 1000000        2.default      start: 0:04:05
  9.0.1 1000000           2.info      start: 0:04:07
  9.0.1 1000000         2.no_val      start: 0:03:32
 8.0.39 1000000        4.default      start: 0:02:42
 8.0.39 1000000           4.info      start: 0:02:43
 8.0.39 1000000         4.no_val      start: 0:02:19
  9.0.1 1000000        4.default      start: 0:02:42
  9.0.1 1000000           4.info      start: 0:02:44
  9.0.1 1000000         4.no_val      start: 0:02:19

# ssd (not enough disk space for 1M with 2 vcup).
 8.0.39 1000000        4.default      start: 0:02:46
 8.0.39 1000000           4.info      start: 0:02:46
 8.0.39 1000000         4.no_val      start: 0:02:22
  9.0.1 1000000        4.default      start: 0:02:47
  9.0.1 1000000           4.info      start: 0:02:48
  9.0.1 1000000         4.no_val      start: 0:02:25
```

https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err.8.0.39.1000000.m6i_gp3_3kiops_125mbps.2.default

https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err.8.0.39.1000000.m6i_gp3_3kiops_125mbps.2.info

https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err.8.0.39.1000000.m6i_gp3_3kiops_125mbps.2.no_val

...

https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err.8.0.39.1000000.m6i_gp3_3kiops_125mbps.4.default

https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err.8.0.39.1000000.m6i_gp3_3kiops_125mbps.4.info

https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err.8.0.39.1000000.m6i_gp3_3kiops_125mbps.4.no_val

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Code Notes

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### Duplicate Check

In `Tablespace_dirs::scan`, `size_t n_threads = fil_get_scan_threads(ibd_files.size())`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/storage/innobase/fil/fil0fil.cc#L11449

"scan threads" are calling `duplicate_check`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/storage/innobase/fil/fil0fil.cc#L11471

`par_for`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/storage/innobase/include/os0thread-create.h#L330

...

Duplicate Check fills a data structure for Tablespace Path Validation:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/fil/fil0fil.cc#L11059

...m_dirs is a "Scanned":
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/fil/fil0fil.cc#L635

...a Scanned is vector of Tablespace_files...
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/fil/fil0fil.cc#L601

...Tablespace_files has a add(space_id_t, std::string)...
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/fil/fil0fil.cc#L2141

...^^ add feeds a Paths (std::unordered_map<space_id_t, Names>)

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### Tablespace Validation

Tablespace Path Validation, `Validate_files::validate`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/storage/innobase/handler/ha_innodb.cc#L3787

^^ call `Validate_files::check` via par_for:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/storage/innobase/handler/ha_innodb.cc#L3425

`Validate_files::validate` called here:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/storage/innobase/handler/ha_innodb.cc#L3879

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### Tablespace Reading

"Reading DD tablespace files", which can take time, in this:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/storage/innobase/handler/ha_innodb.cc#L3872

^^ calls `Dictionary_client::fetch_global_components`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/sql/dd/impl/cache/dictionary_client.cc#L2250

^^ calls `Dictionary_client::fetch`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.39/sql/dd/impl/cache/dictionary_client.cc#L2033

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

#### Other

IMPLEMENTATION OF THE TABLESPACE MEMORY CACHE:
- https://github.com/jfg956/mysql-server/blob/mysql-9.0.1/storage/innobase/fil/fil0fil.cc#L225

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
- https://dev.mysql.com/doc/relnotes/mysql/8.0/en/news-8-0-39.html
- https://dev.mysql.com/doc/relnotes/mysql/8.4/en/news-8-4-2.html
- https://dev.mysql.com/doc/relnotes/mysql/9.0/en/news-9-0-1.html
- InnoDB: Improved tablespace file scan performance at startup. (Bug #110402, Bug #35200385)
- https://github.com/mysql/mysql-server/commit/8bc6454bfd8fc676aa047332b6c41c76a89c4357

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Fadvise and Light Optimizations

...

https://jfg-mysql.blogspot.com/2024/09/blog-post.html

https://bugs.mysql.com/bug.php?id=115988

https://github.com/jfg956/mysql-server/pull/14

...

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Below is a variation of what is in Bug#115988, idea derived from Dan Reif's comment
on LinkedIn, analysis below.
- https://www.linkedin.com/feed/update/urn:li:activity:7237506503144845312?commentUrn=urn%3Ali%3Acomment%3A%28activity%3A7237506503144845312%2C7237536077027139586%29&dashCommentUrn=urn%3Ali%3Afsd_comment%3A%287237536077027139586%2Curn%3Ali%3Aactivity%3A7237506503144845312%29

```
function start() {
  sudo sync; sudo /usr/bin/bash -c "echo 3 > /proc/sys/vm/drop_caches"

  ( ./start > /dev/null & )
  while sleep 1; do test -e data/mysql_sandbox*.pid && break; done | pv -t -N "$(status_string start)"

  ./use <<< "FLUSH ERROR LOGS"  # Because we cp logs after this, let's make sure they are flushed.
  cp data/msandbox.err ../msandbox.err.$mv.$str
}

function stop() {
  local pid=$(cat data/mysql_sandbox*.pid)
  ( ./stop > /dev/null & )
  while sleep 1; do ps -p $pid | grep -q $pid || break; done
}

function status_string() {
  printf "%6s %13s %9s" $mv $str $1
}

# Below function is to swap between binaries.
function set_bin() {
  ( # In a sub-shell to not have to undo cd.
    cd ~/opt/mysql/mysql_$mv/bin
    rm -f mysqld
    ln -s mysqld_$1 mysqld
  )
}

function stop_wipe_restore() {
  stop; rm -rf data
  set_bin $1; str=$2
  pv -te ~/sandboxes/mysql_${mv}.data.${n}.tgz -N "$(status_string restore)" | tar -zx
}

# The actual test: 1M tables with 9.0.1, org and explo.
n=1000000; mv="9.0.1"; {
  set_bin org
  cd; dbdeployer deploy single mysql_$mv -c log_error_verbosity=3 > /dev/null
  cd ~/sandboxes/msb_mysql_${mv//./_}

  stop_wipe_restore org   default_org;   start
  stop_wipe_restore explo default_explo; start

  echo "innodb_tablespace_startup_testing_light=ON" >> my.sandbox.cnf
  stop_wipe_restore explo light; start

  stop; cd; rm -rf ~/sandboxes/msb_mysql_${mv//./_}
  set_bin org
}

 9.0.1   default_org   restore: 0:13:02
 9.0.1   default_org     start: 0:02:46
 9.0.1 default_explo   restore: 0:13:02
 9.0.1 default_explo     start: 0:02:37
 9.0.1         light   restore: 0:13:01
 9.0.1         light     start: 0:01:08
```

https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err.9.0.1.default_explo

https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err.9.0.1.light

```
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>

int main(int argc, char** argv) {
  char buf[1024*4];
  DIR * pdir = opendir(".");

  while (1) {
    struct dirent *pdirent = readdir(pdir);
    if (!pdirent) break;
    if (pdirent->d_name[0] == '.') continue;

    FILE *pf = fopen(pdirent->d_name, "rb");
    posix_fadvise(fileno(pf), 0, 1024*4, POSIX_FADV_RANDOM);
    setvbuf(pf, 0, _IONBF, 0);
    fread(buf, 1, 1024*4, pf);
    fclose(pf);

    printf(".");
  }

  return 0;
}

# ... stop_wipe_restore explo light_cached.
 9.0.1  light_cached   restore: 0:13:02

~/sandboxes/msb_mysql_9_0_1/data/test_jfg$ sudo sync; sudo /usr/bin/bash -c "echo 3 > /proc/sys/vm/drop_caches"
~/sandboxes/msb_mysql_9_0_1/data/test_jfg$ ~/tmp/a.out | pv -tbea -s 1000000 > /dev/null
 976KiB 0:01:30 [10.7KiB/s]
~/sandboxes/msb_mysql_9_0_1/data/test_jfg$ ~/tmp/a.out | pv -tbea -s 1000000 > /dev/null
 976KiB 0:00:02 [ 385KiB/s]

# ... start wo flushing cache.
 9.0.1  light_cached     start: 0:00:49
```

https://github.com/jfg956/mysql-server/blob/8.0.39_explo_startup_many_tables/explo_files/msandbox.err.9.0.1.light_cached

<a name="light_info_error_log_analysis"></a>

(Direct link to here: [link](#light_info_error_log_analysis))

In default_explo, Duplicate Check from 14:55:25 to 14:57:07 --> 1:32 of 2:37.

In light, Duplicate Check from 15:11:45 to 15:11:59 --> 0:14 of 1:08.

In light_cached, Duplicate Check from 16:07:45 to 16:07:48 --> 0:03 of 0:49.

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
