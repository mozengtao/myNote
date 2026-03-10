- command file
```
cmd1 arg1
cmd2 arg1 arg2
```

- file names saved in a file
```
name1
name2
```

## paste + read
```bash
# paste 命令默认使用 \t 作为列之间的分隔符
paste cmd.txt filename.txt | while IFS="$(printf '\t')" read -r cmd filename; do
    echo "Running $cmd and saving the output to $filename"
done

paste cmd.txt filename.txt | while IFS="$(printf '\t')" read -r cmd filename; do
    ./exec.sh "$cmd" < /dev/null > "$filename"
done

<<EOF
Problem: When a script inside a while read loop reads from stdin, it steals the piped input meant for the loop.

paste output -> while read (gets line 1)
                    -> vmc_confd_cli.sh reads stdin (steals lines 2, 3, ...)
                -> while read (nothing left!)

Fix: < /dev/null blocks the inner script from reading the loop's stdin.
EOF
```