
<!-- 6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 -->

I would like to implement InnoDB Read IO Tail Latenct Monitoring.

When running MySQL on AWS EBS, GCP PV, or other complex network block device
back-end, MySQL performance can degrade if there are increased tail latencies on
read IOs because of an impaired network block device.  In an HA environment, such
degraded MySQL instance would be replaced (failover to a replica if impaired
primary, and stop sending reads to it if impaired replica).  However, detecting
such impaired MySQL is not easy.  I want to make this easier by adding "InnoDB
Read IO Tail Latenct Monitoring" to MySQL.

The idea is to add a threshold (global variable) and all Read IOs longer than it
(suggested name `innodb_io_read_slow_threshold`) would increment a counter.
This counter could be a global status or an InnoDB Metric.

- https://dev.mysql.com/doc/refman/9.0/en/server-status-variables.html

- https://dev.mysql.com/doc/refman/9.0/en/innodb-information-schema-metrics-table.html

InnoDB Metric query:
```
select * from INFORMATION_SCHEMA.INNODB_METRICS\G
```

Example of metrics related to IOs:
```
           NAME: buffer_page_read_index_leaf
      SUBSYSTEM: buffer_page_io
           TYPE: counter
        COMMENT: Number of Index Leaf Pages read
```

...

