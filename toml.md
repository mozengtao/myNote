[TOML](https://toml.io/en/)  
[toml](https://pypi.org/project/toml/)  
[Python and TOML: New Best Friends](https://realpython.com/python-toml/)  
[]()  
[]()  
[]()  
[]()  


```python
import toml

toml_string = """
  # This is a comment

  title = "TOML Example"

  [owner]
  name = "John Doe"

  [servers]

    # Indentation (tabs and/or spaces) is allowed but not required
    [servers.alpha]
      ip = "10.0.0.1"
      port = 8000

    [servers.beta]
      ip = "10.0.0.2"
      port = 8001
  
  # Line breaks are OK when inside arrays
  hosts = [
    "host1",
    "host2"
  ]
"""

parsed_toml = toml.loads(toml_string)

with open('config.toml', 'w') as f:
  toml.dump(parsed_toml, f)

print(open('config.toml').read())

with open('config.toml', 'r') as f:
    loaded_toml = toml.load(f)
    print(loaded_toml)
```