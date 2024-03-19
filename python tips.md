- 省略号
    ```python
    在Python中，一切皆对象，省略号也不例外

    type(...)       # <class 'ellipsis'>
    bool(...)       # True

    def func():
        ...
    等价于
    def func():
        pass
    ```
- 使用 end 来结束代码块
    ```python
    __builtins__.end = None

    def my_abs(x):
        if x > 0:
            return x
        else:
            return -x
        end
    end
    ```
- 可直接运行的 zip 包
    ```python
    $ ls demo
        calc.py  __main__.py  __pycache__
    $ cat demo/calc.py
        def add(x, y):
            return x+y
    $ cat demo/__main__.py
        import calc

        print(calc.add(2, 3))
    $ python3 -m zipfile -c demo.zip demo/*
    $ python3 demo.zip
    5
    ```
- 反斜杠
    ```python
    1.在行尾时，用做续行符
    print("hello "\
        "world")
    2.转义字符
    print('\nhello')

    str3 = "\"      # SyntaxError: EOL while scanning string literal
    str4 = r"\"     # SyntaxError: EOL while scanning string literal
    ```
- 修改解释器提示符
    ```python
    import sys
    print(sys.ps1)
    print(sys.ps2)

    sys.ps1 = "morris>"
    sys.ps2 = "---"
    morris>for i in range(2):
    ---     print(i)
    ---
    0
    1
    ```
- 简洁而优雅的链式比较
    ```python
    False == False == True
    等价于
    False == False and False == True

    if 80 < score <= 90:
        print("xxx")
    ```
- and 和 or 的短路效应
    ```python
    1.当一个 or 表达式中所有值都为真，Python会选择第一个值
    2.当一个 and 表达式 所有值都为真，Python 会选择最后一个值
    >>>(2 or 3) * (5 and 6 and 7)
    14  # 2*7
    ```
- 连接多个列表最极客的方式
    ```python
    >>> a = [1,2]
    >>> b = [3,4]
    >>> c = [5,6]
    >>>
    >>> sum((a,b,c), [])
    [1, 2, 3, 4, 5, 6]
    ```
- 字典居然是可以排序的
    ```python
    在 Python3.6 + 中字典已经是有序的
    # Python3.6.7
    >>> mydict = {str(i):i for i in range(5)}
    >>> mydict
    {'0': 0, '1': 1, '2': 2, '3': 3, '4': 4}
    ```
- 
    ```python
    ```
- 
    ```python
    ```