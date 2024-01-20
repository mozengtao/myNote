- **Python Enhancement Proposal**
- 命名规范
	- **function** 小写单词+下划线, **ex:** function, my_function
	- **variable** 小写单词+下划线, **ex:** x, var, my_variable
	- **class** start 单词首字母大写且**无**下划线, **ex:** Model, MyClass
	- **method** 小写单词+下划线, **ex:** class_method, method
	- **constant** 大写单词 +下划线, **ex:** CONSTANT, MY_CONSTANT, MY_LONG_CONST
	- **module** 小写单词+下划线, **ex:** module.py, my_module.py
	- **package** 小写单词**无**下划线s, **ex:** package, mypackage
-
- Code Layout
	- Seperate top-level **functions and classes** with **two** blank lines
	- Seperate **method definitions inside classes** with a **single** blank line
	- Use **blank** lines sparingly **inside functions** to show clear steps
	- Breaking **before** binary operators produces more readable code
-
- Code samples
  ```python
  # explicit is better than implicit
  def multiply_by_two(x):
      return x * 2
  
  # blank line
  class MyClass:
      def first_method(self):
          return None
  
      def second_method(self):
          return None
  
  # blank line
  def calculate_variance(number_list):
      sum_list = 0
      for number in number_list:
          sum_list = sum_list + number
      mean = sum_list / len(number_list)
  
      sum_squares = 0
      for number in number_list:
          sum_squares = sum_squares + number**2
      mean_squares = sum_squares / len(number_list)
      
      return mean_squares - mean**2
  
  # indention
  def function(arg_one, arg_two,
               arg_three, arg_four):
      return arg_one
  
  # indention
  total = (first_variable
           + second_variable
           - third_variable)
  
  # indention
  def function(arg_one, arg_two,
               arg_three, arg_four):
      return arg_one
  
  # extra comment to make it more clear
  x = 5
  if (x > 3 and
      x < 10):
      # Both conditions satisfied
      print(x)
  
  
  # hanging indent
  def function(
          arg_one, arg_two,
          arg_three, arg_four):
      return arg_one
  
  # indention
  list_of_numbers = [
      1, 2, 3,
      4, 5, 6,
      7, 8, 9
  ]
  
  # comments
  def quadratic(a, b, c, x):
      # Calculate the solution to a quadratic equation using the quadratic
      # formula.
      #
      # There are always two solutions to a quadratic equation, x_1 and x_2.
      x_1 = (- b+(b**2-4*a*c)**(1/2)) / (2*a)
      x_2 = (- b-(b**2-4*a*c)**(1/2)) / (2*a)
      return x_1, x_2
  
  
  # document string
  def quadratic(a, b, c, x):
  
      """Solve quadratic equation via the quadratic formula.
      A quadratic equation has the following form:
      ax**2 + bx + c = 0
      There always two solutions to a quadratic equation: x_1 & x_2.
      """
      x_1 = (- b+(b**2-4*a*c)**(1/2)) / (2*a)
      x_2 = (- b-(b**2-4*a*c)**(1/2)) / (2*a)
      
      return x_1, x_2
  
  
  # default parameter
  def function(default_parameter=5):
      # ...
  
  # whitespace
  y = x**2 + 5
  z = (x+y) * (x-y
  
  # whitespace
  if x>5 and x%2==0:
      print('x is larger than 5 and divisible by 2!') 
  
  # whitespace
  list[x+1 : x+2 : x+3]
  list[x+1 : x+2 :]
  my_list = [1, 2, 3]
  print(x, y)
  list[3]
  tuple = (1,)
  
  # whitespace
  var1 = 5
  var2 = 6
  some_long_var = 7
  
  # directly check bool value
  if my_bool:
      return '6 is bigger than 5'
  
  # directly check list is empty or not
  my_list = []
  if not my_list:
      print('List is empty!')
  
  # explicitly check for None
  if x is not None:
      return 'x exists!'
  
  
  if arg is not None:
      # Do something with arg...
  
  # use startwith or endwitch to do prefix or suffix check
  if word.startswith('cat'):
      print('The word starts with "cat"')
  
  
  if file_name.endswith('jpg'):
      print('The file is a JPEG')
  ```