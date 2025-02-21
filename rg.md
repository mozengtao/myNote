- [ripgrep](https://github.com/BurntSushi/ripgrep)
	- #+BEGIN_QUOTE
	  ripgrep (rg) recursively searches the current directory for a regex pattern.
	  By default, ripgrep will respect gitignore rules and automatically skip hidden
	  files/directories and binary files.
	  
	  Use -h for short descriptions and --help for more details.
	  
	  Project home page: https://github.com/BurntSushi/ripgrep
	  
	  
	  USAGE:
	      rg [OPTIONS] PATTERN [PATH ...]
	      rg [OPTIONS] -e PATTERN ... [PATH ...]
	      rg [OPTIONS] -f PATTERNFILE ... [PATH ...]
	      rg [OPTIONS] --files [PATH ...]
	      rg [OPTIONS] --type-list
	      command | rg [OPTIONS] PATTERN
	      rg [OPTIONS] --help
	      rg [OPTIONS] --version
	  #+END_QUOTE
- # Usage
	- ```bash
	  1. Search for a Specific Pattern
	  Syntax: rg PATTERN
	  Example: rg 'error'
	  
	  2. Search for a Pattern in a Specific File Type
	  Syntax: rg PATTERN -g EXTENSION
	  Example: rg 'include' -g '*.h'
	  
	  3. Search for a Pattern and Show Line Numbers
	  Syntax: rg PATTERN -n
	  Example: rg 'main' -n
	  
	  4. Search for a Pattern in a Specific Directory
	  Syntax: rg PATTERN DIRECTORY
	  Example: rg 'function' /path/to/directory
	  
	  5. Search for a Pattern Case-Insensitively
	  Syntax: rg PATTERN -i
	  Example: rg 'error' -i
	  
	  6. Search for a Whole Word
	  Syntax: rg PATTERN -w
	  Example: rg 'main' -w
	  
	  7. Search for a Pattern and Show Context
	  Syntax: rg PATTERN -C NUM
	  Example: rg 'function' -C 2
	  
	  8. Search for a Pattern and Replace with Another String
	  Syntax: rg PATTERN -r REPLACEMENT
	  Example: rg 'error' -r 'warning'
	  
	  9. Search for a Pattern in Files Modified Within a Specific Size
	  Syntax: rg PATTERN --max-filesize SIZE
	  Example: rg 'include' --max-filesize 1M
	  ```