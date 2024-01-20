- What is JSON
	- #+BEGIN_QUOTE
	  JSON (JavaScript Object Notation) is a lightweight data-interchange format. It is easy for humans to read and write. It is easy for machines to parse and generate.
	  #+END_QUOTE
- JSON data structures
	- #+BEGIN_QUOTE
	  JSON defines **only two data structures: objects and arrays**. An **object** is a set of name-value pairs, and an **array** is a list of values. JSON defines seven value types: **string, number, object, array, true, false, and null**.
	  #+END_QUOTE
- JSON syntax
	- #+BEGIN_QUOTE
	  **Objects** are enclosed in braces ({}), their **name-value pairs are separated by a comma (,)**, and the **name and value in a pair are separated by a colon (:)**. Names in an object are **strings**, whereas values may be of any of the **seven** value types, including another object or an array.
	  
	  **Arrays** are enclosed in brackets ([]), and their values are separated by a comma (,). Each value in an array may be of a different type, including another array or an object.
	  
	  When objects and arrays contain other objects or arrays, the data has a tree-like structure.
	  #+END_QUOTE
- Example
	- ```json
	  {
	     "firstName": "Duke",
	     "lastName": "Java",
	     "age": 18,
	     "streetAddress": "100 Internet Dr",
	     "city": "JavaTown",
	     "state": "JA",
	     "postalCode": "12345",
	     "phoneNumbers": [
	        { "Mobile": "111-111-1111" },
	        { "Home": "222-222-2222" }
	     ]
	  }
	  ```
- 处理json格式数据或者文件的命令
	- [[jq]]
- 参考文档
	- [Introducing JSON](https://www.json.org/json-en.html)
	- [The Complete Guide to Working With JSON](https://www.nylas.com/blog/the-complete-guide-to-working-with-json/)
	- [microjson](http://www.catb.org/esr/microjson/)
	- [jsmn](https://github.com/zserge/jsmn)
	- [Introducing JSON](https://www.json.org/json-en.html)
	- [json rfc](https://datatracker.ietf.org/doc/html/rfc8259)
	- [cjson download](https://sourceforge.net/projects/cjson/)
	- [json editor online](https://jsoneditoronline.org/#right=local.yocuhe)