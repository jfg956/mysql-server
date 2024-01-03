
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

The initial idea was to fix [Bug#109200](https://bugs.mysql.com/bug.php?id=109200).

This is pre-requisite work for another project: LRU flushing improvements.

But this ended-up being a bigger rabbit hole than initially thought,
full brain dump below.


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### MySQL 8.0 contains the bug

Link to `buf_LRU_free_from_common_LRU_list` of 8.0.35:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0lru.cc#L1094

Because initial value of `scanned` (`ulint scanned{}`), if a page is freed in
the 1st iteration of the for-loop, we are skipping `++scanned` (because
of the `if (freed) break`) so we are not reaching
`MONITOR_INC_VALUE_CUMULATIVE` (because it is in a `if (scanned)`.


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### MySQL 5.6 might have another bug

Link to `buf_LRU_free_from_common_LRU_list` of 5.6.51:
- https://github.com/jfg956/mysql-server/blob/mysql-5.6.51/storage/innobase/buf/buf0lru.cc#L1002

If a page is freed in the first iteration of the loop, `scanned` will be 2 when
exiting the loop (because `scanned = 1` and `++scanned`).  So instead of
incrementing by 1, we increment by 2.

(Above is from code interpretation, to be confirmed by a test.)


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### MySQL 5.7 looks fine

Link to `buf_LRU_free_from_common_LRU_list` of 5.7.44:
- https://github.com/jfg956/mysql-server/blob/mysql-5.7.44/storage/innobase/buf/buf0lru.cc#L1054

Initialized at 0, will get `++scanned`, so 1 as expected.

...BUF_LRU_SEARCH_SCAN_THRESHOLD...


### The problem in 8.0 is `if (freed) break;`

...


### WL#8423 introduces `if (freed) break;`...

In below:
- go to storage/innobase/buf/buf0lru.cc
- search for buf_LRU_free_from_common_LRU_list

https://github.com/jfg956/mysql-server/commit/2bcc00d11f21fe43ba3c0e0f81d3d9cec44c44a0

Link to WL#8423:
- https://dev.mysql.com/worklog/task/?id=8423

...TODO: get a pdf of this WL...

Link to `if (freed) break;`:
- https://github.com/jfg956/mysql-server/blob/2bcc00d11f21fe43ba3c0e0f81d3d9cec44c44a0/storage/innobase/buf/buf0lru.cc#L1127

This `...break;` construct is weird, because the for-loop condition includes
`!freed`...

Link to `!freed`:
- https://github.com/jfg956/mysql-server/blob/2bcc00d11f21fe43ba3c0e0f81d3d9cec44c44a0/storage/innobase/buf/buf0lru.cc#L1094C10-L1094C16

...I can only guess, because no explaination in the code, that it is to avoid
the `bpage = ...` part of the for-loop...

Link to `bpage = ...`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0lru.cc#L1104

...but this `break` also bypass the `++scanned`, so it needs to be added there.


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### MySQL 5.7 introduces `BUF_LRU_SEARCH_SCAN_THRESHOLD`

...

https://dev.mysql.com/worklog/task/?id=7047

https://github.com/mysql/mysql-server/commit/6ef8c343445a26aaf9ebd76d72cf57db44b481f5

...

<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
<!-- EOF -->
