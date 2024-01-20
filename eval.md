- > On Unix-like operating systems, eval is a builtin command of the Bash shell. It ^^concatenates^^ its arguments into a single string, joining the arguments ^^with spaces^^, then ^^executes^^ that string as ^^a bash command.^^ It's similar to running bash -c "string", but eval executes the command ^^in the current shell environment^^ rather than creating a child shell process.
- 提供给eval命令的字符串可以包含预留关键字，这些关键字会在第一轮被解释，之后别的字符串在第二轮被解释
	- ```bash
	  cmd="if true; then echo 1; else echo 0; fi
	  eval "$cmd"
	  ```
- eval 命令可以用来提供额外的间接层引用
	- ```bash
	  cmd1="cmd2"
	  cmd2="echo Hi!"
	  eval "\${$cmd1}"
	  ```
- eval 在当前 shell 环境下执行，而不是子 shell
	- ```bash
	  cat variables.txt
	  first=How-to
	  second=Geek
	  
	  eval "$(cat variables.txt)"
	  echo $first $second
	  How-to Geek
	  ```