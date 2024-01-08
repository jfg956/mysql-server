
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

The initial idea was to fix [Bug#109200](https://bugs.mysql.com/bug.php?id=109200).

This is pre-requisite work for another project: LRU flushing improvements.

But this ended-up being a bigger rabbit hole than initially thought,
full brain dump in the rest of this file, TL&DR:
- I initially thought 5.6 was not affected, this was not fully true;
- I initially thought 5.7 was affected, this was wrong;
- I found the work that introduced the bug in 8.0, classic ommittion error;
- but at the same time, I saw a problem introduced in 5.7 (`BUF_LRU_SEARCH_SCAN_THRESHOLD`);
- ...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### Reminder of the bug

...

https://bugs.mysql.com/bug.php?id=109200

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### MySQL 8.0 contains the bug

Link to `buf_LRU_free_from_common_LRU_list` of 8.0.35:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0lru.cc#L1094

Because initial value of `scanned` (`ulint scanned{}`), if a page is freed in
the 1st iteration of the for-loop, we are skipping `++scanned` (because
of the `if (freed) break`) so we are not reaching
`MONITOR_INC_VALUE_CUMULATIVE` (because it is in a `if (scanned)` with `scanned`
not having been incremented).


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### MySQL 5.6 might have another bug

Link to `buf_LRU_free_from_common_LRU_list` of 5.6.51:
- https://github.com/jfg956/mysql-server/blob/mysql-5.6.51/storage/innobase/buf/buf0lru.cc#L1002

If a page is freed in the first iteration of the loop, `scanned` will be 2 when
exiting the loop (because initialized as
`scanned = 1` and `++scanned` before the for-loop ends).  So instead of
incrementing by 1, we increment by 2, which reports the wrong metric.

(Above is from code interpretation, to be confirmed by a test.)


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### MySQL 5.7 looks fine

Link to `buf_LRU_free_from_common_LRU_list` of 5.7.44:
- https://github.com/jfg956/mysql-server/blob/mysql-5.7.44/storage/innobase/buf/buf0lru.cc#L1054

The `scanned` variable is initialized at 0, runs `++scanned` before the for-loop
ends, so 1 as expected which enters the `if (scanned)`.

...BUF_LRU_SEARCH_SCAN_THRESHOLD...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### The problem in 8.0 is `if (freed) break;`

So, the problem is not in 5.7, but it is in 8.0.  What is the difference ?

Link to `buf_LRU_free_from_common_LRU_list` of 5.7.44:
- https://github.com/jfg956/mysql-server/blob/mysql-5.7.44/storage/innobase/buf/buf0lru.cc#L1054

Link to `buf_LRU_free_from_common_LRU_list` of 8.0.35:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0lru.cc#L1094

The difference if the `if (freed) break` (simplified), exact link in 8.0.35:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0lru.cc#L1125

WL#8423 introduces `if (freed) break`.  Getting there is a little convoluted,
- in https://github.com/jfg956/mysql-server/commit/2bcc00d11f21fe43ba3c0e0f81d3d9cec44c44a0...
- go to storage/innobase/buf/buf0lru.cc...
- search for buf_LRU_free_from_common_LRU_list...
- and you will see `if (freed) break` added by above commit.

Link to WL#8423:
- https://dev.mysql.com/worklog/task/?id=8423

...TODO: get a pdf of this WL...

Link to `if (freed) break` in WL#8423 commit:
- https://github.com/jfg956/mysql-server/blob/2bcc00d11f21fe43ba3c0e0f81d3d9cec44c44a0/storage/innobase/buf/buf0lru.cc#L1127

This `...break;` construct is weird, because the for-loop condition includes
`!freed`...

Link to `!freed` in WL#8423 commit:
- https://github.com/jfg956/mysql-server/blob/2bcc00d11f21fe43ba3c0e0f81d3d9cec44c44a0/storage/innobase/buf/buf0lru.cc#L1094C10-L1094C16

...I can only guess, because no explaination in the code, that it is to avoid
the `bpage = ...` part of the for-loop...

Link to `bpage = ...`:
- https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0lru.cc#L1104

...but this `break` also bypass the `++scanned` part of the for-loop.  So a fix to
Bug#109200 is to add `++scanned` in the `if (freed)`.


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### MySQL 5.7 introduces `BUF_LRU_SEARCH_SCAN_THRESHOLD`

...

https://dev.mysql.com/worklog/task/?id=7047

https://github.com/mysql/mysql-server/commit/6ef8c343445a26aaf9ebd76d72cf57db44b481f5

...


### MySQL does not refill the free list unless...

...

Valid for 5.6, 5.7 and 8.0...

To test for 8.2...

Emptying the free list with a SELECT does not trigger a refill...

The next write will trigger a refill !

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
### `MONITOR_LRU` InnoDB Metrics Code Paths

...

<pre>
...

<a href="https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0lru.cc#L1156">buf_LRU_scan_and_free_block<a>
 +-> <a href="https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0lru.cc#L1046">buf_LRU_free_from_unzip_LRU_list<a>
 |    \-> <a href="https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0lru.cc#L1080">MONITOR_LRU_UNZIP_SEARCH_SCANNED<a>
 +-> <a href="https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0lru.cc#L1094">buf_LRU_free_from_common_LRU_list<a>
      \-> <a href="https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0lru.cc#L1145">MONITOR_LRU_SEARCH_SCANNED<a>

<a href="https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0flu.cc#L2156">buf_flush_single_page_from_LRU<a>
 \-> MONITOR_LRU_SINGLE_FLUSH_SCANNED


Below thread created in <a href="https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0flu.cc#L2826C6-L2826C33">buf_flush_page_cleaner_init</a>.

<a href="https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0flu.cc#L3179">buf_flush_page_coordinator_thread</a>
 +-> create more page cleaner threads which are calling buf_flush_page_cleaner_thread.
 +-> pc_flush_slot

<a href="https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0flu.cc#L3602">buf_flush_page_cleaner_thread</a>
 +-> pc_flush_slot

<a href="https://github.com/jfg956/mysql-server/blob/mysql-8.0.35/storage/innobase/buf/buf0flu.cc#L2930">pc_flush_slot</a>
 +-> ...

<a href="">
...
</pre>

...


<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->
<!-- EOF -->
