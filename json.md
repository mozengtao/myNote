[JSON formatter](https://jsonformatter.org/) 

## A complete example
```json
{
  /* ================================
     1. Scalar Types (single values)
     ================================ */
  // String (must use double quotes; escape special characters with backslash)
  "plain_string": "Hello World",
  "string_with_escapes": "Line 1\nLine 2\tTab Character\\Backslash\"Double Quote/'Single Quote'", // Escapes: \n=newline, \t=tab, \\=backslash, \"=double quote
  "string_with_special_chars": "🌍 Unicode Emoji | + - * / = @ # $ % ^ &", // Supports Unicode and special symbols (no escape needed if not reserved)

  // Number (integer, float, scientific notation; no hex/octal/binary in standard JSON)
  "integer_positive": 42,
  "integer_negative": -100,
  "float_basic": 3.14159,
  "float_scientific_notation": 6.02e23, // 6.02 × 10²³ (positive exponent)
  "float_scientific_negative": 1.23e-4, // 1.23 × 10⁻⁴ (negative exponent)
  "float_zero": 0.0,

  // Boolean (only two valid values: true/false, lowercase)
  "boolean_true": true,
  "boolean_false": false,

  // Null (only one value: null, lowercase; represents empty/undefined)
  "null_value": null,

  /* ================================
     2. Collection Types (complex values)
     ================================ */
  // Array (ordered list, enclosed in []; can contain mixed types)
  "simple_array": ["apple", "banana", "cherry"], // Array of strings
  "mixed_type_array": [10, true, "text", null, 3.14], // Array with mixed scalar types
  "nested_array": [ // Nested array (array inside array)
    [1, 2, 3], // Inline sub-array
    [4, 5, 6],
    ["seven", "eight", "nine"]
  ],

  // Object (unordered key-value pairs, enclosed in {}; keys are always double-quoted strings)
  "simple_object": {
    "name": "Alice",
    "age": 30,
    "is_student": false,
    "email": "alice@example.com"
  },
  "nested_object": { // Object inside another object
    "personal_info": {
      "address": {
        "street": "123 Main St",
        "city": "New York",
        "zip_code": "10001"
      },
      "phone": "+1-555-1234" // Preserves special characters (no escape needed here)
    },
    "preferences": {
      "colors": ["red", "blue", "green"],
      "notifications": true
    }
  },
  "array_of_objects": [ // Common pattern: array containing multiple objects
    {
      "id": 1,
      "product": "Laptop",
      "price": 999.99,
      "in_stock": true
    },
    {
      "id": 2,
      "product": "Smartphone",
      "price": 699.99,
      "in_stock": false,
      "features": ["5G", "48MP Camera", "5000mAh Battery"]
    }
  ],

  /* ================================
     3. Edge Cases & Practical Notes
     ================================ */
  "empty_array": [], // Valid empty array
  "empty_object": {}, // Valid empty object
  "number_without_fraction": 10.0, // Equivalent to 10, but explicitly float
  "large_number": 9876543210987654321 // JSON supports arbitrary-precision integers (depends on parser)
}
```

```bash
# JSON (JavaScript Object Notation) is a lightweight data-interchange format. It is easy for humans to read and write. It is easy for machines to parse and generate

# JSON 的数据类型
	1. object
	2. array

# The structure of a JSON object
	Curly braces {} hold objects
	The data are in key, value pairs
	Square brackets [] hold arrays
	Each data element is enclosed with quotes if it‘s a character, or without quotes if it is a numeric value
	Commas are used to separate pieces of data

# JSON Data Types
	string – Literal text that’s enclosed in quotes.
	number – Positive or negative integers or floating point numbers.
	object – A key, value pair enclosed in curly braces
	array – A collection of one or more JSON objects.
	boolean – A value of either true or false with no quotes.
	null – Indicates the absence of data for a key value pair, represented as “null” with no quotes.
```


```json
{ 
  "name":"Katherine Johnson", 
  "age":101,
  "orbital_mechanics": ["trajectories","launch windows","emergency return paths"], 
  "mathmatician": true, 
  "last_location": null 
}

{
  "inventors":[
    { "name":"Katherine Johnson", "age":101, "city":"Newport News" },
    { "name":"Dorothy Vaughan", "age":98, "city":"Hampton" },
    { "name":"Henry Ford", "age":83, "city":"Detroit" }
  ]
}
```

```xml
<?xml version="1.0" encoding="UTF-8" ?>
<inventors>
    <inventor>
        <name>Katherine Johnson</name>
        <age>101</age>
        <city>Newport News</city>
    </inventor>
    <inventor>
        <name>Dorothy Vaughan</name>
        <age>98</age>
        <city>Hampton</city>
    </inventor>
    <inventor>
        <name>Henry Ford</name>
        <age>83</age>
        <city>Detroit</city>
    </inventor>
</inventors>
```

```YAML
inventors:
- name: Katherine Johnson
  age: 101
  city: Newport News
- name: Dorothy Vaughan
  age: 98
  city: Hampton
- name: Henry Ford
  age: 83
  city: Detroit
```

```csv
name,age,city
Katherine Johnson,101,Newport News
Dorothy Vaughan,98,Hampton
Henry Ford,83,Detroit
```

```python
import json

dog_data = {
  "name": "Frieda",
  "is_dog": True,
  "hobbies": ["eating", "sleeping", "barking",],
  "age": 8,
  "address": {
    "work": None,
    "home": ("Berlin", "Germany",),
  },
  "friends": [
    {
      "name": "Philipp",
      "hobbies": ["eating", "sleeping", "reading",],
    },
    {
      "name": "Mitch",
      "hobbies": ["running", "snacking",],
    },
  ],
}

# Serialize obj to a JSON formatted str using this conversion table.
print(json.dumps(dog_data, indent=2, ensure_ascii=False))

with open("dog.json", mode="w", encoding="utf-8") as write_file:
    # Serialize obj as a JSON formatted stream to fp (a .write()-supporting file-like object) using this Python-to-JSON conversion table.
    json.dump(dog_data, write_file, indent=2)

with open("dog.json", mode="r", encoding="utf-8") as read_file:
    # Deserialize fp to a Python object using the JSON-to-Python conversion table.
    dog_info = json.load(read_file)

print(dog_info)
print(dog_info["name"])
```

[Introducing JSON](https://www.json.org/json-en.html)  
[The Complete Guide to Working With JSON](https://www.nylas.com/blog/the-complete-guide-to-working-with-json/)  
[microjson](http://www.catb.org/esr/microjson/)  
[jsmn](https://github.com/zserge/jsmn)  
[Introducing JSON](https://www.json.org/json-en.html)  
[json rfc](https://datatracker.ietf.org/doc/html/rfc8259)  
[cjson download](https://sourceforge.net/projects/cjson/)  
[json editor online](https://jsoneditoronline.org/#right=local.yocuhe)  
[json — JSON encoder and decoder](https://docs.python.org/3/library/json.html) 
[Working With JSON Data in Python](https://realpython.com/python-json/) 
[Python JSON Data: A Guide With Examples](https://www.datacamp.com/tutorial/json-data-python) 
[JSON in Python: How To Read, Write, and Parse](https://python.land/data-processing/working-with-json) 
[Python CSV: Read And Write CSV Files](https://python.land/data-processing/python-csv) 
[]() 
[]() 
[]() 
[]() 
[]() 
[]() 
[]() 