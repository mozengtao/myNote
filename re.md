- ```python
  # Searching Functions
  re.search()    	Scans a string for a regex match
  re.match()	    Looks for a regex match at the beginning of a string
  re.fullmatch()	Looks for a regex match on an entire string
  re.findall()	Returns a list of all regex matches in a string
  re.finditer()	Returns an iterator that yields regex matches from a string
  
  >>> re.search(r'[a-z]+', '123FOO456', flags=re.IGNORECASE)
  <_sre.SRE_Match object; span=(3, 6), match='FOO'>
  
  >>> re.match(r'\d+', '123foobar')
  <_sre.SRE_Match object; span=(0, 3), match='123'>
  
  >>> re.fullmatch(r'\d+', '123')
  <_sre.SRE_Match object; span=(0, 3), match='123'>
  
  >>> re.findall(r'\w+', '...foo,,,,bar:%$baz//|')
  ['foo', 'bar', 'baz']
  >>> re.findall(r'(\w+),(\w+)', 'foo,bar,baz,qux,quux,corge')
  [('foo', 'bar'), ('baz', 'qux'), ('quux', 'corge')]
  >>> re.findall(r'(\w+),(\w+),(\w+)', 'foo,bar,baz,qux,quux,corge')
  [('foo', 'bar', 'baz'), ('qux', 'quux', 'corge')]
  
  >>> for i in re.finditer(r'\w+', '...foo,,,,bar:%$baz//|'):
  ...     print(i)
  ...
  <_sre.SRE_Match object; span=(3, 6), match='foo'>
  <_sre.SRE_Match object; span=(10, 13), match='bar'>
  <_sre.SRE_Match object; span=(16, 19), match='baz'>
  
  
  # Substitution Functions
  (If re.sub() doesn’t find any matches, then it always returns <string> unchanged.)
  re.sub()	Scans a string for regex matches, replaces the matching portions of the string with 
  			the specified replacement string, and returns the result
  re.subn()	Behaves just like re.sub() but also returns information regarding the number of 
  			substitutions made
  
  >>> s = 'foo.123.bar.789.baz'
  >>> re.sub(r'\d+', '#', s)
  'foo.#.bar.#.baz'
  >>> re.sub('[a-z]+', '(*)', s)
  '(*).123.(*).789.(*)'
  
  >>> re.sub(r'(\w+),bar,baz,(\w+)',
  ...        r'\2,bar,baz,\1',
  ...        'foo,bar,baz,qux')
  'qux,bar,baz,foo'
  
  >>> re.sub(r'(\d+)', r'\g<1>0', 'foo 123 bar')
  'foo 1230 bar'
  (\g<1> to refer to the group)
  
  >>> re.sub(r'\d+', '/\g<0>/', 'foo 123 bar')
  'foo /123/ bar'
  (The backreference \g<0> refers to the text of the entire match)
  
  >>> re.sub('x*', '-', 'foo')
  '-f-o-o-'
  (If <regex> specifies a zero-length match, then re.sub() will substitute <repl> into every 
   character position in the string)
  
  
  # Substitution by Function
  >>> def f(match_obj):
  ...     s = match_obj.group(0)  # The matching string
  ...
  ...     # s.isdigit() returns True if all characters in s are digits
  ...     if s.isdigit():
  ...         return str(int(s) * 10)
  ...     else:
  ...         return s.upper()
  ...
  >>> re.sub(r'\w+', f, 'foo.10.bar.20.baz.30')
  'FOO.100.BAR.200.BAZ.300'
  
  
  # Utility Functions
  re.split()	Splits a string into substrings using a regex as a delimiter
  re.escape()	Escapes characters in a regex
  
  >>> re.split('\s*[,;/]\s*', 'foo,bar  ;  baz / qux')
  ['foo', 'bar', 'baz', 'qux']
  >>> re.split('(\s*[,;/]\s*)', 'foo,bar  ;  baz / qux')
  ['foo', ',', 'bar', '  ;  ', 'baz', ' / ', 'qux']
  
  This can be useful if you want to split <string> apart into delimited tokens, process the tokens 
  in some way, then piece the string back together using the same delimiters that originally 
  separated them:
  >>> string = 'foo,bar  ;  baz / qux'
  >>> regex = r'(\s*[,;/]\s*)'
  >>> a = re.split(regex, string)
  
  >>> # List of tokens and delimiters
  >>> a
  ['foo', ',', 'bar', '  ;  ', 'baz', ' / ', 'qux']
  
  >>> # Enclose each token in <>'s
  >>> for i, s in enumerate(a):
  ...
  ...     # This will be True for the tokens but not the delimiters
  ...     if not re.fullmatch(regex, s):
  ...         a[i] = f'<{s}>'
  ...
  
  >>> # Put the tokens back together using the same delimiters
  >>> ''.join(a)
  '<foo>,<bar>  ;  <baz> / <qux>'
  
  If you need to use groups but don’t want the delimiters included in the return list, then you 
  can use noncapturing groups:
  >>> string = 'foo,bar  ;  baz / qux'
  >>> regex = r'(?:\s*[,;/]\s*)'
  >>> re.split(regex, string)
  ['foo', 'bar', 'baz', 'qux']
  
  If the optional maxsplit argument is present and greater than zero, then re.split() performs at 
  most that many splits.
  >>> s = 'foo, bar, baz, qux, quux, corge'
  
  >>> re.split(r',\s*', s)
  ['foo', 'bar', 'baz', 'qux', 'quux', 'corge']
  >>> re.split(r',\s*', s, maxsplit=3)
  ['foo', 'bar', 'baz', 'qux, quux, corge']
  
  
  This is useful if you’re calling one of the re module functions, and the <regex> you’re passing 
  in has a lot of special characters that you want the parser to take literally instead of as 
  metacharacters.
  >>> print(re.match('foo^bar(baz)|qux', 'foo^bar(baz)|qux'))
  None
  >>> re.match('foo\^bar\(baz\)\|qux', 'foo^bar(baz)|qux')
  <_sre.SRE_Match object; span=(0, 16), match='foo^bar(baz)|qux'>
  
  >>> re.escape('foo^bar(baz)|qux') == 'foo\^bar\(baz\)\|qux'
  True
  >>> re.match(re.escape('foo^bar(baz)|qux'), 'foo^bar(baz)|qux')
  <_sre.SRE_Match object; span=(0, 16), match='foo^bar(baz)|qux'>
  
  
  # Compiled Regex Objects in Python
  >>> re.search(r'(\d+)', 'foo123bar')
  <_sre.SRE_Match object; span=(3, 6), match='123'>
  
  >>> re_obj = re.compile(r'(\d+)')
  >>> re.search(re_obj, 'foo123bar')
  <_sre.SRE_Match object; span=(3, 6), match='123'>
  >>> re_obj.search('foo123bar')
  <_sre.SRE_Match object; span=(3, 6), match='123'>
  
  If you use a particular regex in your Python code frequently, then precompiling allows you to 
  separate out the regex definition from its uses. This enhances modularity.
  >>> s1, s2, s3, s4 = 'foo.bar', 'foo123bar', 'baz99', 'qux & grault'
  >>> re_obj = re.compile('\d+')
  
  >>> re_obj.search(s1)
  >>> re_obj.search(s2)
  <_sre.SRE_Match object; span=(3, 6), match='123'>
  >>> re_obj.search(s3)
  <_sre.SRE_Match object; span=(3, 5), match='99'>
  >>> re_obj.search(s4)
  
  
  # Match Object Methods
  match.group()		The specified captured group or groups from match
  match.__getitem__()	A captured group from match
  match.groups()		All the captured groups from match
  match.groupdict()	A dictionary of named captured groups from match
  match.expand()		The result of performing backreference substitutions from match
  match.start()		The starting index of match
  match.end()		The ending index of match
  match.span()		Both the starting and ending indices of match as a tuple
  # Match Object Attributes
  match.pos
  match.endpos	The effective values of the <pos> and <endpos> arguments for the match
  match.lastindex	The index of the last captured group
  match.lastgroup	The name of the last captured group
  match.re	The compiled regular expression object for the match
  match.string	The search string for the match
  ```
- [Regular Expressions: Regexes in Python (Part 1)](https://realpython.com/regex-python/)
- [Regular Expressions: Regexes in Python (Part 2)](https://realpython.com/regex-python-part-2/)