
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

Main brain dump in parent branch:
- https://github.com/jfg956/mysql-server/blob/mysql-8.3.0_bug113598/jfg_brain_dump.md

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### Tests

```
export mysql_v=8.0.36
export gtid='-c gtid_mode=ON -c enforce-gtid-consistency -c relay-log-recovery=on'

(

export bin_dir="$HOME/opt/mysql/mysql_$mysql_v"
build_dir="$HOME/src/github/https/jfg956/mysql-server/worktrees/mysql-${mysql_v}_bug113598/build/default"
sb_dir="$HOME/sandboxes/rsandbox_mysql_${mysql_v//./_}"

f=$bin_dir/bin/mysqld
test -e ${f}_org || mv ${f}{,_org}
cp $build_dir/bin/mysqld ${f}_bug113598

f=$bin_dir/lib/plugin/semisync_master.so
test -e ${f}_org || mv ${f}{,_org}
cp $build_dir/plugin_output_directory/semisync_master.so ${f}_bug113598

function set_v() {
  ( cd $bin_dir/bin; rm -f mysqld; ln -s mysqld_$1 mysqld; )
  ( cd $bin_dir/lib/plugin; rm -f semisync_master.so; ln -s semisync_master.so_$1 semisync_master.so; )
}

function create_sb() {
  cd; rm -rf $sb_dir; set_v org
  dbdeployer deploy replication mysql_$mysql_v $1 --semi-sync > /dev/null
  cd $sb_dir; ./stop_all > /dev/null
}

create_sb "$gtid"

function run_test() {
  ./start_all > /dev/null
  test "$1" == "" || ./m <<< "set global rpl_semi_sync_master_log_gtid_timeout = $1"
  ./m <<< "DROP DATABASE IF EXISTS test_jfg"
  ( ./node1/stop > /dev/null& ./node2/stop >/dev/null& wait; )
  ./m <<< "SET GLOBAL rpl_semi_sync_master_timeout = 1000"
  local sql="SELECT now(), @@GLOBAL.gtid_executed"
  ./m -N <<< "$sql; CREATE DATABASE test_jfg; $sql"
  sleep 1; grep -e "Timeout waiting for reply of binlog" master/data/msandbox.err | tail -n 1
  ./stop_all > /dev/null
}

set_v org;       echo; echo "# org with GTID:";                        run_test "";
set_v bug113598; echo; echo "# bug113598 with GTID logging disabled:"; run_test OFF
set_v bug113598; echo; echo "# bug113598 with GTID logging enabled:";  run_test ON

create_sb

set_v org;       echo; echo "# org without GTID:";                        run_test ""
set_v bug113598; echo; echo "# bug113598 without GTID logging disabled:"; run_test OFF
set_v bug113598; echo; echo "# bug113598 without GTID logging enabled:";  run_test ON

set_v org; rm -rf $sb_dir

)

# org with GTID:
2024-04-11 21:27:36     00022637-1111-1111-1111-111111111111:1-34
2024-04-11 21:27:37     00022637-1111-1111-1111-111111111111:1-35
2024-04-11T21:27:37.256692Z 12 [Warning] [MY-011153] [Repl] Timeout waiting for reply of binlog (file: mysql-bin.000002, pos: 588), semi-sync up to file mysql-bin.000002, position 391.

# bug113598 with GTID logging disabled:
2024-04-11 21:28:00     00022637-1111-1111-1111-111111111111:1-36
2024-04-11 21:28:01     00022637-1111-1111-1111-111111111111:1-37
2024-04-11T21:28:01.595961Z 13 [Warning] [MY-011153] [Repl] Timeout waiting for reply of binlog (file: mysql-bin.000003, pos: 597), semi-sync up to file mysql-bin.000003, position 400.

# bug113598 with GTID logging enabled:
2024-04-11 21:28:23     00022637-1111-1111-1111-111111111111:1-38
2024-04-11 21:28:24     00022637-1111-1111-1111-111111111111:1-39
2024-04-11T21:28:24.924644Z 13 [Warning] [MY-014071] [Repl] Timeout waiting for reply of binlog (file: mysql-bin.000004, pos: 597, gtid: 00022637-1111-1111-1111-111111111111:39), semi-sync up to file mysql-bin.000004, position 400.

# org without GTID:
2024-04-11 21:30:16
2024-04-11 21:30:17
2024-04-11T21:30:17.501782Z 12 [Warning] [MY-011153] [Repl] Timeout waiting for reply of binlog (file: mysql-bin.000002, pos: 548), semi-sync up to file mysql-bin.000002, position 351.

# bug113598 without GTID logging disabled:
2024-04-11 21:30:39
2024-04-11 21:30:40
2024-04-11T21:30:40.833339Z 13 [Warning] [MY-011153] [Repl] Timeout waiting for reply of binlog (file: mysql-bin.000003, pos: 557), semi-sync up to file mysql-bin.000003, position 360.

# bug113598 without GTID logging enbled:
2024-04-11 21:31:02
2024-04-11 21:31:03
2024-04-11T21:31:03.584208Z 13 [Warning] [MY-014071] [Repl] Timeout waiting for reply of binlog (file: mysql-bin.000004, pos: 557, gtid: ANONYMOUS), semi-sync up to file mysql-bin.000004, position 360.
```


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
<!-- EOF -->
