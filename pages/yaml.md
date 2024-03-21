- What is YAML
	> YAML is one of the most popular data serialization languages. Its popularity stems from its simplicity, as well as the fact that it is human-readable and simple to understand.
- YAML Syntax
	> A YAML format primarily uses 3 node types:
	>  
	>  Maps/Dictionaries (YAML calls it mapping):
	>  The content of a mapping node is an **unordered** set of key/value node pairs, with the restriction that each of the keys is unique. YAML places no further restrictions on the nodes.
	>  
	>  Arrays/Lists (YAML calls them sequences):
	>  The content of a sequence node is an **ordered** series of zero or more nodes. In particular, a sequence may contain the same node more than once. It could even contain itself.
	>  
	>  Literals (Strings, numbers, boolean, etc.):
	>  The content of a scalar node is an **opaque** datum that can be presented as a series of zero or more Unicode characters.
- **Indentation**
	> A YAML file relies on whitespace and indentation to indicate nesting.
- **Mapping**
	> Mappings are used to associate key/value pairs that are unordered. Maps can be nested by increasing the indentation, or new maps can be created at the same level by resolving the previous one.
	```yaml
	  name: "YAML Ain't Markup Language" #mapping
	  type: awesome
	  born: 2001
	```
- **Sequences**
	> Sequences in YAML are represented by using the **hyphen (-) and space**. They are ordered and can be embedded inside a map using indentation.
	```yaml
	  languages:
	  #Sequence 
	    - YAML
	    - JAVA
	    - XML
	    - Python
	    - C
	```
- Example
	```yaml
	  ---
	  # key: value [mapping]
	  company: spacelift
	  # key: value is an array [sequence]
	  domain:
	   - devops
	   - devsecops
	  tutorial:
	    - yaml:
	        name: "YAML Ain't Markup Language" #string [literal]
	        type: awesome #string [literal]
	        born: 2001 #number [literal]
	    - json:
	        name: JavaScript Object Notation #string [literal]
	        type: great #string [literal]
	        born: 2001 #number [literal]
	    - xml:
	        name: Extensible Markup Language #string [literal]
	        type: good #string [literal]
	        born: 1996 #number [literal]
	  author: omkarbirade
	  published: true
	```
- [YAML Tutorial : A Complete Language Guide with Examples](https://spacelift.io/blog/yaml)