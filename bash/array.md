## array
### indexed array
```bash
declare -a fruits=("apple" "banana" "cherry")

echo "${fruits[0]}"   # apple
echo "${fruits[1]}"   # banana
echo "${fruits[2]}"   # cherry

fruits[3]="orange"
fruits+=("grape")     # append automatically at next index

for f in "${fruits[@]}"; do
  echo "$f"
done
```

## associative array
```bash
declare -A capital

capital["France"]="Paris"
capital["Japan"]="Tokyo"
capital["Canada"]="Ottawa"

echo "${capital["Japan"]}"   # Tokyo

for country in "${!capital[@]}"; do
  echo "$country -> ${capital[$country]}"
done
```

```bash
# create array
nums=(10 20 30)

# access array elements
echo "${nums[0]}"  # Output: 10

# add/modify elements
nums[3]=40
nums[1]=25

# append to array
nums+=(50 60)

# get array length
echo "${#nums[@]}"  # Output: 4

# iterate over array
# loop over values
for num in "${nums[@]}"; do
  echo "$num"
done
# loop over indices
for i in "${!nums[@]}"; do
  echo "Index $i: ${nums[i]}"
done

# remove element
unset 'nums[2]'
echo "After unset:"
for num in "${nums[@]}"; do
  echo "$num"
done

# delete element by its value
delete_from_array() {
    local value=$1      # value to remove
    shift
    local -n arr=$1     # nameref to the array (Bash 4.3+ required)

    local i
    for i in "${!arr[@]}"; do
        if [[ "${arr[i]}" == "$value" ]]; then
            unset 'arr[i]'   # remove element by index
        fi
    done
}

delete_from_array() {
    local value=$1
    local -n arr=$2
    for i in "${!arr[@]}"; do
        if [[ "${arr[i]}" == "$value" ]]; then
            unset 'arr[i]'
        fi
    done
}


# slice array
slice=("${nums[@]:1:2}")  # From index 1, take 2 elements
echo "Slice:"
for num in "${slice[@]}"; do
  echo "$num"
done

# check if an index exists
if [[ -v nums[3] ]]; then
  echo "Index 3 exists"
else
  echo "Index 3 does not exist"
fi

# in_array needle haystack[@]
in_array() {
    local needle=$1
    shift
    local haystack=("$@")

    for e in "${haystack[@]}"; do
        if [[ $e == "$needle" ]]; then
            return 0  # found
        fi
    done
    return 1  # not found
}


# join array into string
joined=$(IFS=,; echo "${nums[*]}")
echo "Joined: $joined"  # Output: Joined: 10,25,40

# read array from command output
# 1.-t removes trailing newlines 2.< <(...) is process substitution, feeding the command’s output.
mapfile -t files < <(ls /etc)
echo "Files in /etc:"
for file in "${files[@]}"; do
  echo "$file"
done

# If the command output is space-separated, you can assign it directly
arr=($(echo "a b c d"))
echo "${arr[2]}"   # c

# Use read -a with custom IFS for splitting on delimiters.
mac="6c:ca:08:8b:b9:76"
IFS=: read -r -a parts <<< "$mac"

# associative array
declare -A user_ages
user_ages=(["Alice"]=30 ["Bob"]=25)
echo "Alice's age: ${user_ages["Alice"]}"
user_ages["Charlie"]=35
echo "All users:"
for user in "${!user_ages[@]}"; do
  echo "$user is ${user_ages[$user]} years old"
done

# remove associative array element
unset 'user_ages["Bob"]'
echo "After removing Bob:"
for user in "${!user_ages[@]}"; do
  echo "$user is ${user_ages[$user]} years old"
done

# get associative array length
echo "Number of users: ${#user_ages[@]}"

# check if key exists
if [[ -v user_ages["Alice"] ]]; then
  echo "Alice is in the array"
else
  echo "Alice is not in the array"
fi

# clear entire array
unset user_ages
echo "After clearing, number of users: ${#user_ages[@]}"  # Output: 0

# array utilities
# Check if an element exists in array
in_array() {
    local -n arr=$1
    local val=$2
    for e in "${arr[@]}"; do
        [[ $e == "$val" ]] && return 0
    done
    return 1
}

# Append values to array
append_array() {
    local -n arr=$1
    shift
    arr+=("$@")
}

# Delete element(s) by value
delete_value() {
    local -n arr=$1
    local val=$2
    for i in "${!arr[@]}"; do
        [[ "${arr[i]}" == "$val" ]] && unset 'arr[i]'
    done
}

# Print array
print_array() {
    local -n arr=$1
    echo "${arr[@]}"
}

# Get array length
array_length() {
    local -n arr=$1
    echo "${#arr[@]}"
}

# Get last element
array_last() {
    local -n arr=$1
    echo "${arr[-1]}"
}

# Join array with delimiter
join_array() {
    local -n arr=$1
    local IFS="$2"
    echo "${arr[*]}"
}

# usage
fruits=(apple banana cherry)

# 1. Check if in array
if in_array fruits "banana"; then
    echo "banana exists"
fi

# 2. Append
append_array fruits orange grape
print_array fruits   # apple banana cherry orange grape

# 3. Delete
delete_value fruits banana
print_array fruits   # apple cherry orange grape

# 4. Length
echo "Length: $(array_length fruits)"   # 4

# 5. Last element
echo "Last: $(array_last fruits)"       # grape

# 6. Join with comma
echo "CSV: $(join_array fruits ,)"      # apple,cherry,orange,grape

# Associative Array Utilities
# Check if a key exists in associative array
assoc_has_key() {
    local -n map=$1
    local key=$2
    [[ -v map["$key"] ]]
}

# Get value by key
assoc_get() {
    local -n map=$1
    local key=$2
    echo "${map[$key]}"
}

# Set (insert/update) key=value
assoc_set() {
    local -n map=$1
    local key=$2
    local value=$3
    map["$key"]="$value"
}

# Delete entry by key
assoc_del() {
    local -n map=$1
    local key=$2
    unset 'map[$key]'
}

# Print all key=value pairs
assoc_print() {
    local -n map=$1
    for k in "${!map[@]}"; do
        echo "$k=${map[$k]}"
    done
}

# Get all keys
assoc_keys() {
    local -n map=$1
    echo "${!map[@]}"
}

# Get all values
assoc_values() {
    local -n map=$1
    echo "${map[@]}"
}

# Length (number of pairs)
assoc_length() {
    local -n map=$1
    echo "${#map[@]}"
}

# usage
declare -A capitals=(
    [france]="Paris"
    [japan]="Tokyo"
    [canada]="Ottawa"
)

# 1. Check key
if assoc_has_key capitals japan; then
    echo "Japan exists"
fi

# 2. Get
echo "Capital of France: $(assoc_get capitals france)"

# 3. Set
assoc_set capitals germany Berlin
echo "Capital of Germany: $(assoc_get capitals germany)"

# 4. Delete
assoc_del capitals canada
assoc_print capitals
# france=Paris
# japan=Tokyo
# germany=Berlin

# 5. Keys and values
echo "Keys: $(assoc_keys capitals)"      # france japan germany
echo "Values: $(assoc_values capitals)"  # Paris Tokyo Berlin

# 6. Length
echo "Length: $(assoc_length capitals)"  # 3
```

