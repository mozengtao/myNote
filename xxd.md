
[man 1 xxd](https://linux.die.net/man/1/xxd)  

## Options
```bash
# Options
       -p | -ps | -postscript | -plain
              Output in postscript continuous hexdump style. Also known as plain hexdump style.

       -r | -revert
              Reverse operation: convert (or patch) hexdump into binary.  If not writing to stdout, xxd writes into its output file without truncating it. Use the combination -r -p to read  plain, hexadecimal dumps without line number information and without a particular column layout. Additional Whitespace and line-breaks are allowed anywhere.

       -l len | -len len
              Stop after writing <len> octets.			  

       -s [+][-]seek
              Start at <seek> bytes abs. (or rel.) infile offset.  + indicates that the seek is relative to the current stdin file position (meaningless when not reading from stdin).  - indicates that the seek should be that many characters from the end of the input (or if combined with +: before the current stdin file position).  Without -s option, xxd starts at the current file position.		

       -g bytes | -groupsize bytes
              Separate  the output of every <bytes> bytes (two hex characters or eight bit-digits each) by a whitespace.  Specify -g 0 to suppress grouping.  <Bytes> defaults to 2 in normal mode, 4 in little-endian mode and 1 in bits mode.  Grouping does not apply to postscript or include style.			  	  

```

## Write Hex Data
```bash
# "xxd -r -p" is used to convert plain hex dump (space-separated hex values) into binary data, 
echo "eb fe 11 22 33 44" | xxd -r -p > test.bin		# Writes the bytes 0xEB 0xFE 0x11 0x22 0x33 0x44 to the output


# Common Use Cases

#1 Generate a Hexdump
xxd file.bin

#2 Create Binary from Hex (Reverse Mode)
echo "55aa" | xxd -r -p > sector.sig  # Write 0x55 0xAA to a file

#3 Inspect Specific Bytes
xxd -s 0x1FE -l 2 -p /dev/sda  # Read 2 bytes at offset 510 (0x1FE)

#4 Edit Binary Files
# Patch byte at offset 0x05 to 0xFF
echo "000005: FF" | xxd -r - file.bin
# 

```

- 常用命令
	- ```bash
	  xxd -p -c1 file.png > file.png.xxd
	  xxd -p -r file.png.xxd > file.png.bak
	  file.png.bak文件的内容应该与file.png完全一致
	  
	  -r | -revert
	  reverse operation: convert (or patch) hexdump into binary. If not writing to stdout, 
	  xxd writes into its output file without truncating it. Use the combination -r -p to read 
	  plain hexadecimal dumps without line number information and without a particular column 
	  layout. Additional Whitespace and line-breaks are allowed anywhere.
	  
	  -c cols | -cols cols
	  format <cols> octets per line. Default 16 (-i: 12, -ps: 30, -b: 6). Max 256.
	  ```
	- `reverse-hexdump.sh`
		- ```bash
		  reverse-hexdump.sh is a small shell/awk tool to reverse hexdump -C output back to the 
		  original data. The command reads from standard input or named files and writes to 
		  standard output
		  
		  Usage:
		  reverse-hexdump.sh [FILE]... > DEST
		  
		  (FILE: hexdump -C test.bin > FILE)
		  ```
		- ```bash
		  #!/bin/sh
		  # SPDX-License-Identifier: MIT
		  # Copyright (c) 2020 Mike Fleetwood
		  # FILE: reverse-hexdump.sh
		  # Reverse 'hexdump -C' output back to the original data.
		  # USAGE: reverse-hexdump.sh [FILE]... > DEST
		  
		  LANG=C awk '
		  function outputbinary(text)
		  {
		  	num_elements = split(text, hex_strs)
		  	for (i = 1; i <= num_elements; i++)
		  		# WARNING: Run in "C" locale to prevent GAWK using
		  		# multibyte character encoding rather than printing
		  		# the desired byte.
		  		#   The GNU Awk Users Guide,
		  		#   5.5.2 Format-Control Letters, %c
		  		#   https://www.gnu.org/software/gawk/manual/html_node/Control-Letters.html
		  		printf "%c", strtonum("0x" hex_strs[i])
		  	return num_elements
		  }
		  
		  BEGIN {
		  	curr_offset = 0
		  	next_offset = 0
		  	repeat = 0
		  }
		  
		  /^[[:xdigit:]]/ {
		  	next_offset = strtonum("0x" $1)
		  }
		  
		  /^[[:xdigit:]]/ && repeat == 1 {
		  	while (curr_offset < next_offset)
		  		curr_offset += outputbinary(hex_representation)
		  	repeat = 0
		  }
		  
		  /^[[:xdigit:]]/ && repeat == 0 {
		  	curr_offset = strtonum("0x" $1)
		  	hex_representation = substr($0,11,48)
		  	curr_offset += outputbinary(hex_representation)
		  }
		  
		  # for handling the result of 'hexdump -C FILE', which may contain '*' as 1 line
		  /^\*/ {
		  	repeat = 1
		  }' "${@}"
		  ```
- 参考链接
	- [[hexdump]]
	- [man 1 xxd](https://linux.die.net/man/1/xxd)
	- [Doing a Reverse Hex Dump](https://www.linuxjournal.com/content/doing-reverse-hex-dump)
	- [xxd, Binary to ASCII or ASCII to Binary](https://www.real-world-systems.com/docs/xxd.1.html)
	- [reverse-hexdump](https://github.com/mfleetwo/reverse-hexdump)