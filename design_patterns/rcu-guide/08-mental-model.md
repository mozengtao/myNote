# Final Mental Model

RCU provides lock-free reading for read-heavy structures. Readers use rcu_read_lock, writers must wait for all readers before freeing old data.

**中文总结：**

RCU提供无锁读取。读者用rcu_read_lock，写者必须等所有读者完成才能释放旧数据。

## Version

Based on Linux kernel v3.2.
