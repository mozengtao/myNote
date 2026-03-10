## setsid, nohup, disown and double-fork daemon
```
| Method           | Detaches from shell? | Removes controlling TTY?    | Avoids SIGHUP?                            | Avoids Ctrl-C?  | Survives logout?                                 | True daemon? |
| ---------------- | -------------------- | --------------------------- | ----------------------------------------  | --------------  | ------------------------------------------------ | ------------ |
| **nohup cmd &**  | ❌ No                 | ❌ No                     | ✅ Yes                                   | ❌ No           | ⚠️ Depends (dies if parent dies under systemd)  | ❌ No         |
| **disown**       | ❌ No                 | ❌ No                     | ❌ No (unless `set -m` rules happen)     | ❌ No           | ⚠️ Usually no                                   | ❌ No         |
| **setsid cmd &** | ✅ Yes                | ✅ Yes                    | ⚠️ Usually yes (TTY-driven SIGHUP gone)  | ✅ Yes          | ⚠️ Often yes; systemd logouts may still kill it | ❌ No         |
| **double-fork**  | ✅ Yes                | ✅ Yes                    | ✅ Yes                                   | ✅ Yes          | ✅ Yes (fully daemonized)                       | ✅ Yes        |
```
## test.sh
```bash
#!/bin/bash
echo "PID=$$ PPID=$PPID PGID=$(ps -o pgid= $$) SID=$(ps -o sid= $$)"
echo "TTY=$(ps -o tty= $$)"
echo "Sleeping..."
sleep 600

#
nohup ./test.sh &

#
./test.sh &
disown %1

#
setsid ./test.sh &
```

## daemonize.c
```c
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

int main() {
    pid_t pid = fork();
    if (pid > 0) exit(0);  // parent exits

    setsid();             // new session

    pid = fork();
    if (pid > 0) exit(0); // session leader exits

    chdir("/");
    umask(0);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    sleep(600);
}
```

## inspect
`ps -o pid,ppid,pgid,sid,tty,cmd -p <pid>`

## Which one should use

- you want a CLI job to survive logout
`setsid cmd &`

- you want full daemon behavior
`use double-fork or systemd service`

- you want simple SIGHUP protection
`nohup cmd &`

- you just don't want the shell to track the job
`disown`


## combined_test.sh
```bash
#!/usr/bin/env bash
#
# Demonstration of: nohup, disown, setsid, double-fork
#
# Each technique launches a "test worker" that prints its own
# PID tree info every few seconds so we can inspect it.

WORKER="./worker.sh"

cat > "$WORKER" <<'EOF'
#!/usr/bin/env bash
while true; do
    echo "--- Worker $$ ---"
    echo "PID : $$"
    echo "PPID: $PPID"
    echo "PGID: $(ps -o pgid= $$ | tr -d ' ')"
    echo "SID : $(ps -o sid= $$ | tr -d ' ')"
    echo "TTY : $(ps -o tty= $$ | tr -d ' ')"
    echo "Command: $0"
    echo
    sleep 5
done
EOF
chmod +x "$WORKER"

echo "=============================="
echo "   1) nohup"
echo "=============================="
nohup "$WORKER" > nohup.out 2>&1 & 
echo "nohup PID: $!"

sleep 1

echo "=============================="
echo "   2) disown"
echo "=============================="
"$WORKER" &
PID=$!
echo "disown PID before: $PID"
disown $PID
sleep 1

echo "=============================="
echo "   3) setsid"
echo "=============================="
# setsid makes a new session, no controlling TTY
setsid "$WORKER" > setsid.out 2>&1 &
echo "setsid PID: $!"

sleep 1

echo "=============================="
echo "   4) double-fork daemon"
echo "=============================="
(
    # First fork
    (
        # Second fork (actual daemon)
        "$WORKER" &
        exit 0
    ) &
    # parent exits
    exit 0
) &
echo "double-fork daemon started (PID printed by worker itself)"
sleep 1

echo
echo "=== All workers launched ==="
echo "Log out, log in again, then use:"
echo "    ps -e -o pid,ppid,pgid,sid,tty,cmd | grep worker"
echo "to check which ones survived."

# test
chmod +x combined_test.sh
./combined_test.sh

# logout SSH session
# log in again
# check:
ps -e -o pid,ppid,pgid,sid,tty,cmd | grep worker
```

## Summary
1. nohup
    Keeps process alive after logout
    HUP signal ignored
    Same session & PGID typically
    Output redirected to nohup.out

2. disown
    Removes job from shell’s job table
    No longer gets SIGHUP from shell
    Still in same session/PGID unless you manually change them
    Dies if session ends (i.e., terminal fully goes away)

3. setsid
    Starts a new session
    New SID = its PID
    No controlling terminal
    Survives logout unless it uses the TTY

4. double-fork
    Standard daemon pattern
    Parent exits → orphan → adopted by PID 1
    Fully independent of terminal/session
    Always survives logout
    No controlling terminal

## session, process group, job
```bash
                   ┌──────────────────────────────┐
                   │        Terminal (TTY)        │
                   │   → controlling terminal     │
                   └───────────────┬──────────────┘
                                   │
                           Session (SID = 1000)
                                   │
     ┌─────────────────────────────┼──────────────────────────────┐
     │                             │                              │
Process Group (PGID=1000)   Process Group (PGID=2001)      Process Group (PGID=2002)
Foreground Job               Background Job                 Background Job
     │                             │                              │
 ┌───┴────────────────┐     ┌──────┴────────────────┐      ┌──────┴────────────────┐
 │bash (PID 1000)     │     │grep (PID 2001)        │      │sleep (PID 2002)       │
 │vim  (PID 1200)     │     │sort (PID 2005)        │      │                       │
 └────────────────────┘     └───────────────────────┘      └───────────────────────┘
```

- Meaning:
    One session per terminal
    A session contains multiple process groups
    Each process group may contain several processes
    One process group at a time is the foreground
    Others are background jobs


- Session
	a session is a kernel-managed group of one or more process groups
	a session has a unique session ID (sid)
	a session may have 0 or 1 controlling terminal(tty)

- Process group
	a process group is a set of processes
	all the processes in the same process group share the same pgid
	process group is used for job control (Ctrl-C, Ctrl-Z, etc.)

- Job(shell-level abstraction)
	a job corresponds to 1 process group(kernel-enforced entity), because job control signals(SIGINT, SIGSTSOP, etc.) apply to process group

- Controlling terminal
	when you open a terminal:
		the shell becomes session leader(the process that initiates the session)
		the terminal becomes the controlling terminal for that session
		1 process group becomes the foreground process group, foreground PG receives: stdin, SIGINT, SIGSTP, etc.
```