[Bash readarray with Examples](https://linuxopsys.com/bash-readarray-with-examples)  

```bash
#
dd if=/dev/urandom of=random_10G.bin bs=1M count=10240 status=progress

# process substitution
diff <(ls dir1) <(ls dir2)

# 多命令并行执行
cmd1 & cmd 2 & wait

# 变量默认值
echo "enter your name"
read name
name=${name:-Unknown}

# replace if .. then
[ ! $? -eq 0 ] && { echo "error with rsync"; exit 1; }

# chain multiple conditions
[[ -z "$v1" && -z "$v2" ]] && { echo "need v1 and v2"; exit 1; }

# use trap to handle script failure
cleanup() {
	echo "Cleaning up ..."
	rm -rf /tmp/tetmpfile
}

trap cleanup EXIT

# mkfifo for complex IPC

# use readarray instead of manual loops
readarray -t lines < /var/log/syslog
for line in "${lines[@]}"; do
  echo "$line"
done

#
readarray -t myArr  < sample.txt

readarray -t myArr < <(seq 5)

readarray myArr  <<< $(cat sample.txt)

config="$(<cfg.toml)"

config="$(cat cfg.toml)"


# Use xargs -P for Parallel Execution
cat hosts.txt | xargs -P 4 -I {} ssh {} 'uptime'

# Avoid cat When You Don’t Need It
grep "error" file.txt

# Use [[ ... ]] Instead of [ ... ]
[[ $VAR == "foo" ]]


```