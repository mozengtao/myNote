> look up or signal processes based on name and other

```bash
# 1
root:/tmp [P2]# pgrep cms
3116

# 2
admin@localhost:~$ pgrep -af "135.242.60.169"
1435590 ssh -CNfR 58080:127.0.0.1:3128 morrism@135.242.60.169
```

[man pgrep](https://linux.die.net/man/1/pgrep)  