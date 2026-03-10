## Template
```bash
#!/bin/bash

usage()
{
	echo "Usage: $0 -t <task> -n <number> [-v] [args...]"
	exit 1
}

# defaults
verbose=0

while getopts ":vt:n:" opt; do
	case $opt in
		v) verbose=1 ;;
		t) task=$OPTARG ;;
		n) number=$OPTARG ;;
		\?) echo "Error: invalid option -$OPTARG"; usage ;;
		:)  echo "Error: -$OPTARG requires an argument"; usage ;;
	esac
done
shift $((OPTIND - 1))

# remaining positional arguments
arg1=$1
arg2=$2

# validate required options
if [ -z "$task" ] || [ -z "$number" ]; then
	usage
fi
```

## `getopts` Option String Format

The first argument to `getopts` is the option string that defines recognized options.

| String | Meaning |
|--------|---------|
| `"t:"` | `-t` requires an argument |
| `"v"` | `-v` is a boolean flag (no argument) |
| `"vt:n:"` | `-v` flag, `-t` with arg, `-n` with arg |
| `":vt:n:"` | leading `:` enables silent error mode |

Rules:
- Each letter represents one option
- A `:` after a letter means that option **requires an argument**
- A `:` at the **beginning** enables silent error mode

## Built-in Variables

| Variable | Description |
|----------|-------------|
| `$opt` | The current option letter being processed |
| `$OPTARG` | The argument value for the current option (when `:` follows the letter) |
| `$OPTIND` | Index of the **next** argument to be processed (starts at 1) |

## `OPTIND` and `shift`

`OPTIND` tracks which positional parameter `getopts` will process next.

### Default Mode (no leading `:`)

`getopts` prints error messages to stderr automatically.

```bash
while getopts "t:n:" opt; do
	case $opt in
		t) task=$OPTARG ;;
		n) number=$OPTARG ;;
		*) usage ;;       # catches all errors
	esac
done
```

- Unknown option `-x` → getopts prints error, `opt` is set to `?`
- Missing argument for `-t` → getopts prints error, `opt` is set to `?`

### Silent Mode (leading `:`)

Suppresses automatic error messages, giving full control to the script.

```bash
while getopts ":t:n:" opt; do
	case $opt in
		t) task=$OPTARG ;;
		n) number=$OPTARG ;;
		\?) echo "Error: unknown option -$OPTARG"; usage ;;
		:)  echo "Error: -$OPTARG requires an argument"; usage ;;
	esac
done
```

## Manual Loop (Full Control)

Skip `getopts` entirely. A `while/case` loop with `shift` handles arbitrary complexity.

```bash
# ./script.sh --task vmc 2 --port 8100 -v

task=""
instance=""
port=""
verbose=0

while [ $# -gt 0 ]; do
	case $1 in
		-t|--task)
			task=$2
			instance=$3
			shift 3
			;;
		-p|--port)
			port=$2
			shift 2
			;;
		-v|--verbose)
			verbose=1
			shift
			;;
		--)
			shift
			break
			;;
		-*)
			echo "Error: unknown option $1"
			usage
			;;
		*)
			break
			;;
	esac
done
```

No `getopts` limitations. Supports long options, multi-value options, and any
argument structure. Best for complex CLIs.

## Long Options Alternative

`getopts` does not support `--long-options`. For those, parse manually:

```bash
while [ $# -gt 0 ]; do
	case $1 in
		-t|--task)   task=$2;   shift 2 ;;
		-n|--number) number=$2; shift 2 ;;
		-v|--verbose) verbose=1; shift ;;
		--)          shift; break ;;
		-*)          echo "Error: unknown option $1"; usage ;;
		*)           break ;;
	esac
done

# $@ now contains remaining positional arguments
```