
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

...

... Internal Slack:
- https://aiven-io.slack.com/archives/C01U0HU5UQZ/p1712852364336069?thread_ts=1712690974.151329&cid=C01U0HU5UQZ

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


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

### Other Notes

...

#### Modified files:

```
fs="$(echo storage/innobase/{fil/fil0fil.cc,handler/ha_innodb.cc,include/srv0srv.h,srv/srv0srv.cc})"

```

...


<!-- EOF -->
