- [**C++ Online Compiler**](https://www.mycompiler.io/new/cpp)

- [**C/C++ Programming**](https://www3.ntu.edu.sg/home/ehchua/programming/#Cpp)


- [OOPS Concepts in C++ with Examples](https://www.prepbytes.com/blog/cpp-programming/oops-concepts-in-c-with-examples/)

- [Learn C++ by Example](https://www.manning.com/books/learn-c-plus-plus-by-example)
- [LEARN C++](https://www.learncpp.com/)
- [C++ By Example](https://cppbyexample.com/index.html)
- [LearnCPP](https://github.com/Lakhankumawat/LearnCPP)
- [C-plusplus-Programming-By-Example](https://github.com/PacktPublishing/C-plusplus-Programming-By-Example/tree/master)
- [CPP_Beginner_to_Expert](https://github.com/tridibsamanta/CPP_Beginner_to_Expert/tree/master)

- C++
  collapsed:: true
	- 面向过程
		- 提供低层硬件访问能力
		- C语言强调的是编程的算法方面
			- 结构化编程
			- 自顶向下
	- 面向对象
		- 提供高级抽象
		- 管理大型项目的工具
		- 采用自下向上的编程方法
		- 首先设计类，用来表示程序要处理的对象
			- 信息隐藏
			- 多态
			- 继承
	- 泛型编程
		- 提供执行常见任务(数据排序，合并链表)的工具
		- 独立于特定数据类型，泛型generic指创建独立于类型的代码，C++的模板提供了完成这种任务的机制
- ```cpp
  # 头文件包含
  #include <iostream>
  #include <cmatch> //对应于c语言中的 #include <math.h>
  
  # 使用命名空间
  # 名称空间namespace是一项C++特性，用于区分不同厂商的同名函数，对于编写大型程序是十分必要的
  using namespace std
    
  cout << "C++ RULES" << 2 << endl; // << 运算符重载，可以接收不同类型的输入
  
  a = b = c = 25; // 连续赋值
  
  sizeof(int) // 类型名称必须用括号括起来
  sizeof i    // 变量名称可以不用括号
    
  #include <climits> // 不同整数类型的最大最小值
  int n_int = INT_MAX;
  bool is_ready = true; // bool类型 true/false
  
  const int SIZE = 5; 	// 必须在声明变量的同时进行初始化
  
  l/L: long
  u/U: unsigned int
  ul/UL: unsigned long
  ll/LL: long long
  ull/ULL: unsigned long long
  
  
  ```
- 类
  collapsed:: true
	- 类用来描述数据和对数据可执行的操作
	- 对象是根据数据描述创建的实体，类指定了可对对象执行的所有操作
	- 对特定对象执行操作有两种方法
		- 类方法(本质是函数调用)
		- 定义运算符
- 浮点数的内存表示
  collapsed:: true
	- ```cpp
	  # 浮点数的内存表示，符号位 + 指数位 + 小数位
	  # 以32 bit为例，符号位，指数位和小数位分别为1，8，23
	  float 26.0 (32 bits)
	  1. 转换为二进制 11010.00...0
	  2. 二进制的指数表示形式 1.1010...0 * 2^4
	  3. 确定符号位，指数部分和小数部分
	     符号位: 0
	     指数部分: 127 + 4 = 131
	     小数部分: 10100...0
	  0 10000011 10100000000000000000000
	       
	  12.34
	  2.52E8
	  
	  浮点常量
	  1.234f // float常量
	  2.45E20F // float常量
	  2.345324E28 // double常量
	  2.2L  // long double常量
	  ```
	- 浮点数优缺点
		- 优点
			- 可以表示整数之间的值
			- 表示的范围比整数大得多
		- 缺点
			- 运算速度慢
			- 精度可能会降低
- 自动类型
	- 编译器能够根据初始值的类型推断变量的类型
		- ```c++
		  auto y = 1.3e12L    // y is long double
		  ```
	- 自动推断类型主要是针对复杂类型，如标准模块库（STL）中的类型时，自动类型推断才能显现出来威力
		- ```c++
		  std::vector<double> scores;
		  std::vector<double>::iterator pv = scores.begin()
		  可以重新写做
		  std::vector<double> scores;
		  auto pv = scores.begin()
		  ```
- 内存的申请和释放
	- ```c++
	  int *ps = new int;    // allocate memory with new
	  ...                   // use the memory
	  delete ps;            // free memory with delete when done
	  
	  int *psome = newt int[10];
	  ...
	  delete [] psome;
	  
	  不要使用delete来释放不是new分配的内存。
	  不要使用delete释放同一个内存块两次。
	  如果使用new [ ]为数组分配内存，则应使用delete [ ]来释放。
	  如果使用new 为一个实体分配内存，则应使用delete（没有方括号）来释放。
	  对空指针应用delete是安全的
	  
	  通常数组名被解释为其第一个元素的地址，而对数组名应用地址运算符时，得到的是整个数组的地址
	  short tell[10];          // tell an array of 20 bytes
	  cout << tell << endl;    // display &tell[0]
	  cout << &tell << endl;    // display address of whole array tell
	  ```
- 文件操作
  collapsed:: true
	- 读
		- 包含头文件fstream,头文件fstream定义了一个用于处理输入的ifstream类。
		- 声明ifstream变量（对象）
		- 将ifstream对象与文件关联起来
		- 使用完文件使用close( )方法将其关闭。
		- 可以结合使用ifstream和eof( )、fail( )等方法来判断输入是否成功。ifstream对象本身被用作测试条件时，如果最后一个读取操作成功，它将被转换为布尔值true，否则被转换为false
		- ```c++
		  #include <fstream>
		  using namespace std;
		  ifstream inFile;
		  inFile.open(fileName);
		  if(!inFile.is_open())
		      error handling;
		  inFile >> value;     // get first value
		  while(inFile.good())
		      read more data
		  if(inFile.eof())
		      cout << "end of file";
		  else if(inFile.fail())
		      cout << "data mismatch";
		  else
		      cout << "unknown reason";
		  inFile.close();
		  
		  可以将
		  inFile >> value;     // get first value
		  while(inFile.good())
		      read more data
		  简化为
		  while(inFile >> value)
		      read Data
		  ```
	- 写
	  collapsed:: true
		- 包含头文件fstream
		- 创建一个ofstream对象
		- 将该ofstream对象同一个文件关联起来
		- 就像使用cout那样使用该ofstream对象
		- ```c++
		  #include <fstream>
		  using namespace std;
		  
		  ofstream outFile;
		  outFile.open("out.txt");
		  // display information on screen with cout
		  cout << fixed;
		  cout.precision(2);
		  cout.setf(ios_base::showpoint);
		  cout << 1.2345;
		  cout << "end";
		  
		  // do exact same things using outFile instead of cout
		  outFile << fixed;
		  outFile.precision(2);
		  outFile.setf(ios_base::showpoint);
		  outFile << 1.2345;
		  outFile << "end";
		  
		  outFile.close();
		  ```
- 引用
	- 引用变量的主要用途是**用作函数的形参**。通过将引用变量用作参数，**函数将使用原始数据**，而不是其副本。
	- 引用变量必须在声明的同时进行初始化操作
		- ```c++
		  int a;
		  int &b = a;
		  ```
	- 引用参数传递的优点
		- 程序员能够**修改调用函数中的数据对象**
		- 通过传递引用而不是整个数据对象，可以**提高程序的运行速度**
	- 尽可能将引用形参声明为const
		- 使用const可以避免无意中修改数据的编程错误
		- 使用const使函数能够处理const和非const实参，否则将只能接受非const数据
		- 使用const引用使函数能够正确生成并使用临时变量
	- 指针或者引用作为形参
		- 对于使用传递的值而不作修改的函数
			- 如果数据对象很小，如内置数据类型或**小型结构，则按值传递**
			- **如果数据对象是数组，则使用指针**，因为这是唯一的选择，并将指针声明为指向const的指针
			- **如果数据对象是较大的结构，则使用const指针或const引用**，以提高程序的效率。这样可以**节省复制结构所需的时间和空间**
			- **如果数据对象是类对象，则使用const引用**。类设计的语义常常要求使用引用，这是C++新增这项特性的主要原因。因此，**传递类对象参数的标准方式是按引用传递**
		- 对于修改调用函数中数据的函数
			- 如果数据对象是内置数据类型，则使用指针。如果看到诸如fixit（&x）这样的代码（其中x是int），则很明显，该函数将修改x
			- 如果数据对象是数组，则只能使用指针
			- 如果数据对象是结构，则使用引用或指针
			- 如果数据对象是类对象，则使用引用
- 默认参数
	- 使用不同数目的参数调用同一个函数
	- 只有函数原型指定默认值，函数定义与默认参数无关
- 函数重载
	- 为了使用多个同名的函数
	- 定义多个同名函数，但是函数的特征不同(**参数数目+参数类型+参数排列顺序**)，参数名和函数返回类型则是无关紧要的，即对函数名称进行了重载
	- 仅当**函数执行相同的任务，但使用不同形式的数据时**，才应采用函数重载
	- ```c++
	  void print(const char *str, int width);    // #1
	  void print(double d, int width);    // #2
	  void print(long l, int width);        // #3
	  void print(int i, int width);        // #4
	  void print(const char *str);        // #5
	  
	  
	  print("Pancakes", 15);            # uses #1
	  print("Pancakes");            # uses #5
	  print(1999.0, 10);            # uses #2
	  print(1999, 12);            # uses #4
	  print(1999L, 15);            # uses #3
	  如果函数调用不与任何原型匹配则C++将尝试使用标准类型转换强制进行匹配
	  
	  注意
	  double cube(double x);
	  doulbe cube(double &x);
	  不是函数重载，因为对于函数调用cube(x)编译器无法确定究竟使用哪个原型，
	  因此编译器在检查函数特征标时，将把类型引用和类型本身视为同一个特征标
	  
	  
	  重载引用参数
	  void sink(double &r1);        // matches modifiable lvalue
	  void sink(const double &r2);    // matches modifiable or const lvalue, rvalue
	  void sink(double &&r3);        // matches rvalue
	  ```
- 函数模板
	- 解决不同类型使用同一种算法函数的需求
	- 使用泛型定义函数，将类型作为参数传递给模板，编译器使用模板为特定类型生成函数定义，得到模板实例(instantiation)，隐式实例化(implicit instantiation)：编译器根据程序调用提供的参数类型，生成针对特定类型的函数定义。
	- 类型用参数表示，因此模板特性也称为参数化类型(parameterized types)
	- 模板不创建函数，而是告诉编译器如何定义函数
	- 函数模板不减小可执行程序，最终仍然将有多个独立的函数定义，最终的代码不包含任何模板，而只包含为程序生成的实际函数，使用模板的好处是为了是生成多个函数定义更简单可靠
	- ```c
	  // function template prototype
	  template <typename T>
	  void swap(T &a, T &b);
	  
	  // function template definition
	  template <typename T>
	  void swap(T &a, T &b)
	  {
	      T temp;
	      temp = a;
	      a = b;
	      b = temp;
	  }
	  ```
	- 通常将模板放在头文件中，在需要使用模板的文件中包含头文件
- 重载模板
	- 解决不同类型使用不同算法的需求，但函数名相同
	- 与常规重载类似，被重载的模板的函数特征标必须不同
	- ```c++
	  template <typename T>    // original template
	  void swap(T &a, T &b);
	  
	  template <typename T>    // new template
	  void swap(T *a, T *b, int n);
	  
	  template <typename T>
	  void swap(T &a, T &b)
	  {
	      T temp;
	      temp = a;
	      a = b;
	      b = temp;
	  }
	  
	  template <typename T>
	  void swap(T *a, T *b, int n)
	  {
	      T temp;
	      for(int i = 0; i < n; i++) {
	          temp = a[i];
	          a[i] = b[i];
	          b[i] = temp;
	      }
	  }
	  ```
	- 需要注意的是，编写的模板函数很可能无法处理某些类型的数据，例如模板函数`void swap(T &a, T &b)`通常无法处理数组类型的参数。解决方法是显式具体化(explicit specialization)
- 显式具体化
	- 提供一个具体化函数定义，当编译器找到与函数调用匹配的具体化定义时则使用该定义，而不再继续查找模板函数
	- ```c++
	  template <> void swap<int>(int &, int &);    // explicit specialization
	  template <> void swap(int &, int &);        // explicit specialization
	  “不要使用Swap( )模板来生成函数定义，而应使用专门为int类型显式地定义的函数定义”。
	  这些原型必须有自己的函数定义。显式具体化声明在关键字template后包含<>，而显式实例化没有。
	  ```
- 对于给定函数名，可能存在如下匹配的函数类型
	- 非模板函数
	- 模板函数
	- 显示具体化模板函数
	- 以上函数的重载版本
	- 非模板函数优先级高于具体化模板，具体化模板优先级高于常规模板
	- ```c++
	  // 非模板函数
	  void swap(job &, job &);
	  
	  // 模板函数
	  template <typename T>
	  void swap(T &, T &);
	  
	  // 针对job类型的显式具体化
	  // explicit specialization for the job type
	  template <> void swap<job>(job &, job &);
	  template <> void swap(job &, job &);    // simpler form
	  ```
- 显式实例化
	- explicit instantiation
	- 无需给该函数重新编写函数体，这只是个声明，目的是为了提高程序执行的效率，在使用模板之前，编译器根据显式实例化指定的类型生成模板实例，相当于本程序里有了针对该类型的函数实现，否则每次调用针对该类型的代码时都需要重新生成该类型的代码
	- 显式实例化声明
		- ```c++
		  // explicit instantiation 显式实例化声明在关键字template后不包含<>
		  template void swap<int>(int, int);
		  ```
	- 注意，在同一个文件中同时使用同一种类型的显式具体化和显式实例化将出错
- 不同具体化函数的比较
	- 隐式实例化
		- 使用模板之前，编译器不生成模板实例，后面有程序用到了编译器才会根据模板生成模板实例
	- 显式实例化
		- 无论程序是否用到，编译器都会生成实例函数
	- 具体实例化
		- 针对某些类型，不适合函数模板来实现，需要重新定义实现，此时使用具体实例化
	- ```c++
	  #include <iostream>
	  #include <string>
	  using namespace std;
	  
	  struct job
	  {
	          string name;
	          int salary;
	          job(string _name, int _salary) : name(_name), salary(_salary) {};
	  };
	  
	  // 函数模板
	  template <typename T>
	  void Swap(T &a, T &b)
	  {
	          T temp;
	          temp = a;
	          a = b;
	          b = temp;
	  }
	  
	  // 针对 job 类型的显示具体化
	  template <> void Swap<job>(job &a, job &b)
	  {
	          int temp;
	          temp = a.salary;
	          a.salary = b.salary;
	          b.salary = temp;
	  }
	  
	  // 针对int类型的显式实例化
	  template void Swap<int>(int &, int &);
	  int main(void)
	  {
	          char a = 'a', b = 'b';
	          cout << "a:" << a << " b:" << b << endl;
	          Swap(a, b);     // 针对char类型的隐式实例化
	          cout << "a:" << a << " b:" << b << endl;
	  
	          int c = 1, d = 2;
	          cout << "c:" << c << " d:" << d << endl;
	          Swap(c, d);     // 针对 int 类型显式实例化
	          cout << "c: " << c << " d:" << d << endl;
	  
	          Swap<int>(c, d);        // 针对 int 类型显式实例化
	          cout << "c: " << c << " d:" << d << endl;
	  
	          job e("lucy", 100), f("bob", 200);
	          cout << "lucy:" << e.name << " " << e.salary << "; bob: " << f.name << " " << f.salary << endl;
	          Swap(e, f);     //针对 job 类型的显示具体化
	          cout << "lucy:" << e.name << " " << e.salary << "; bob: " << f.name << " " << f.salary << endl;
	  }
	  ```
- 重载解析
	- 编译器决定为函数调用使用哪一个函数定义的具体策略
	- 步骤
		- **创建候选函数列表**。其中包含与被调用函数的**名称相同的函数和模板函数**
		- 使用候选函数列表**创建可行函数列表**。这些都是参数数目正确的函数，为此有一个隐式转换序列，其中包括实参类型与相应的形参类型完全匹配的情况。例如，使用float参数的函数调用可以将该参数转换为double，从而与double形参匹配，而模板可以为float生成一个实例
		- **确定是否有最佳的可行函数**。如果有，则使用它，否则该函数调用出错
			- **完全匹配**，但常规函数优先于模板
			- **提升转换**（例如，char和shorts自动转换为int，float自动转换为double）
			- **标准转换**（例如，int转换为char，long转换为double）
			- **用户定义的转换**，如类声明中定义的转换
- 名称空间
	- 控制名称的作用域
	- ```c++
	  namespace Jack {
	      double pail;        // variable declaration
	      void fetch();        // function prototype
	      int pal;        // variable declaration
	      struct Well {...};    // structure declaration
	  }
	  
	  
	  namespace Jill {
	      double bucket(double n_int) {...}    // function definition
	      double fetch;                // variable declaration
	      int pal;                // variable declaration
	      struct Hill {...};            // structure declaration
	  }
	  ```
	- 名称空间可以是全局的，也可以位于另一个名称空间中，但不能位于代码块中
	- 名称空间的主要是为了简化大型编程项目的管理工作
	- 指导原则
		- 使用在已命名的名称空间中声明的变量，而不是使用外部全局变量。
		- 使用在已命名的名称空间中声明的变量，而不是使用静态全局变量。
		- 如果开发了一个函数库或类库，将其放在一个名称空间中。事实上，C++当前提倡将标准函数库放在名称空间std中，这种做法扩展到了来自C语言中的函数。例如，头文件math.h是与C语言兼容的，没有使用名称空间，但C++头文件cmath应将各种数学库函数放在名称空间std中。实际上，并非所有的编译器都完成了这种过渡
		- 仅将编译指令using作为一种将旧代码转换为使用名称空间的权宜之计。
		- 不要在头文件中使用using编译指令。首先，这样做掩盖了要让哪些名称可用；另外，包含头文件的顺序可能影响程序的行为。如果非要使用编译指令using，应将其放在所有预处理器编译指令`#include`之后。
		- 导入名称时，首选使用作用域解析运算符或using声明的方法。
		- 对于using声明，首选将其作用域设置为局部而不是全局
- 声明区域
	- declaration region
	- 可以在其中进行声明的区域
		- 例如，可以在函数外面声明全局变量，对于这种变量，其声明区域为其声明所在的文件。对于在函数中声明的变量，其声明区域为其声明所在的代码块
- 作用域
	- scope
	- 变量对程序而言的可见范围
		- 潜在作用域
			- potential scope
			- 从声明点开始，到其声明区域的结尾
			- 潜在作用域比声明区域小，因为它可能被另一个在嵌套声明区域中声明的同名变量隐藏
- 类
	- 类声明
	  collapsed:: true
		- 以数据成员描述数据部分，以成员函数描述公有接口
	- 类方法定义
	  collapsed:: true
		- 描述如何实现成员函数
		- ```c++
		  // stock00.h -- Stock class interface
		  // version 00
		  #ifndef STOCK00_H_
		  #define STOCK00_H_
		  
		  #include <string>
		  
		  class Stock    // class declaration
		  {
		  private:
		      std::string company;
		      long shares;
		      double share_val;
		      double total_val;
		      void set_tot() { total_val = shares * share_val; }
		  public:
		  // two constructors
		      Stock();    // default constructor
		      Stock(const std::string &co, long n = 0; double pr = 0.0);
		      ~Stock();    // noisy destructor
		      void acquire(const std::string &co, long n, doulbe pr);
		      void buy(long num, double price);
		      void sell(long num, double price);
		      void update(double price);
		      void show() const;    // promises not to change invoking object
		  };    // note semicolon at the end
		  ```
	- 类成员访问控制
	  collapsed:: true
		- private
		- public
		- 类对象可以直接访问共有部分，但是只能通过共有成员函数或者友元函数访问对象的私有成员
		- 公有成员函数是程序和对象私有成员之间的桥梁，提供了对象和程序之间的接口，防止了程序直接访问私有数据，公有成员函数可以直接访问对象的私有成员
	- 内联方法
	  collapsed:: true
		- 位于类声明中的函数自动称为内联函数
	- 构造函数
	  collapsed:: true
		- 显示调用构造函数
		  collapsed:: true
			- ```c++
			  Stock food = Stock("World Cabbage", 250, 1.25);
			  ```
		- 隐式调用构造函数
		  collapsed:: true
			- ```c++
			  Stock food("World Cabbage", 250, 1.25);
			  ```
		- 默认构造函数
		  collapsed:: true
			- 在用户未显式提供初始值时，会调用默认构造函数，用来创建对象，不使用圆括号
			  collapsed:: true
				- ```c++
				  Stock fluffy_the_cat;
				  ```
			- 当且仅当没有定义任何构造函数时，编译器才会提供默认构造函数。为类定义了构造函数后，程序员就必须为它提供默认构造函数
	- 析构函数
	  collapsed:: true
		- 对象过期时，程序自动调用的特殊成员函数
		- 通常析构函数由编译器决定何时调用，而不应该在代码中显式调用
	- const类型的成员函数
	  collapsed:: true
		- 用来保证函数不会修改调用对象
		- ```c++
		  // declaration
		  void show() const;
		  
		  // definition
		  void stock::show() const
		  {
		  
		  }
		  ```
	- this指针
	  collapsed:: true
		- 指向调用函数的对象本身，this作为隐藏参数传递给成员函数，通常所有类方法将this指针设置为调用对象的地址，例如total_val只不过是`this->total_val`的简写
	- 类作用域
	  collapsed:: true
		- 定义类作用域内的常量
		  collapsed:: true
			- ```c++
			  class Bakery
			  {
			  private:
			      enum {Months = 12};
			    // 用这种方式声明枚举并不会创建类数据成员, Months只是一个符号名称，
			    // 在作用域为整个类的代码中遇到它时，编译器将用30来替换它
			  }
			  
			  class Bakery
			  {
			  private:
			      static const int Months = 12;
			      // 创建一个名为Months的常量，该常量将与其他静态变量存储在一起，而不是存储在对象中
			  }
			  ```
	- 作用域内枚举
	  collapsed:: true
		- ```c++
		  enum class egg {Small, Medium, Large, Junbo};
		  enum class t_shirt {Small, Medium, Large, Xlarge};
		  
		  enum choice = egg:Large;    // the Large enumerator of the egg enum
		  t_shirt Floyd = t_shirt::Large    // the Large enumerator of the t_shirt enum
		  ```
	- 类的类型转换
	  collapsed:: true
		- 自动转换
			- ```c++
			  对于如下的构造函数
			  Stonewt::Stonewt(double lbs)
			  {...}
			  可以编写这样的代码
			  Stonewt myCat;        // create a Stonewt object
			  myCat = 19.6;        // use Stonewt(double) to convert 19.6 to Stonewt
			  
			  隐式转换:
			    将Stonewt对象初始化为double值时。
			    将double值赋给Stonewt对象时。
			    将double值传递给接受Stonewt参数的函数时。
			    返回值被声明为Stonewt的函数试图返回double值时。
			  在上述任意一种情况下，使用可转换为double类型的内置类型时
			  ```
		- 强制转换
			- 在某些时候这种自动转换特性并非总是合乎需要的，因为这会导致意外的类型转换,C++新增了关键字explicit，用于关闭这种自动特性
				- ```c++
				  explicit Stonewt(double lbs);    // no implicit conversions allowed
				  
				  Stonewt myCat;        // create a Stonewt object
				  myCat = 19.6;        // not valid if Stonewt(double) is declared as explicit
				  myCat = Stonewt(19.6);    // ok, an explicit conversion
				  myCat = (Stonewt)19.6;    // ok, old form for explicit typecast
				  ```
			- 转换函数
				- 用户定义的强制类型转换，可以像使用强制类型转换那样使用它们
					- 转换函数必须是类方法
					- 转换函数不能指定返回类型
					- 转换函数不能有参数
					- 类型转换函数通常应该是 const
				- 语法
					- `operator typeName();`
				- 示例
					- ```c++
					  // conversion functions
					      operator int() const;
					      operator double() const;
					  
					  Stonewt::operator int() const
					  {
					      return int(pounds + 0.5);
					  }
					  
					  Stonewt::operator double() const
					  {
					      return pounds;
					  }
					  ```
			- 转换函数 or 友元函数
				- ```c++
				  将double量和Stonewt量相加，有两种选择
				  1. 定义成员函数，让Stonewt(double)构造函数将double类型的参数转换为Stonewt类型的参数
				  operator+(const Stonewt &, const Stonewt &);
				  2. 将加法运算符重载为一个显式使用double类型参数的函数
				  Stonewt operator+(double x);    // member function
				  friend Stonewt operator+(double x, Stonewt &s);
				  
				  这样
				  total = jennySt + kennyD;    // Stonewt + double
				  与成员函数
				  operator+(double x)
				  完全匹配
				  
				  total = kennyD + jennySt;    // double + Stonewt
				  与友元函数
				  operator+(double x, Stonewt &s)
				  完全匹配
				  
				  第一种方法（依赖于隐式转换）使程序更简短，缺点是，每次需要转换时，都将调用转换构造函数，这增加时间和内存开销
				  第二种方法（增加一个显式地匹配类型的函数）程序较长，但运行速度较快
				  ```
	- 类和动态内存分配
	  collapsed:: true
		- 存在问题的string类实现
		  collapsed:: true
			- ```c++
			  // stringbad.h
			  #include <iostream>
			  #ifndef STRINGBAD_H_
			  #define STRINGBAD_H_
			  
			  class StringBad
			  {
			  private:
			      char *str;        // pointer to string
			      int len;        // length of string
			      static int num_strings;    // number of objects
			  public:
			      StringBad(const char *s);    // constructor
			      StringBad();            // default constructor
			      ~StringBad();            // destructor
			  // friend function
			      friend std::ostream & operator<<(std::ostream &os, const StringBad &st);
			  }
			  
			  #endif
			  
			  // stringbad.cpp -- StringBad class methods
			  #include <cstring>
			  #include "stringbad.h"
			  using std:cout;
			  
			  // initializing static class member
			  int StringBad::num_strings = 0;
			  
			  // class methods
			  StringBad::StringBad(const char *s)
			  {
			      len = std::strlen(s);
			      str = new char[len + 1];
			      std::strcpy(str, s);
			      num_strings++;
			      cout << "num_strings: " << num_strings << "\n";
			  }
			  
			  StringBad::StringBad()
			  {
			      len = 4;
			      str = new char[4];
			      std::strcpy(str, "C++");
			      num_strings++;
			      cout << "num_strings: " << num_strings << "\n";
			  }
			  
			  StringBad::~StringBad()
			  {
			      cout << "\"" << str << "\" object deleted, ";
			      --num_strings;
			      cout << num_strings << " objects left\n";
			      delete [] str;
			  }
			  
			  std::ostream & operator<<(std::ostream &os, const StringBad &st)
			  {
			      os << st.str;
			      return os;
			  }
			  ```
		- 存在的问题
		  collapsed:: true
			- 程序无法准确地记录对象计数
			- ```c++
			  StringBad sailor = sports;
			  等价于
			  StringBad sailor = StringBad(sports);    // construct using sports
			  对应的构造函数原型
			  StringBad(const StringBad &);
			  (当使用一个对象来初始化另一个对象时，编译器将自动生成上述构造函数(复制构造函数))，
			  该构造函数不会自动更新num_strings，会导致计数变乱
			  ```
				- 如何解决问题
					- 提供一个对计数进行更新的显式复制构造函数
					- ```c++
					  StringBad::StringBad(const StringBad &s)
					  {
					      num_strings++;
					      ...
					  }
					  ```
			- 字符串内容出现乱码
				- ```c++
				  隐式复制构造函数是按值进行复制，隐式复制构造函数的功能相当于：
				  sailor.str = sport.str;
				  这里复制的并不是字符串，而是一个指向字符串的指针
				  另一个症状是，析构函数StringBad释放str指针指向的内存
				  之后试图第二次释放内存则可能导致程序异常终止
				  ```
				- 如何解决问题
					- 定义一个显式复制构造函数进行深度复制
					- ```c++
					  StringBad::StringBad(const StringBad &st)
					  {
					      num_strings++;
					      len=st.len;
					      str = new char[len+1];
					      std::strcpy(str, st.str);
					  }
					  必须定义复制构造函数的原因在于，一些类成员是使用new初始化的、指向数据的指针，
					  而不是数据本身
					  ```
		- 类的默认成员函数
		  collapsed:: true
			- 如果用户没有定义，编译器自动为用户生成的类成员函数
				- 默认构造函数
					- 只能有1个默认构造函数
					- 只要所有参数都有默认值，默认构造函数也可以带参数
				- 默认析构函数
				- 复制构造函数
					- 用于将一个对象复制到新创建的对象中,它用于初始化过程中(包括按值传递)
					- 新建一个对象并将其初始化为同类现有对象时，复制构造函数都将被调用
					- ```c++
					  StringBad ditto(motto);
					  StringBad metoo = motto;
					  StringBad also = StringBad(motto);
					  StringBad *pStringBad = new StringBad(motto);
					  
					  每当程序生成了对象副本时，编译器都将使用复制构造函数,
					  即当函数按值传递对象或函数返回对象时，都将使用复制构造函数
					  ```
					- 默认复制构造函数
						- 逐个复制非静态成员（成员复制也称为浅复制），复制的是成员的值
						- 如果成员本身就是类对象，则将使用这个类的复制构造函数来复制成员对象。静态函数（如num_strings）不受影响，因为它们属于整个类，而不是各个对象
						- ```c++
						  StringBad sailor = sports;
						  等价于
						  StringBad sailor;
						  sailor.str = sports.str;
						  sailor.len = sports.len;
						  ```
				- 赋值运算符
					- `Class_name & Class_name::operator=(const Class_name &);`
					- 将已有的对象赋给另一个对象时，将使用重载的赋值运算符
						- ```c++
						  StringBad headline1("ABC");
						  StringBad knot;
						  knot = headline1;    // assignment operator invoked
						  ```
					- 与复制构造函数相似，赋值运算符的隐式实现也对成员进行逐个复制,如果成员本身就是类对象，则程序将使用为这个类定义的赋值运算符来复制该成员，但静态数据成员不受影响
					- 赋值运算符（进行深度复制）定义,其实现与复制构造函数相似，但也有一些差别，由于目标对象可能引用了以前分配的数据，所以函数应使用delete[]来释放这些数据，函数应当避免将对象赋给自身；否则，给对象重新赋值前，释放内存操作可能删除对象的内容，函数返回一个指向调用对象的引用
						- ```c++
						  StringBad & StringBad::operator=(const StringBad &st)
						  {
						      if(this == &st)            // object assigned to itself
						          return *this;        // all done
						      delete [] str;            // free old string
						      len = st.len;
						      str = new char[len + 1];    // get space for new string
						      std:strcpy(str, st.str);    // copy the string
						      return *this;            // return reference to invoking object
						  }
						  ```
					- 赋值运算符是只能由类成员函数重载的运算符之一,赋值操作并不创建新的对象，因此不需要调整静态数据成员num_strings的值
				- 地址运算符
				- 对于复制构造函数，赋值运算符和地址运算符，如果程序使用对象的方式要求这样做，例如将一个对象赋值给另一个对象，编译器将生成函数的定义
		- 静态类成员函数
		  collapsed:: true
			- 静态成员函数不与特定的对象相关联，因此只能使用静态数据成员
			- ```c++
			  类声明中的原型定义：
			  static int HowMany() { return num_strings; }
			  调用方式：
			  int count = String::HowMany();    // invoking a static member function
			  ```
		- 改进StringBad类
		  collapsed:: true
			- ```c++
			  // 功能完善
			  int length() const { return len; }
			  friend bool operator<(const String &st, const String &str2);
			  friend bool operator>(const String &st, const String &str2);
			  friend bool operator==(const String &st, const String &str2);
			  friend operator>>(istream &is, String &st);
			  char & operator[](int i);
			  const char & operator[](int i) const;
			  static int HowMany();
			  
			  // string1.h -- fixed and augmented string class definition
			  
			  #ifndef STRING1_H_
			  #define STRING1_H_
			  
			  #include <iostream>
			  using std::ostream;
			  using std::istream;
			  
			  class String
			  {
			  private:
			      char *str;            // pointer to string
			      int len;            // length of string
			      static int num_strings;        // number of objects
			      static const int CINLIM = 80;    // cin input limit
			  public:
			  // constructors and other methods
			      String(const char *s);        // constructor
			      String();            // default constructor
			      String(const String &);        // copy constructor
			      ~String();            // destructor
			      int length() const { return len; }
			  // overloaded operator methods
			      String & operator=(const String &);
			      String & operator=(const char *);
			      char & operator[](int i);
			      const char & operator[](int i) const;
			  // overloaded operator friends
			      friend bool operator<(const String &st, const String &str2);
			      friend bool operator>(const String &st, const String &str2);
			      friend bool operator==(const String &st, const String &str2);
			      friend ostream & operator<<(ostream &os, const String &st);
			      friend istream & operator>>(istream &is, String &st);
			  // static function
			      static int HowMany();
			  }
			  #endif
			  
			  
			  // string1.cpp -- String class methods
			  #include <cstring>
			  #include "string1.h"
			  
			  using std::cin;
			  using std::cout;
			  
			  // initializing static class member
			  
			  int String::num_strings = 0;
			  
			  // static method
			  int String::HowMany()
			  {
			      return num_strings;
			  }
			  
			  // class methods
			  String::String(const char *s)
			  {
			      len = std::strlen(s);
			      str = new char[len + 1];
			      std::strcpy(str, s);
			      num_strings++;
			  }
			  
			  String::String()
			  {
			      len = 4;
			      str = new char[1];
			      str[0] = '\0';
			      num_strings++;
			  }
			  
			  String::String(const String &st)
			  {
			      num_strings++;
			      len = st.len;
			      str = new char[len + 1];
			      std::strcpy(str, st.str);
			  }
			  
			  String::~String()
			  {
			      --num_strings;
			      delete [] str;
			  }
			  
			  // overloaded operator methods
			  // assign a String to a String
			  String & String::operator=(const String &st)
			  {
			      if(this == &st)
			          return *this;
			      delete [] str;
			      len = st.len;
			      str = new char[len + 1];
			      std::strcpy(str, st.str);
			      return *this;
			  }
			  
			  // assign a C string to a String
			  String & String::operator=(const char *s)
			  {
			      delete [] str;
			      len = std::strlen(s);
			      str = new char[len + 1];
			      std::strcpy(str, s);
			      return *this;
			  }
			  
			  // read-write char access for non-const String
			  char & String::operator[](int i)
			  {
			      return str[i];
			  }
			  
			  // read-only char access for non-const String
			  const char & String::operator[](int i) const
			  {
			      return str[i];
			  }
			  
			  
			  // overloaded operator friends
			  bool operator<(const String &st1, const String &st2)
			  {
			      return (std::strcmp(st1.str, st2.str) < 0);
			  }
			  
			  bool operator>(const String &st1, const String &st2)
			  {
			      return st2 < st1;
			  }
			  
			  bool operator==(const String &st1, const String &st2)
			  {
			      return (std::strcmp(st1.str, st2.str) == 0);
			  }
			  
			  // simple String output
			  ostream & operator<<(ostream &os, const String &st)
			  {
			      os << st.str;
			      return os;
			  }
			  
			  // quick and dirty String input
			  istream & operator>>(istream &is, String &st)
			  {
			      char temp[String::CINLIM];
			      is.get(temp, String::CINLIM);
			      if(is)
			          st = temp;
			      while(is && is.get() != '\n')
			          continue;
			      return is;
			  }
			  ```
	- 成员初始化列表
	  collapsed:: true
		- 提出问题：对于const数据成员，必须在执行到构造函数体之前，即创建对象时进行初始化
		- 解决问题：成员初始化列表
		- 成员初始化列表由逗号分隔的初始化列表组成（前面带冒号）
		- 成员初始化列表位于参数列表的右括号之后、函数体左括号之前
		- 初值可以是常量或构造函数的参数列表中的参数
		- ```c++
		  Queue::Queue(int qs) : qsize(qs)    // initialize qsize to qs
		  {
		      front = rear = NULL;
		      items = 0;
		  }
		  
		  Queue::Queue(int qs) : qsize(qs), front(NULL), rear(NULL), items(0)
		  {
		  }
		  ```
		- 只有构造函数可以使用这种初始化列表语法
		- 对于非静态const数据成员，必须使用这种语法
		- 对于引用数据成员，也必须使用这种语法(因为引用与const数据类似，只能在被创建时进行初始化)
		- 数据成员被初始化的顺序与它们出现在类声明中的顺序相同，与初始化器中的排列顺序无关
			- `Classy::Classy(int n, int m) : mem1(n), mem2(0), mem3(n*m + 2)`
		- C++11的类内初始化
			- ```c++
			  class Classy
			  {
			      int mem1 = 10;        // in-class initialization
			      const int mem2 = 20;    // in-class initialization
			  }
			  与在构造函数中使用成员初始化列表等价：
			  Classy::Classy() : mem1(n), mem2(20) {...}
			  ```
	- 类定义示例
	  collapsed:: true
		- 用队列类Queue模拟ATM
			- 队列存储有序的项目序列
			- 队列所能容纳的项目数有一定的限制
			- 应当能够创建空队列
			- 应当能够检查队列是否为空
			- 应当能够检查队列是否是满的
			- 应当能够在队尾添加项目
			- 应当能够从队首删除项目
			- 应当能够确定队列中项目数
		- 代码
		  collapsed:: true
			- ```c++
			  class Queue
			  {
			  private:
			  // class scope definitions
			      // Node is a nested structure definition local to this class
			      struct Node { Item item; struct Node *next; };
			      enum { Q_SIZE = 10 };
			  // private class members
			      Node *front;
			      Node *rear;
			      int items;
			      const int qsize;
			  public:
			      Queue(int qs = Q_SIZE);    // create queue with a qs limit
			      ~Queue();
			      bool isempty() const;
			      bool isfull() const;
			      int queuecout() const;
			      bool enqueue(const Item &item);
			      bool dequeue(Item &item);
			  };
			  
			  bool Queue::enqueue(const Item &item)
			  {
			      if(isfull())
			          return false;
			      Node *add = new Node;
			      add->item = item;
			      add->next = NULL;
			      items++;
			      if(front == NULL)
			          front = add;
			      else
			          rear->next = add;
			      rear = add;
			      return true;
			  }
			  
			  bool Queue::dequeue(Item &item)
			  {
			      if(front == NULL)
			          return false;
			      item = front->item;
			      items--;
			      Node *temp = front;
			      front = front->next;
			      delete temp;
			      if(items == 0)
			          rear = NULL;
			      return true;
			  }
			  
			  Queue::~Queue()
			  {
			      Node *temp;
			      while(front != NULL) {
			          temp = front;
			          front = front->next;
			          delete temp;
			      }
			  }
			  
			  Class Queue
			  {
			  private:
			      Queue(const Queue &q) : qsize(0) {}    // preemptive definition
			      Queue & operator=(const Queue &q) { return *this; }
			  //...
			  };
			  这样做有两个作用：
			  第一，它避免了本来将自动生成的默认方法定义。
			  第二，因为这些方法是私有的，所以不能被广泛使用。
			  也就是说，如果nip和tuck是Queue对象，则编译器就不允许这样做
			  Queue snick(nip);    // not allowed
			  tuck = nip;        // not allowed
			  
			  Customer类：
			  class Customer
			  {
			  private:
			      long arrive;        // arrival time for customer
			      int processtime;    // process time for customer
			  public:
			      Customer() { arrive = processtime = 0; }
			      void set(long when);
			      long when() const { return arrive; }
			      int ptime() const { return processtime; }
			  }
			  
			  void Customer::set(long when)
			  {
			      processtime = std::rand() % 3 + 1;
			      arrive = when;
			  }
			  
			  // queue.h -- interface for a queue
			  #ifndef QUEUE_H_
			  #define QUEUE_H_
			  // This queue will contain Customer items
			  class Customer
			  {
			  private:
			      long arrive;
			      int processtime;
			  public:
			      Customer() { arrive = processtime = 0; }
			      void st(long when);
			      long when() const { return arrive; }
			      int ptime() const { return processtime; }
			  };
			  
			  typedef Customer Item;
			  
			  class Queue
			  {
			  private:
			  // class scope definitions
			      // Node is a nested structure definition local to this class
			      struct Node { Item item; struct Node *next; };
			      enum { Q_SIZE = 10 };
			  // private class members
			      Node *front;
			      Node *rear;
			      int items;
			      const int qsize;
			      // preemptive definitions to prevent public copying
			      Queue & operator=(const Queue & q) { return *this; }
			  public:
			      Queue(int qs = Q_SIZE);
			      ~Queue();
			      bool isempty() const;
			      bool isfull() const;
			      int queuecount() const;
			      bool enqueue(const Item &item);
			      bool dequeue(Item &item);
			  }
			  #endif
			  ```
			-
	- 类继承
		- 从已有的类派生出新的类，而派生类继承了原有类（称为基类）的特征，包括方法
		- 派生类对象存储了基类的数据成员
		- 派生类对象可以使用基类的方法(派生类继承了基类的接口)
		- 派生类需要自己的构造函数
		- 派生类可以根据需要添加额外的数据成员和成员函数
		- 派生类不能直接访问基类的私有成员，而必须通过基类方法进行访问
		- 派生类构造函数必须使用基类构造函数
		- 创建派生类对象时，程序首先创建基类对象，这意味着基类对象应当在程序进入派生类构造函数之前被创建
		- ```c++
		  RatedPlayer::RatedPlayer(unsigned int r, const string &fn,
		      const string &ln, bool ht) : TableTennisPlayer(fn, ln, ht)
		  {
		      rating = r;
		  }
		  ```
		- 派生类构造函数
			- 首先创建基类对象
			- 派生类构造函数应通过成员初始化列表将基类信息传递给基类构造函数
			- 派生类构造函数应初始化派生类新增的数据成员
		- 派生类和基类之间的关系
			- 派生类对象可以使用基类的方法，条件是方法不是私有的
			- 基类指针可以在不进行显式类型转换的情况下指向派生类对象
			- 基类引用可以在不进行显式类型转换的情况下引用派生类对象
			- 基类指针或引用只能用于调用基类方法,不可以将基类对象和地址赋给派生类引用和指针
		- 继承方式
			- 公有继承
				- is-a关系，即派生类对象也是一个基类对象，更准确关系描述应该是is-a-kind-of
					- Banana类派生自Fruit类
				- 公有继承^^不建立^^has-a关系
					- 午餐可能包含水果但通常午餐并不是水果
				- 公有继承^^不建立^^is-like-a关系
					- 律师就像鲨鱼，但律师并不是鲨鱼
				- 公有继承^^不建立^^is-implemented-as-a关系
					- 可以使用数组来实现栈，但从Array类派生出Stack类是不合适的
				- 公有继承^^不建立^^uses-a关系
					- 计算机可以使用激光打印机，但从Computer类派生出Printer类（或反过来）是没有意义的
			- 保护继承
			- 私有继承
- 运算符重载
	- 赋予运算符多重含义，多态的一种形式，与函数重载类似，扩展了重载的概念
	- ```c++
	  class Time
	  {
	  private:
	      int hours;
	      int minutes;
	  public:
	      ...
	      Time operator+(const Time &t) const;
	  }
	  
	  Time Time::operator+(const Time &t) const
	  {
	      Time sum;
	      sum.minutes = minutes + t.minutes;
	      sum.hours = hours + t.hours + sum.minutes / 60;
	      sum.minutes %= 60;
	      return sum;
	  }
	  
	  // 调用形式
	  total = coding.operator+(fixing);    // function notation 函数记法
	  total = coding + fixing;        // operator notation 运算符记法
	  
	  t4 = t1 + t2 + t3;
	  等价于
	  t4 = t1.operator+(t2.operator+(t3))
	  ```
	- 运算符重载的限制
		- 重载后的运算符必须至少有一个操作数是用户定义的类型，这将防止用户为标准类型重载运算符
		- 使用运算符时不能违反运算符原来的句法规则,例如不能将求模运算符（%）重载成使用一个操作数
		- 不能创建新运算符
		- 不能重载下面的运算符
		  collapsed:: true
			- ```c++
			  sizeof sizeof运算符
			  . 成员运算符
			  . * 成员指针运算符
			  :: 作用域解析运算符
			  ?: 条件运算符
			  typeid 一个RTTI运算符
			  const_cast 强制类型转换运算符
			  dynamic_cast 强制类型转换运算符
			  reinterpret_cast 强制类型转换运算符
			  static_cast 强制类型转换运算符
			  ```
		- 下面的运算符只能通过成员函数进行重载
		  collapsed:: true
			- ```c++
			  = 赋值运算符。
			  ( ) 函数调用运算符。
			  [ ] 下标运算符。
			  -> 通过指针访问类成员的运算符
			  ```
- 友元
	- 让函数成为类的友元，赋予该函数与类的成员函数相同的访问权限
	- 友元类型
		- 友元函数
			- 类的友元函数是非成员函数，其访问权限与成员函数相同,友元函数是类的扩展接口的组成部分
			- 友元函数解决的问题
				- ```c++
				  例如
				  对于重载的乘法运算符
				  A = B * 2.75;
				  相当于
				  A = B.operator*(2.75);
				  
				  但是 A = 2.75 * B; 则会出错，因为没有对应的成员函数与之对应
				  
				  解决方法：使用非成员函数，因为非成员函数不是由对象调用的，
				  它使用的所有值(包括对象)都是显示参数，因此编译器能够将
				  A = 2.75 * B;
				  与
				  A = operator*(2.75, B);
				  对应起来
				  
				  非成员函数存在的问题：非成员函数不能直接访问类的私有数据，至少常规非成员函数不能访问，
				  但是友元函数作为特殊的非成员函数，具有访问类的私有成员的能力
				  ```
			- 创建友元函数
				- 将友元函数原型放在类声明中
				- 实现友元函数
					- ```c++
					  友元函数原型放在类声明中
					  friend Time operator*(double m, const Time &t); // goes in class declaration
					  该原型意味着：
					  1.虽然operator *( )函数是在类声明中声明的，但它不是成员函数，因此不能使用成员运算符
					  来调用
					  2.虽然operator *( )函数不是成员函数，但它与成员函数的访问权限相同
					  
					  友元函数实现
					  Time operator*(double multi, const Time &t) // 不需要friend关键字
					  {
					      Time result;
					      long totalminutes = t.hours * multi * 60 + t.minutes * mult;
					      result.hours = totalminutes % 60;
					      return result;
					  }
					  或者
					  Time operator*(double multi, const Time &t)
					  {
					      return t * m;    // use t.operator*(m)
					  }
					  ```
				- 友元函数应用
					- 为类重载运算符，将非类的项作为第一个参数，此时可使用友元函数来反转操作数的顺序
					- 重载`<<`运算符
						- ```c++
						  draft 实现
						  void operator<<(ostream &os, const Time &t)
						  {
						      os << t.hours << " hours, " << t.minutes << " minutes";
						  }
						  存在的问题：
						  调用方式只能是 cout << trip;
						  cout << "Trip time: " << trip << " (Tuesday)\n"; 会出错
						  
						  改进后的实现
						  ostream & operator<<(ostream &os, const Time &t)
						  {
						      os << t.hours << " hours, " << t.minutes << " minutes";
						      return os;
						  }
						  
						  示例1：
						  // mytime3.h -- Time class with friends
						  #ifndef MYTIME3_H_
						  #define MYTIME3_H_
						  #include <iostream>
						  
						  class Time
						  {
						  private:
						      int hours;
						      int minutes;
						  public:
						      Time();
						      Time(int h, int m = 0);
						      void AddMin(int m);
						      void AddHr(int h);
						      void Reset(int h = 0, int m = 0);
						      Time operator+(const Time &t) const;
						      Time operator-(const Time &t) const;
						      Time operator*(double n) const;
						      friend Time operator*(double m, const Time &t)
						          { return t * m; }    // inline definition
						      friend std::ostream & operator<<(std::ostream &os, const Time &t);
						  }
						  #endif
						  
						  示例2:
						  // vect.h -- Vector class with <<, mode state
						  #ifndef VECT_H_
						  #define VECT_H_
						  #include <iostream>
						  namespace VECTOR
						  {
						      class Vector
						      {
						      public:
						          enum Mode {RECT, POL};
						          // RECT for rectangular, POL for Polar modes
						      private:
						          doulbe x;        // horizontal value
						          doulbe y;        // vertical value
						          double mag;        // length of vector
						          doulbe ang;        // direction of vector in degrees
						          Mode mode;        // RECT or POL
						      // private methods for setting values
						          void set_mag();
						          void set_ang();
						          void set_x();
						          void set_y();
						      public:
						          Vector();
						          Vector(double n1, double n2, Mode form = RECT);
						          void reset(double n1, double n2, Mode form = RECT);
						          ~Vector();
						          double xval() const {return x;}        // report x value
						          double yval() const {return y;}        // report y value
						          double magval() const {return mag;}    // report mag value
						          double angval() const {return ang;}    // report ang value
						          void polar_mode();            // set mode to POL
						          void rect_mode();            // set mode to RECT
						      // operator overloading
						          Vector operator+(const Vector &b) const;
						          Vector operator-(const Vector &b) const;
						          Vector operator-() const;        // 对已重载的运算符进行重载
						          Vector operator*(double n) const;
						      // friends
						          friend Vector operator*(double n, const Vector &a);
						          friend std::ostream & operator<<(std::ostream &os, const Vector &v);
						      }
						  }    // end namespace VECTOR
						  #endif
						  ```
				- 重载运算符
					- 成员函数 or 友元函数 ?
						- ```c++
						  // 成员函数版本
						  Time operator+(const Time &t) const;
						  
						  // 非成员函数(友元函数)实现
						  friend Time operator+(const Time &t1, const Time &t2);
						  ```
						- 在定义运算符时，只能选择其中的一种格式，而不能同时选择这两种格式。因为这两种格式都与同一个表达式匹配，同时定义这两种格式将被视为二义性错误，导致编译错误
		- 友元类
		- 友元成员函数
- 参考文档
	- [Beginning C++ Programming](https://notalentgeek.github.io/note/note/project/project-independent/pi-brp-beginning-c-programming/document/20170807-1504-cet-1-book-and-source-1.pdf) #pdf
	- [leveldb](https://github.com/google/leveldb/tree/main)
	- [LevelDB 源码分析](https://sf-zhou.github.io/leveldb/leveldb_01_data_structure.html)
	- [LevelDB 源码分析](https://github.com/balloonwj/CppGuide/tree/master/articles/leveldb%E6%BA%90%E7%A0%81%E5%88%86%E6%9E%90)
	- [LevelDB 源码分析](https://hardcore.feishu.cn/mindnotes/bmncnzpUmXNQruVGOwRwisHyxoh)
	- [The C++ Programming Language Fourth Edition](https://www.academia.edu/41763404/The_C_Programming_Language_Fourth_Edition_Bjarne_Stroustrup)
	- [Essential C++](https://www.programming-books.io/essential/cpp/index.html) #online
	- [C++ Templates](https://github.com/xiaoweiChen/Cpp-Templates-2nd) #pdf
	- [力扣](https://leetcode.cn/)
	- [C++学习](https://github.com/chengxumiaodaren/cpp-learning)
	- [设计模式和重构](https://refactoringguru.cn)
	- [A Tour of C++](http://www.staroceans.org/e-book/ATourofC%2B%2B.pdf) #pdf
	- [A Tour of C++ Second Edition](https://eldalin.com/private/library/A_Tour_of_C++_2nd_edition.pdf) #pdf
	- [C and C++ Projects](https://www.codewithc.com/c-projects-with-source-code/)
	- [Sams Teach Yourself C++](http://library.uc.edu.kh/userfiles/pdf/17.SAMS%20Teach%20Yourself%20C++%20in%2024%20Hours.pdf) #pdf
	- Learn C++ Quickly #pdf
	- [C++ Language](https://en.cppreference.com/w/cpp/language)
	- [C++ 参考手册](https://c-cpp.com/cpp)
	- [Standard C++ Library reference](https://cplusplus.com/reference/)
	- [The GNU C++ Library Manual](https://gcc.gnu.org/onlinedocs/libstdc++/manual/index.html)
	- [C++ Language Tutorial](https://cplusplus.com/files/tutorial.pdf)
	- [The GNU C++ Library](https://gcc.gnu.org/onlinedocs/libstdc++/index.html)
	- [C++ Standard Template Library](https://www.cppreference.com/Cpp_STL_ReferenceManual.pdf) #pdf
	- [C++ Standard Library reference (STL)](https://learn.microsoft.com/en-us/cpp/standard-library/cpp-standard-library-reference?view=msvc-170)
	- [Code Examples of C++ Templates - The Complete Guide](http://tmplbook.com/code/toc.html) #code
	- [C++ Templates - The Complete Guide](http://www.tmplbook.com/tmplbooktoc.pdf)
	- [**Large scale C++ Software development tutorials**](https://github.com/kks32/cpp-software-development/blob/master/README.md)
	- [The comprehensive catalog of C++ books](https://github.com/yuchdev/CppBooks?tab=readme-ov-file)
	- [Large Scale C++ Software Design](https://github.com/wonter/learning-distributed-storage/blob/master/sources/ebooks/Large%20Scale%20C%2B%2B%20Software%20Design.pdf) #github #pdf
	- [The Definitive C++ Book Guide and List.md](https://gist.github.com/tassoevan/0f7edd03e19f9674df07)
	- [Programming Principles and Practice Using C++(code)](https://github.com/Chrinkus/stroustrup-ppp/tree/master)
	- [Programming Principles and Practice Using C++(code)](https://github.com/LIParadise/DSnP_textbook/blob/master/Programming%20Principles%20and%20Practice%20Using%20C%2B%2B%202nd%20edition.pdf) #github #pdf
	- [API Design for Cpp.pdf](https://github.com/GeorgeQLe/Textbooks-and-Papers/blob/master/%5BC%2B%2B%5D%20API%20Design%20for%20Cpp.pdf)
	- [**C++ Books**](https://github.com/EbookFoundation/free-programming-books/blob/main/books/free-programming-books-langs.md#c-2)
	- [Open Data Structures (in C++)](http://opendatastructures.org/ods-cpp.pdf) #pdf
	- []()