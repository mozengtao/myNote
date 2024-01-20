- Formatting Strings
	- ```python
	  # 1  The string interpolation operator (%), or modulo operator
	  name = "Jane"
	  age = 25
	  "Hello, %s! You're %s years old." % (name, age)
	  -->  'Hello, Jane! You're 25 years old.'
	  "Hello, %(name)s! You're %(age)s years old." % {"name": "Jane", "age": 25}
	  -->  "Hello, Jane! You're 25 years old."
	  
	  # 2  The str.format() method
	  name = "Jane"
	  age = 25
	  "Hello, {}! You're {} years old.".format(name, age)
	  -->  "Hello, Jane! You're 25 years old."
	  "Hello, {1}! You're {0} years old.".format(age, name)
	  -->  "Hello, Jane! You're 25 years old."
	  "Hello, {name}! You're {age} years old.".format(name="Jane", age=25)
	  -->  "Hello, Jane! You're 25 years old."
	  
	  person = {"name": "Jane", "age": 25}
	  "Hello, {name}! You're {age} years old.".format(**person)
	  -->  "Hello, Jane! You're 25 years old."
	  
	  # 3 f-string syntax (Python 3.6 and later)
	  name = "Jane"
	  age = 25
	  f"Hello, {name}! You're {age} years old."
	  -->  'Hello, Jane! You're 25 years old.'
	  
	  (You can embed almost any Python expression in an f-string.)
	  f"{2 * 21}"
	  --> '42'
	  
	  f"Hello, {name.upper()}! You're {age} years old."
	  --> "Hello, JANE! You're 25 years old."
	  
	  f"{[2**n for n in range(3, 9)]}"
	  --> '[8, 16, 32, 64, 128, 256]'
	  
	  (The expressions that you embed in an f-string are evaluated at runtime. 
	   Then, Python formats the result using the .__format__() special method 
	   under the hood.)
	  format(5425.9292, ".2f")
	  --> '5425.93'
	  
	  balance = 5425.9292
	  f"Balance: ${balance:.2f}"
	  --> 'Balance: $5425.93'
	  
	  (You can create a wide variety of format specifiers.)
	  >>> integer = -1234567
	  >>> f"Comma as thousand separators: {integer:,}"
	  'Comma as thousand separators: -1,234,567'
	  
	  >>> sep = "_"
	  >>> f"User's thousand separators: {integer:{sep}}"
	  'User's thousand separators: -1_234_567'
	  
	  >>> floating_point = 1234567.9876
	  >>> f"Comma as thousand separators and two decimals: {floating_point:,.2f}"
	  'Comma as thousand separators and two decimals: 1,234,567.99'
	  
	  >>> date = (9, 6, 2023)
	  >>> f"Date: {date[0]:02}-{date[1]:02}-{date[2]}"
	  'Date: 09-06-2023'
	  
	  >>> from datetime import datetime
	  >>> date = datetime(2023, 9, 26)
	  >>> f"Date: {date:%m/%d/%Y}"
	  'Date: 09/26/2023'
	  ```
- ```
  capitalize()	Change to upper case for the first char
  center()	Center the string by using filling chars
  casefold()	Change to lower case for all upper case chars in a string
  count()		Counting Presence of sub string inside main string
  endswith()	Checking if string ends with input string
  expandtabs()	Number of spaces to use in place of tab
  find()		Case sensitive string search
  index()		Case sensitive string search
  isalnum()	Check if all chars are alphanumeric in a string
  isalpha()	Check if all chars are alphabets in a string
  isdecimal()	Check if all chars are decimal numbers in a string
  isdigit()	Check if all chars are digits in a string
  isidentifier()	Check if the string is identifier
  islower()	Check if all chars are lower case only
  isnumeric()	Check if all chars are numeric
  isprintable()	Check if all chars are printable or not
  isspace()	Check if all chars are whitespace or not
  istitle()	Check if all words starts with upper case letters
  isupper()	Check if all chars are upper case letters
  join()		Join elements of a iterable object
  ljust()		Left Justifying the string
  lower()		Change all upper case to lower case chars
  lstrip()	Removes space or char from left side of the string
  partition()	Breaking string by using search word
  replace()	search and replace string inside a main string
  rfind()		search from right and returns the position of the matching string
  rindex()	search from right and returns the position of the matching string
  rjust()		Right Justify the string
  rpartition()	Breaking string by using search word from right
  rsplit()	Breaking string by using delimiter
  rstrip()	Removing chars from right side
  split()		Split the string using delimiters
  splitlines()	Break the string using line breaks
  startswith()	Check if string is starting with or not
  strip()		Remove space or char from left or right of the string
  swapcase()	Change case from lower to upper and upper to lower
  title()		First char of each word to upper case letter
  translate()	Mapping chars of a string
  upper()		Change all lower case to upper case letter
  zfill()		fill the left of the string with zeros
  ```
- Find out the frequency of occurrence of chars in a string
	- ```python
	  my_str='Welcome to Python'
	  my_dict={}
	  for i in my_str:
	    if i in my_dict:
	      my_dict[i] = my_dict[i] + 1
	    else:
	      my_dict[i]  = 1
	  print(my_dict)
	  ```
- Return the matching strings from a list
	- ```python
	  # 1
	  my_list=['aecde','adba','acbd','abcd','abded','bdbd','baba']
	  search_str='ab'
	  filtered = [s for s in my_list if search_str in s] 
	  
	  # 2
	  import re
	  my_list=['aecde','adba','acbd','abcd','abded','bdbd','baba']
	  search_str='ab' # searh string 
	  for element in my_list:
	      #if(re.search(search_str,element,re.IGNORECASE)):
	      if(re.match(search_str,element,re.IGNORECASE)):    
	          print(element)
	  ```
- collect string between two sub-strings
	- ```python
	  import re
	  
	  data="$var1=PY_tkinter_end;"
	  str1 = re.search('PY_(.*)_end', data)
	  print(str1.group(1)) # tkinter
	  ```
- F-String
	- [Python's F-String for String Interpolation and Formatting](https://realpython.com/python-f-strings/)
- [Text Sequence Type — str](https://docs.python.org/3/library/stdtypes.html#str)