[**C++ online compiler**](https://www.programiz.com/cpp-programming/online-compiler/)
[**Online Compiler**](https://www.mycompiler.io/new/asm-x86_64) #online

```cpp

/*
#include tells the compiler to copy the contents of the specified file into the source file at this point
<> 表示 编译器会从标准路径查找头文件
C++标准库通常使用不带后缀的头文件，用户的自定义头文件通常使用.h, .hpp, .hxx的后缀
*/
#include <iostream>

// 单行注释

/*
function 定义：
返回类型 函数名(参数列表(参数类型:参数名称))
{
    函数体；
}

{} 标识一个 code block，在该例中标识 main 函数的 code block

The convention with C++ is that a function called main is the entry point of the executable，即main函数是程序执行时第一个被call到的函数
*/
int main(int argc, char *argv[])
{
/*
std 表示 std namespace
:: is the scope resolution operator, and indicates that you want to access the cout object declared in the std namespace
std::cout 表示 cout stream object is part of the standard C++ library
<< means that a function called "operator <<" is called and is passed the string
C++支持operator overloading，因此对于不同类型的参数，operator 的行为也不同

The cout object is an instance of the ostream class and this has already been created before the main function is called
*/
    std::cout << "there are " << argc << " parameters" << std::endl;
    for(int i = 0; i < argc; i++)
    {
        std::cout << argv[i] << std::endl;
    }
/*
The rule in C++ is that, if the function declares that it returns a value, then it must return a value. However, there is a single exception to
this rule: if the main function does not return a value, then a value of 0 will be assumed. 
*/
}


Associated with symbols and conditional compilation is the compiler directive, #pragma once.
Pragmas are directives specific to the compiler, and different compilers will support different pragmas.

Windows uses the extension lib for static libraries and dll for dynamiclink libraries. 
GNU gcc uses the extension a for static libraries and so for shared libraries

Function prototype gives the compiler the information it needs to know about calling the function without providing the actual body of the function, the function definition

When you include a file into your source file, the preprocessor will include the contents of that file (after taking into account any conditional compilation directives) and, recursively, any files included by that file.

The compiler will not look forward in a source file, so if function A calls another function, B, in the same source file then function B must have already been defined before function A calls it, or there must be a prototype declaration
This leads to a typical convention of having a header file associated with each source file that contains the prototypes of the functions in the source file, and the source file includes this header. This convention becomes more important when you write classes.

The common terminology is that the output of a build step is called a target and the inputs of the build step (for example, source files) are the dependencies of that target.

A simple project structure:
// utils.h:
#include <iostream>
#include <string>
#include <ctime>

// utils.cpp:
#include ″utils.h″
...

// main.cpp:
#include "utils.h"
#include "name.h"
#include "time.h"
void main()
{
	print_name();
	print_time();
}

// name.h:
void print_name();

// time.h:
void print_time();

// name.c:
#include "utils.h"
#include "name.h"

void print_name()
{
	...
}

// time.c:
#include "utils.h"
#include "time.h"

void print_time()
{
	...
}


K&R style:
if (/* some test */) {
// the test is true
if (/* some other test */) {
// second test is true
} else {
// second test is false
}
} else {
// the test is false
}

An expression is a sequence of operators and operands (variables or literals) that results in some value.

A statement can be a declaration of a variable, an expression that evaluates to a value, or it can be a definition of a type. A statement may also be a control structure to affect the flow of the execution through your code.
A statement ends with a semicolon. A semicolon on its own is called a null statement. A null statement does nothing, so having too many semicolons is usually benign.

Broadly speaking, an expression becomes a statement by when you append a semicolon.

Every expression is either an lvalue or an rvalue. An lvalue is an expression that refers to some memory location. An lvalue can appear on the left-hand or right-hand side of an assignment. All variables are lvalues

An rvalue is a temporary item that does not exist longer than the expression that uses it; it will have a value, but cannot have a value assigned to it, so it can only exist on the right-hand side of an assignment. Literals are rvalues. 

Comma operator has the lowest precedence

In general, it is best to declare the variable as close as possible to where you will use it, and within the most restrictive scope. This prevents name clashes, where you will have to add additional information to disambiguate two or more variables

Three ways to initialize variables:
1. assign a value
2. call the type constructor
3. initialize a variable using function syntax

int i = 1;
int j = int(2);
int k(3);

Each type will have a literal representation.

constants:
const double pi = 3.1415;

constant expressions:
C++11 introduces a keyword called constexpr. This is applied to an expression, and indicates that the expression should be evaluated at compile type rather than at runtime

constexpr double pi = 3.1415;
constexpr double twopi = 2 * pi;

The constexpr keyword can also be applied to functions that return a value that can be evaluated at compile time, and so this allows the compiler to optimize the code
constexpr int triang(int i)
{
	return (i == 0) ? 0 : triang(i - 1) + i;
}

This function, when called with a literal in your code, can be evaluated at compile time. The constexpr is an indication to the compiler to check the usage of the function to see if it can determine the parameter at compile time
If the compiler cannot determine the parameter at compile-time, the function will be called as normal. A function marked with the constexpr keyword must only have one expression

An enum is a group of named constants, which means that you can use an enum as a parameter to a function
An enumeration is an integer type and by default the compiler will assume an int, but you can change this by specifying the integer type in the declaration

enum suits {clubs, diamonds, hearts, spades};
enum suits : char {clubs, diamonds, hearts, spades};

suits card1 = diamonds;
suits card2 = suits::diamonds;    // scope it with the name of the enumeration which is better

To force developers to specify the scope, you can apply the keyword class

enum class suits : char {clubs, diamonds, hearts, spades};

By default, the compiler will give the first enumerator a value of 0 and then increment the value for the subsequent enumerators.

enum ports {ftp=21, ssh, telnet, smtp=25, http=80};

In C++, you will access memory using a typed pointer, pointers are declared using the * symbol and you access a memory address with the & operator

Namespaces give you one mechanism to modularize code. A namespace allows you to label your types, functions, and variables with a unique name so that, using the scope resolution operator, you can give a fully qualified name. 

Defining a namespace is simple: you decorate the types, functions, and global variables with the namespace keyword and the name you give to it.

// 1
namespace utilities
{
	bool poll_data()
	{
		// code that returns a bool
	}
	int get_data()
	{
		// code that returns an integer
	}
}

// 2
namespace utilities
{
	// declare the functions
	bool poll_data();
	int get_data();
}

//define the functions
bool utilities::poll_data()
{
	// code that returns a bool
}

int utilities::get_data()
{
	// code that returns an integer
}

One use of namespaces is to version your code
 namespace utilities
 {
	bool poll_data();
	int get_data();

	namespace V2
	{
		bool poll_data();
		int get_data();
		int new_feature();
	}
 }

When an item in a specific namespace calls an item in the same namespace, it does not have to use a qualified name
It is important to note that, to declare a nested namespace, you have to do the nesting manually


C++11 provides a facility called an inline namespace that allows you to define a nested namespace, but allows the compiler to treat the items as being in the parent namespace when it performs an argument-dependent lookup
namespace utilities
{
	inline namespace V1
	{
		bool poll_data();
		int get_data();
	}

	namespace V2
	{
		bool poll_data();
		int get_data();
		int new_feature();
	}
}

Now to call the first version of get_data, you can use utilities::get_data or utilities::V1::get_data


Use using statement to indicate that symbols declared in the specified namespace can be used without a fully qualified name
using namespace utilities;
int i = get_data();
int j = V2::get_data();

using std::cout;
using std::endl;
cout << "Hello, World!" << endl;

The great advantage of a namespace is to be able to define your items with names that may be common, but are hidden from other code that does not know the namespace name of.

namespace alias:

namespace com_packtpub_richard_grimes
{
	int get_data();
}

int i = com_packtpub_richard_grimes::get_data();
可以被简化为
namespace packtRG = com_packtpub_richard_grimes;
int i = packtRG::get_data();

A namespace without a name has the special meaning that it has internal linkage, that is, the items can only be used in the current translation unit, the current file, and not in any other file.

Code that is not declared in a namespace will be a member of the global namespace.  You can call the code without a namespace name, but you may want to explicitly indicate that the item is in the global namespace using the scope resolution operator without a namespace name

int version = 42;
void print_version()
{
	std::cout << "Version = " << ::version << std::endl;
}

Operators are used to compute a value from one or more operands. T


The vector template is a class that contains items of the type specified in the angle brackets (<>); 
The vector can be initialized in a special way called "list initialization" which is new to C++11, 

using namespace std;
vector<string> beatles = { "John", "Paul", "George", "Ringo" };
for (int i = 0; i < beatles.size(); ++i)
{
	cout << beatles.at(i) << endl;
}

Range-based for:

vector<string> beatles = { "John", "Paul", "George", "Ringo" };
for (string musician : beatles)
{
	cout << musician << endl;
}

// 2
int birth_years[] = { 1940, 1942, 1943, 1940 };
for (int birth_year : birth_years)
{
	cout << birth_year << endl;
}


A variable is an instance of a type; it is the memory allocated to hold the data that the type can hold.

C++ provides integer types of various sizes, the actual sizes of these types can be determined by the sizeof operator

// #include <cstdint>
using namespace std; // Values for x86
cout << sizeof(int8_t) << endl; // 1
cout << sizeof(int16_t) << endl; // 2
cout << sizeof(int32_t) << endl; // 4
cout << sizeof(int64_t) << endl; // 8

// Using bitset to show bit patterns

#include <iostream>
#include <bitset>

int main()
{
    // The compiler ignores the quote; it is just used as a visual aid.
    unsigned long long every_other = 0xAAAA'AAAA'AAAA'AAAA;
    unsigned long long each_other = 0x5555'5555'5555'5555;
    std::cout << std::hex << std::showbase << std::uppercase;
    std::cout << every_other << std::endl;
    std::cout << each_other << std::endl;

    // Using bitset to show bit patterns
    std::bitset<64> bs_every(every_other);
    std::cout << bs_every << std::endl;
    
    bs_every.set(0);
    every_other = bs_every.to_ullong();
    std::cout << bs_every << std::endl;
    std::cout << every_other << std::endl;
}


reverse the byte order for big-endian and little-endian:
	unsigned short reverse(unsigned short us)
	{
		return ((us & 0xff) << 8) | ((us & 0xff00) >> 8);
	}

Using character macros
	Macro 		Tests if the character is:
	isalnum 	An alphanumeric character, A to Z, a to z, 0 to 9
	isalpha 	An alphabetic character, A to Z, a to z
	isascii 	An ASCII character, 0x00 to 0x7f
	isblank 	A space or horizontal tab
	iscntrl 	A control character, 0x00 to 0x1f or 0x7f
	isdigit 	A decimal digit 0 to 9
	isgraph 	A printable character other than space, 0x21 to 0x7e
	islower 	A lowercase character, a to z
	isprint 	A printable character, 0x20 to 0x7e
	ispunct 	A punctuation character, ! " # $ % & ' ( ) * + , - . / : ; < = > ? @ [ ] ^ _ ` { | } ~ \
	isspace 	A space
	isupper 	An uppercase character, A to Z
	isxdigit 	A hexadecimal digit, 0 to 9, a to f, A to F


	toupper 	The uppercase version of the character
	tolower 	The lowercase version of the character


Raw strings
	When you use a raw string literal you essentially switch off the meaning of escape characters.
	The raw string is delimited with R"( and )". That is, the string is between the inner parentheses (Note that, the () is part of the syntax and is not part of the string.)

	cout << R"(newline is \n in C++ and "quoted text" use quotes)";
	打印结果：
	newline is \n in C++ and "quoted text" use quotes


String byte order
	Extended character sets use more than one byte per character. If such characters are stored in a file, the order of the bytes becomes important. In this situation, the writer of the character must use the same order that will be used by potential readers.
	One way to do this is to use a Byte Order Mark (BOM).

The bool type holds a Boolean value, that is, just one of two values: true or false.

Note that void is not really a type because you cannot create a void variable; it is the absence of a type.

In C++11 another way to initialize variables was introduced: construction through a list initializer.

	int i = 1;			// initialized to a value
	int j = int(2);		// calling the type as if it is a function
	int k(3);			// calls the constructor of the int type
	int m{4};			// initializes the variable using an initialize list between curly braces ({})

C++11 introduces a mechanism for declaring that a variable's type should be determined from the data it is initialized with, that is, auto

	The auto keyword means that the compiler should create a variable with the type of the data that is assigned to it. The variable can only have a single type, the type the compiler decides is the type it needs for the data assigned to it, and you cannot use the variable elsewhere to hold data of a different type. Because the compiler needs to determine the type from an initializer, it means that all auto variables must be initialized:

	auto i = 42; // int
	auto l = 42l; // long
	auto ll = 42ll; // long long
	auto f = 1.0f; // float
	auto d = 1.0; // double
	auto c = 'q'; // char
	auto b = true; // bool

	The power of auto is when you use containers that can result in some fairly complicated looking types

Storage classes

	When declaring a variable, you can specify its storage class which indicates the lifetime, linkage (what other code can access it), and memory location of the variable.

	static, which when applied to a variable in a function means that the variable can only be accessed within that function, but its lifetime is the same as the program

	static can be used on variables declared at file scope, in which case it indicates that the variable can only be used in the current file, which is called internal linkage

	If you omit the static keyword on a variable, defined at file scope, then it has an external linkage, which means the name of the variable is visible to code in other files

	The static keyword says that the variable can only be used in the current file. The extern keyword indicates the opposite; the variable (or function) has external linkage and can be accessed in other files in the project.

	The final storage class specifier is thread_local

Using type aliases

	C++ provides the typedef statement to create an alias for a type

	typedef tuple<string, int> name_year_t;
	vector<name_year_t> beatles;


	The typedef keyword is a well-established way to create aliases in C++
	C++11 introduces another way to create a type alias, the using statement
		using name_year = tuple<string, int>;


Aggregating data in record types

	struct time_of_day
	{
		int sec;
		int min;
		int hour;
	};

There are several ways to initialize an instance of a structure.

	You can also initialize structures using the list initializer syntax using curly braces ({}). The items in the braces should match the members of the struct in the order of the members as declared. If you provide fewer values than there are members, the remaining members are initialized to zero. Indeed, if you provide no items between the curly braces then all members are set to zero. It is an error to provide more initializers than there are members.

	time_of_day lunch {0, 0, 13};
	time_of_day midnight {};
	time_of_day midnight_30 {0, 30};

	// You can have a member of a struct that is a struct itself
	struct working_hours
	{
		time_of_day start_work;
		time_of_day end_work;
	};

	working_hours weekday{ {0, 30, 8}, {0, 0, 17} };


Structure fields
	A structure can have members that are as small as a single bit, called a bit-field. In this case, you declare an integer member with the number of bits that the member will take up. You are able to declare unnamed members.

	struct item_length
	{
		unsigned short len : 10;
		unsigned short : 5;
		bool dirty : 1;
	};

Using structure names

	In some cases, you may need to use a type before you have actually defined it. As long as you do not use the members, you can declare a type before defining it:

	struct time_of_day;
	void print_day(time_of_day time);

	There is, however, an exception: a type can hold pointers to instances of the same type before the type is fully declared. This is because the compiler knows the size of a pointer, so it can allocate sufficient memory for the member. It is not until the entire type has been defined before you can create an instance of the type. The classic example of this is a linked list

Determining alignment
	One of the uses of structs is that if you know how data is held in memory you can deal with a struct as a block of memory.

	The compiler will place variables in memory in the way that is the most efficient, in terms of memory usage, or speed of access. The various types will be aligned to alignment boundaries.

	You can test the alignment of a specific type using the alignof operator passing the type name

	std::cout << "alignment boundary for int is " << alignof(int) << std::endl;

Storing data in the same memory with unions
A union is a struct where all the members occupy the same memory. The size of such a type is the size of the largest member. Since a union can only hold one item of data, it is a mechanism to interpret the data in more than one way.

// edited version
 struct VARIANT
 {
	unsigned short vt;
	union
	{
		unsigned char bVal;
		short iVal;
		long lVal;
		long long llVal;
		float fltVal;
		double dblVal;
	};
 }


Accessing runtime type information
C++ provides an operator called typeid that will return type information about a variable (or a type) at runtime. Runtime Type Information (RTTI) is significant when you use custom types that can be used in a polymorphic way

cout << "int type name: " << typeid(int).name() << endl;
int i = 42;
cout << "i type name: " << typeid(i).name() << endl;


auto a = i;
if (typeid(a) == typeid(int))
{
	cout << "we can treat a as an int" << endl;
}


Determining type limits
	cout << "The int type can have values between ";
	cout << numeric_limits<int>::min() << " and ";
	cout << numeric_limits<int>::max() << endl;


Type conversions
Built-in conversions can have one of two outcomes: promotion or narrowing. 
A promotion is when a smaller type is promoted to a larger type and you will not lose data. 
A narrowing conversion happens when a value from a larger type is converted to a smaller type with potential loss of data.


Casting
In some cases, you will have to convert between types

Various cast operations you can use in C++11:
	Name 												Syntax
	Construction										{}
	Remove const requirement 							const_cast
	Cast with no runtime checks 						static_cast
	Bitwise casting of types 							reinterpret_cast
	Cast between class pointers, with runtime checks 	dynamic_cast
	C style 											()
	Function style 										()


Casting without runtime checks
Most casts are performed using the static_cast operator, and this can be used to convert pointers to related pointer types as well as converting between numeric types.

double pi = 3.1415;
int pi_whole = static_cast<int>(pi);

void unsafe_d(void* pData)
{
	double* pd = static_cast<double*>(pData);
	cout << *pd << endl;
}


Casting pointers without runtime checks
The reinterpret_cast operator allows pointers to one type to be converted to pointers of another type, and it can convert from a pointer to an integer and an integer to a pointer

double pi = 3.1415;
int i = reinterpret_cast<int>(&pi);
cout << hex << i << endl;


Casting with runtime checks
The dynamic_cast operator is used to convert pointers between related classes


Casting with list initializer
The C++ compiler will allow some implicit conversions; in some cases, they may be intentional and in some cases, they may not be.


double pi = 3.1415;
// possibly loss of code
int i = pi;


int i = {pi};
In this case, if pi can be converted to an int without loss (for example, if pi is a short) then the code will compile without even a warning. However, if pi is an incompatible type (in this case, a double) the compiler will issue an error


char c = 35;
cout << c << endl;	// "#" printed out

To get the variable to be treated as a number you can use one of the following:

cout << static_cast<short>(c) << endl;
cout << short{ c } << endl;


Using C casts
	double pi = 3.1415;
	float f1 = (float)pi;
	float f2 = float(pi);


Using memory in C++

The & operator returns the address of an object. That object can be a variable, a built-in type or the instance of a custom type, or even a function
To access the data pointed to by a pointer, you must dereference it using the * operator

Using null pointers
The type of constant nullptr is not an integer, it is std::nullptr_t. All pointer types can be implicitly converted to this type, so nullptr can be used to initialize variables of all pointer types

Types of memory
	Static or global
		declare a variable at the global level, or if you have a variable declared in a function as static
	String pool
	Automatic or stack
	Free store

Pointer arithmetic
A pointer points to memory, and the type of the pointer determines the type of the data that can be accessed through the pointer.
The whole reason for the void* pointer type is that it can point to anything.

Passing multidimensional arrays to functions
When you pass an array, the first dimension will be treated as a pointer
	bool safe_torques(double nut_torques[][5], int num_wheels);
	bool safe_torques(double (*nut_torques)[5], int num_wheels);

Allocating individual objects
The new operator is used with the type to allocate memory, and it will return a typed pointer to that memory (Built-in types do not have constructors, so instead a type initialization will occur and this will usually initialize the object to zero)

int *p = new int; // allocate memory for one int
delete p;
p = nullptr;

When you delete a pointer, the destructor for the object is called. For built-in types, this does nothing. 
It is good practice to initialize a pointer to nullptr, after you have deleted it

int *p1 = new int (42);
int *p2 = new int {42};

Allocating arrays of objects
int *p = new int[2];
delete [] p;


Handling failed allocations
	// VERY_BIG_NUMER is a constant defined elsewhere
	int *pi;
	try
	{
	pi = new int[VERY_BIG_NUMBER];
	// other code
	}
	catch(const std::bad_alloc& e)
	{
	cout << "cannot allocate" << endl;
	return;
	}
	// use pointer
	delete [] pi;

// not throw an exception if the allocation fails
	int *pi = new (std::nothrow) int [VERY_BIG_NUMBER];
	if (nullptr == pi)
	{
	cout << "cannot allocate" << endl;
	}
	else
	{
	// use pointer
	delete [] pi;
	}


Resource Acquisition Is Initialization (RAII), which means using the features of C++ objects to manage resources. RAII in C++ needs classes and in particular, copy constructors and destructors.

Standard Template Library (STL), provide a standard way to insert items into collection objects and ways to access the items and iterate through entire collections (called iterators)

Standard Library arrays
	 array and vector
	
Using the stack-based array class
The array class allows you to create fixed sized arrays on the stack and, as with built-in arrays, they cannot shrink or expand at runtime.
	array<int, 4> arr { 1, 2, 3, 4 };
	for (int i : arr) cout << i << endl;
	The reason is that array implements the begin and end functions that are required for this syntax

	for (int i = 0; i < arr.size(); ++i) cout << arr[i] << endl;

	You can access memory outside of the bounds of the array, To guard against this, the class provides a function, at, which will perform a range check and if the index is out of range the class will throw the C++ exception out_of_range.

	array<int, 4> arr3;
	arr3.fill(42); // put 42 in each item
	arr2.swap(arr3); // swap items in arr2 with items in arr3

Using the dynamically allocated vector class
With vector class, the memory is dynamically allocated, which means that a vector can be expanded or shrunk at runtime
The vector class provides indexed random access with square bracket syntax and a range check with the at function


References
A reference is an alias to an object. That is, it is another name for the object, and so access to the object is the same through a reference as it is through the object's variable name

The pointer and reference have two different meanings. The reference is not initialized to the value of the variable, the variable's data; it is an alias for the variable name.

You can have several aliases for a variable, and each must be initialized to the variable at the declaration. Once declared, you cannot make a reference refer to a different object.
The following code will not compile:
 int& r1; // error, must refer to a variable
 int& r2 = nullptr; // error, must refer to a variable
(Since a reference is an alias for another variable, it cannot exist without being initialized to a variable. Likewise, you cannot initialize it to anything other than a variable name, so there is no concept of a null reference.)

int x = 1, y = 2;
int& rx = x; // declaration, means rx is an alias for x
rx = y; // assignment, changes value of x to the value of y

Constant references
This essentially makes the reference read-only: you can access the variable's data to read it, but not to change it
	int i = 42;
	const int& ri = i;
	ri = 99; // error!

Returning references
Returning a reference from a function is a common idiom, but whenever you consider doing this make sure that the lifetime of the aliased variable is not the scope of the function

Temporaries and references
The lvalue references must refer to a variable, but C++ has some odd rules when it comes to const references declared on the stack. If the reference is a const, the compiler will extend the lifetime of a temporary for the lifetime of the reference.
	const int& cri { 42 };
In this code, the compiler will create a temporary int and initialize it to a value and then alias it to the cri reference (it is important that this reference is const). The temporary is available through the reference while it is in scope.


The rvalue references
C++11 defines a new type of reference, rvalue references.
C++11 allows you to write code specifically for temporary objects, so in the case of the assignment, the operator for temporary objects can just move the data from the temporary into the object being assigned. In contrast, if the reference is not to a temporary object then the data will have to be copied. If the data is large, then this prevents a potentially expensive allocation and copy. This enables so-called move semantics.

 string global{ "global" };

 string& get_global()
 {
 	return global;
 }

 string& get_static()
 {
 	static string str { "static" };
 	return str;
 }

 string get_temp()
 {
 	return "temp";
 }

string get_temp()
{
	return "temp";	// returns a temporary object
}

cout << get_temp() << endl;

// use string
void use_string(string& rs)
{
	string s { rs };	// a copy overhead--creating the string, s, from the reference, rs;
	for (size_t i = 0; i < s.length(); ++i)
	{
		if ('a' == s[i] || 'b' == s[i] || 'o' == s[i])
			s[i] = '_';
	}
	cout << s << endl;
}

// use move semantics
void use_string(string&& s)		// the parameter is identified as an rvalue reference using the && suffix to the type
{
	for (size_t i = 0; i < s.length(); ++i)
	{
		if ('a' == s[i] || 'b' == s[i] || 'o' == s[i])
			s[i] = '_';
	}
	cout << s << endl;
}

When you call this function, the compiler will call the right one according to the parameter passed to it:
use_string(get_global()); // string& version
use_string(get_static()); // string& version
use_string(get_temp()); // string&& version
use_string("C string"); // string&& version
string str{"C++ string"};
use_string(str); // string& version

Ranged for and references
// read the value
for (int j : squares)
{
	cout << J << endl;
}

// change the value
for (int& k : squares)
{
	k *= 2;
}


Ranged "for" for multidimensional arrays
int arr[2][3] { { 2, 3, 4 }, { 5, 6, 7} };
for (auto row : arr)
{
	for (auto col : row) // will not compile
	{
		cout << col << " " << endl;
	}
}

Ranged "for" uses iterator objects and for arrays it uses the C++ Standard Library functions, begin and end, to create these objects. The compiler will see from the arr array in the outer ranged for that each item is an int[3] array, and so in the outer for loop the loop variable will be a copy of each element, in this case an int[3] array. You cannot copy arrays like this, so the compiler will provide a pointer to the first element, an int*, and this is used in the inner for loop.
The compiler will attempt to obtain iterators for int*, but this is not possible because an int* contains no information about how many items it points to. There is a version of begin and end defined for int[3] (and all sizes of arrays) but not for int*.


// The outer loop variable is in fact int (&)[3], That is, it is a reference to an int[3] (the parentheses used to indicate that it references an int[3] and is not an array of int&)
for (auto& row : arr)	
{
	for (auto col : row)
	{
		cout << col << " " << endl;
	}
}



Declaring and defining functions
Code that uses a function has to have access to the name of the function, and so it needs to have access to either the function definition or the declaration of the function (also called the function prototype)

The compiler uses the prototype to type-check that the calling code is calling the function, using the right types


C Runtime Library is implemented:
function is compiled in a static library or a dynamic link library
function prototypes are provided in a header file
include the header file for the library so that the function prototypes are available to the compiler
type the prototype in your code
library provided in the linker command line

However, much of the C++ Standard Library is implemented in header files, which means that these files can be quite large

forward declaration: You do not have to define the function before it is used as long as the function prototype is defined before the function is called

internal linkage and external linkage
	static int mult(int, int); // defined in this file
	extern int mult(int, int); // defined in another file

the compiler is free to ignore constexpr and inline specifier, these specifiers are just a suggestion to the compiler.


trailing return type

inline auto mult(int lhs, int rhs) -> int
{
	return lhs * rhs;
}

// the compiler will deduce the return type from the actual value returned.
// compiler will only know what the return type is from the function body, so you cannot provide a prototype for such functions
inline auto mult(int lhs, int rhs)
{
	return lhs * rhs;
}

The return type on the left is given as auto, meaning that the actual return type is specified after the parameter list
The -> int means that the return type is int


Specifying exceptions

Earlier versions of C++:
provide a comma separated list of the types of the exceptions that may be thrown by code in the function
provide an ellipsis (...) which means that the function may throw any exception
provide an empty pair of parentheses, which means the function will not throw exceptions
int calculate(int param) throw(overflow_error)
{
	// do something which potentially may overflow
}

C++11
// C++11 style: 
// no exception will be thrown was found
int increment(int param) noexcept
{
	// check the parameter and handle overflow appropriately
}

Function body
if the function is declared as returning auto, then the compiler will deduce the return type


Using function parameters
Passby-reference means that the variable in the calling code can be altered by the function, but this can be controlled by making the parameters const, in which case the reason for passby-reference is to prevent a (potentially costly) copy being made.

Passing Initializer lists
	point p;
	p.x = 1; p.y = 1;
	set_point(p);
	set_point({ 1, 1 });


Using default parameters
The default values occur in the function definition, not in a function prototype, the parameters that can have default values are the right-most parameters

void log_message(const string& msg, bool clear_screen = false)
{
	if (clear_screen) clear_the_screen();
	cout << msg << endl;
}

log_message("first message", true);
log_message("second message");
bool user_decision = ask_user();
log_message("third message", user_decision);


Variable number of parameters
A function with default parameter values can be regarded as having a variable number of user-provided parameters, where you know at compile time the maximum number of parameters and their values if the caller chooses not to provide values.

Three ways to have a variable number of parameters: initializer lists, C-style variable argument lists, and variadic templated functions

Initializer lists:
	#include <initializer_list>
	int sum(initializer_list<int> values)
	{
		int sum = 0;
		for (int i : values) sum += i;
		return sum;
	}

	cout << sum({}) << endl; // 0
	cout << sum({-6, -5, -4, -3, -2, -1}) << endl; // -21
	cout << sum({10, 20, 30}) << endl; // 60

Argument lists:
// 1
int sum(int first, ...)
{
	int sum = 0;
	va_list args;
	va_start(args, first);
	int i = first;
	while (i != -1)
	{
		sum += i;
		i = va_arg(args, int);
	}
	va_end(args);
	return sum;
}

cout << sum(-1) << endl; // 0
cout << sum(-6, -5, -4, -3, -2, -1) << endl; // -20 !!!
cout << sum(10, 20, 30, -1) << endl; // 60

// 2
int sum(int count, ...)
{
	int sum = 0;
	va_list args;
	va_start(args, count);
	while(count--)
	{
		int i = va_arg(args, int);
		sum += i;
	}
	va_end(args);
	return sum;
}

cout << sum(0) << endl; // 0
cout << sum(6, -6, -5, -4, -3, -2, -1) << endl; // -21
cout << sum(3, 10, 20, 30) << endl; // 60


name mangling
C++ compiler will decorate the name with extra symbols for the return type and parameters so that overloaded functions all have different names

extern "C"
the function has C linkage and the compiler will not use C++ name mangling

Overloading functions
several functions with the same name, but where the parameter list is different (the number of parameters and/or the type of the parameters)

compiler will attempt to find the function that best fits the parameters provided:
If there is not a suitable function, the compiler will attempt to convert the parameters to see if a function with those types exists. The compiler will start with trivial conversions (for example, an array name to a pointer, a type to a const type), and if this fails the compiler will try to promote the type (for example, bool to int). If that fails, the compiler will try standard conversions (for example, a reference to a type). If such conversions results in more than one possible candidate, then the compiler will issue an error that the function call is ambiguous.

Functions and scope
void f(int i) { /*does something*/ }
void f(double d) { /*does something*/ }

int main()
{
	void f(double d);	// The prototype is in the same scope as the function call, it hides the version with an int parameter
	f(1);
	return 0;
}

Deleted functions
prevent the implicit conversion for built-in types you can delete the functions that you do not want callers to use

void f(double) = delete;
void g()
{
	f(1); // compiles
	f(1.0); // C2280: attempting to reference a deleted function
}


Passing by value and passing by reference
// don't allow any more than 100 items
bool get_items(int count, vector<int>& values)
{
	if (count > 100) return false;
	for (int i = 0; i < count; ++i)
	{
		values.push_back(i);
	}
	return true;
}

vector<int> items {};
get_items(10, items);

Asserts are defined using conditional compilation and so will only appear in debug builds (that is, C++ code compiled with debugging information).

Using invariants
If you don't explicitly document what the function does to external data, then you must ensure that when the function finishes such data is left untouched.
For example, the cout object is global to your application, and it can be changed through manipulators to make it interpret numeric values in certain ways. If you change it in a function (say, by inserting the hex manipulator), then this change will remain when the cout object is used outside the function

Declaring function pointers
int (*fn)() = get_status;
int error_value = fn();

declare aliases for the type of the function pointer
	typedef bool(*MyPtr)(MyType*, MyType*);
	using MyPtr = bool(*)(MyType*, MyType*);	// more clear

using two_ints = void (*)(int, int);

void do_something(int l, int r){/* some code */}

void caller()
{
	two_ints fn = do_something;	// Notice: not use a * for fn
	fn(42, 99);
}

Using function pointers
A function pointer is merely a pointer. This means that you can use it as a variable; you can return it from a function, or pass it as a parameter.

using callback = void(*)(const string&);
void big_routine(int loop_count, const callback progress)
{
	for (int i = 0; i < loop_count; ++i)
	{
		if (i % 100 == 0)
		{
			string msg("loop ");
			msg += to_string(i);
			progress(msg);
		}
	// routine
	}
}

Templated functions
C++ provides templates to allow you to write more generic code; you write the routine using a generic type and at compile time the compiler will generate a function with the appropriate types. 


Defining templates
Compiler deduces the template parameters from how you call the function
// 1
	template<typename T>
	T maximum(T lhs, T rhs)
	{
		return (lhs > rhs) ? lhs : rhs;
	}

// 2
	template<typename T, typename U>
	T maximum(T lhs, U rhs)
	{
		return (lhs > rhs) ? lhs : rhs;
	}

// explicitly provide the types in the called function to call a specific version of the function and (if necessary) get the compiler to perform implicit conversions
	// call template<typename T> maximum(T,T);
	int i = maximum<int>(false, 100.99);

Using template parameter values
	template<int size, typename T>		// parameter size can be used in the function as a local (read-only) variable
	T* init(T t)
	{
		T* arr = new T[size];
		for (int i = 0; i < size; ++i) arr[i] = t;
		return arr;
	}

	int *i10 = init<10>(42);	// init<10,int>(42) can be used to explicitly indicate that you require an int array
	for (int i = 0; i < 10; ++i) cout << i10[i] << ' ';
	cout << endl;
	delete [] i10;

// the compiler will instantiate this function for every combination of xxx and yyy that your code calls. 
// If the template function has a large amount of code, then this may be an issue. One way around this is to use a helper function 
	template<typename T> void print_array(T* arr, int size)
	{
		for (int i = 0; i < size; ++i)
		{
			cout << arr[i] << endl;
		}
	}

	template<typename T, int N> inline void print_array(T (&arr)[N])
	{
		print_array(arr, N);
	}

	int squares[] = { 1, 4, 9, 16, 25 };
	print_array(squares);


Specialized templates
	In some cases, you may have a routine that works for most types (and a candidate for a templated function), but you may identify that some types need a different routine.

	template <typename T> int number_of_bytes(T t)
	{
		return sizeof(T);
	}

	template<> int number_of_bytes<const char *>(const char *str)	// add the specialized type to the function name
	{
		return strlen(str) + 1;
	}

	// 2
	define a templated function to return a maximum of two parameters of the same type
	template<typename T>
	T maximum(T lhs, T rhs)
	{
		return (lhs > rhs) ? lhs : rhs;
	}

	// delete the specialization for bool
	template<> bool maximum<bool>(bool lhs, bool rhs) = delete;


Variadic templates
A variadic template is when there is a variable number of template parameters

template<typename T, typename... Arguments>
void func(T t, Arguments... args);

You need to unpack the parameter pack to get access to the parameters passed by the caller
You can determine how many items there are in the parameter pack using the special operator, sizeof... (note the ellipses are part of the name);

To unpack the parameter pack
1.uses recursion

template<typename T> void print(T t)
{
	cout << t << endl;
}

template<typename T, typename... Arguments>
void print(T first, Arguments ... next)
{
	print(first);
	print(next...);
}

print(1, 2.0, "hello", bool);

2.use an initializer list

template<typename... Arguments>
void print(Arguments ... args)
{
	int arr [sizeof...(args)] = { args... };	// all the parameters have to be the same type of the array
	for (auto i : arr) cout << i << endl;
}

3.use the comma operator

template<typename... Arguments>
void print(Arguments ... args)
{
	int dummy[sizeof...(args)] = { (print(args), 0)... };
}


Overloaded operators
C++ provides the keyword operator to indicate that the function is not used with the function call syntax, but instead is called using the syntax associated with the operator

You can provide your own versions of the following unary operators:
	! & + - * ++ -- ~
You can also provide your own versions of the following binary operators:
	!= == < <= > >= && ||
	% %= + += - -= * *= / /= & &= | |= ^ ^= << <<= = >> =>>
	-> ->* ,
You can also write versions of the function call operator (), array subscript [], conversion operators, the cast operator (), and new and delete. 
You cannot redefine the ., .*, ::, ?:, # or ## operators, nor the "named" operators, sizeof, alignof or typeid.

struct point
{
	int x;
	int y;
};

bool operator==(const point& lhs, const point& rhs)
{
	return (lhs.x == rhs.x) && (lhs.y == rhs.y);
}

bool operator!=(const point& lhs, const point& rhs)
{
	return !(lhs == rhs);
}

ostream& operator<<(ostream& os, const point& pt)
{
	os << "(" << pt.x << "," << pt.y << ")";
	return os;
}

point p1{ 1,1 };
point p2{ 1,1 };
cout << boolalpha;
cout << (p1 == p2) << endl; // true
cout << (p1 != p2) << endl; // false

Operator overloading is often referred to as syntactic sugar, syntax that makes the code easier to read--but this trivializes an important technique

One example is functors, or function objects, where the class implements the () operator so that objects can be accessed as if they are functions


Function objects
A function object, or functor, is a custom type that implements the function call operator: (operator()). This means that a function operator can be called in a way that looks like it is a function

The <functional> header file contains various types that can be used as function objects.

The <algorithm> header contains functions that work on function objects


lambda expressions
C++11 provides a mechanism to get the compiler to determine the function objects that are required and bind parameters to them. These are called lambda expressions.

A lambda expression is used to create an anonymous function object at the location where the function object will be used.

auto less_than_10 = [](int a) {return a < 10; };
bool b = less_than_10(4);

The square brackets at the beginning of the lambda expression are called the capture list. This expression does not capture variables, so the brackets are empty.

You can use variables declared outside of the lambda expression and these have to be captured. The capture list indicates whether all such variables will be captured by a reference (use [&]) or by a value (use [=]). You can also name the variables that will be captured (if there are more than one, use a comma-separated list) and if they are captured by a value, you use just their names. If they are captured by a reference, use a & on their names.

// 1
int limit = 99;
auto less_than = [limit](int a) {return a < limit; };

// 2
auto incr = [] { static int i; return ++i; };
incr();
incr();
cout << incr() << endl; // 3

// 3
auto swap = [](int& a, int& b) { int x = a; a = b; b = x; };
int i = 10, j = 20;
cout << i << " " << j << endl;
swap(i, j);
cout << i << " " << j << endl;

// 4
vector<int> v { 1, 2, 3, 4, 5 };
int less_than_3 = count_if(
	v.begin(), v.end(),
	[](int a) { return a < 3; });
cout << "There are " << less_than_3 << " items less than 3" << endl;


Classes
A class allows you to encapsulate data in a custom type, and you can define functions on that type so that only these functions will be able to access the data.

The basic idea behind classes:
A mechanism to encapsulate the data into a type that knows what bytes to change, and only allow that type to access the data.

A struct is one of the class types that you can use in C++; the other two are union and class.


Defining class behavior
A class can define functions that can only be called through an instance of the class; such a function is often called a method. 
An object will have state; this is provided by the data members defined by the class and initialized when the object is created. 
The methods on an object define the behavior of the object, usually acting upon the state of the object. 
When you design a class, you should think of the methods in this way: they describe the object doing something.

class cartesian_vector
{
	// The public keyword means that any members defined after this specifier are accessible by code defined outside of the class.
	// By default, all the members of a class are private unless you indicate otherwise. 
	// private means that the member can only be accessed by other members of the class.
	public:
		double x;
		double y;
		// other methods
		double get_magnitude() { return std::sqrt((x * x) + (y * y)); }
};

the difference between a struct and a class: 
by default, members of a struct are public and by default, members of a class are private


Using the this pointer
class cartesian_vector
{
public:
	double x;
	double y;
	// other methods
	double get_magnitude()
	{
		// this makes it explicit that the items are members of the class
		return std::sqrt((this->x * this->x) + (this->y * this->y));
	}
	reset(double x, double y) { this->x = x; this->y = y; }
};

You can dereference the this pointer with the * operator to get access to the object. This is useful when a member function must return a reference to the current object
A method in a class can also pass the this pointer to an external function, which means that it is passing the current object by reference through a typed pointer


Using the scope resolution operator

	class cartesian_vector
	{
		public:
			double x;
			double y;
			// other methods
			double magnitude();
	};

	double cartesian_vector::magnitude()
	{
		return sqrt((this->x * this->x) + (this->y * this->y));
	}

Defining class state
Bear in mind that if you have a pointer to an item created in the free store, you need to know whose responsibility it is to deallocate the memory that the pointer points to. If you have a reference (or pointer) to an object created on a stack frame somewhere, you need to make sure that the objects of your class do not live longer than that stack frame

When you declare data members as public it means that external code can read and write to the data members
You can decide that you would prefer to only give read-only access, in which case you can make the members private and provide read access through accessors
When you make the data members private it means that you cannot use the initializer list syntax to initialize an object, but we will address this later

class cartesian_vector
{
	double x;
	double y;
	public:
		double get_x() { return this->x; }
		double get_y() { return this->y; }
		// other methods
	};

Creating objects

// This is direct initialization of the object and assumes that the data members of cartesian_vector are public.
cartesian_vector vec { 10, 10 };
cartesian_vector *pvec = new cartesian_vector { 5, 5 };
// use pvec
delete pvec

C++11 allows direct initialization to provide default values in the class:
class point
{
	public:
		int x = 0;
		int y = 0;
};

 class car
{
	public:
		double tire_pressures[4] { 25.0, 25.0, 25.0, 25.0 };
};

Construction of objects
constructors allows you to define special methods to perform the initialization of the object

constructors:
Default constructor: This is called to create an object with the default value.
Copy constructor: This is used to create a new object based on the value of an existing object.
Move constructor: This is used to create a new object using the data moved from an existing object.
Destructor: This is called to clean up the resources used by an object.
Copy assignment: This copies the data from one existing object into another existing object.
Move assignment: This moves the data from one existing object into another existing object.

The compiler-created versions of these functions will be implicitly public;

A constructor is a member function that has the same name as the type, but does not return a value, so you cannot return a value if the construction fails, which potentially means that the caller will receive a partially constructed object

Defining constructors
The default constructor is used when an object is created without a value and hence the object will have to be initialized with a default value.

// 1
class point
{
	double x; double y;
public:
	point() { x = 0; y = 0; }
};

point p; // default constructor called
point p {}; // calls default constructor


// A more efficient way is to use direct initialization with a member list.
point(double x, double y) : x(x), y(y) {}

The identifiers outside the parentheses are the names of class members, and the items inside the parentheses are expressions used to initialize that member (in this case, a constructor parameter). 


point(double x, double y) : x{x}, y{y} {}
point p(10.0, 10.0);

point arr[4];	// default constructor is called on the items; there is no way to call any other constructor, and so you have to initialize each one separately


// default values for constructor parameters
class car
{
	array<double, 4> tire_pressures;;
	double spare;
public:
	car(double front, double back, double s = 25.0)
	: tire_pressures{front, front, back, back}, spare{s} {}
};

car commuter_car(25, 27);
car sports_car(26, 28, 28);

Delegating constructors
A constructor may call another constructor using the same member list syntax:
class car
{
// data members
public:
	car(double front, double back, double s = 25.0)
		: tire_pressures{front, front, back, back}, spare{s} {}
	car(double all) : car(all, all) {}
};


Copy constructor
A copy constructor is used when you pass an object by value (or return by value) or if you explicitly construct an object based on another object

class point
{
	int x = 0;int y = 0;
public:
	point(const point& rhs) : x(rhs.x), y(rhs.y) {}
};

point p1(10, 10);

point p2(p1);	// copy constructor is called
point p3 = p1;	// copy constructor is called

Converting between types

	class cartesian_vector
	{
		double x; double y;
	public:
		cartesian_vector(const point& p) : x(p.x), y(p.y) {}
	};

	point p(10, 10);
	cartesian_vector v1(p);
	cartesian_vector v2 { p };
	cartesian_vector v3 = p;
	(The problem with the code above is that the cartesian_vector class accesses private members of the point class)

Making friends

class cartesian_vector; // forward decalartion

class point
{
	double x; double y;
public:
	point(double x, double y) : x(x), y(y){}
	friend class cartesian_point;
	// This indicates that the code for the entire class, cartesian_vector, can have access to the private members (data and methods) of the point class
};

// declare friend functions
ostream& operator<<(ostream& stm, const point& pt)
 {
 stm << "(" << pt.x << "," << pt.y << ")";
 return stm;
 }

friend ostream& operator<<(ostream&, const point&);

Such friend declarations have to be declared in the point class, but it is irrelevant whether it is put in the public or private section.

Marking constructors as explicit
In some cases, you do not want to allow the implicit conversion between one type that is passed as a parameter of the constructor of another type.
This now means that the only way to call the constructor is using the parentheses syntax: explicitly calling the constructor. 

class mytype
{
public:
	explicit mytype(double x);
};

mytype t1 = 10.0; // will not compile, cannot convert
mytype t2(10.0); // OK


Destructing objects
When an object is destroyed, a special method called the destructor is called. This method has the name of the class prefixed with a ~ symbol and it does not return a value.

// 1
void f(mytype t) // copy created
{
	// use t
} 	// t destroyed

void g()
{
	mytype t1;
	f(t1);
	if (true)
	{
		mytype t2;
	} 	// t2 destroyed

	mytype arr[4];
} 	// 4 objects in arr destroyed in reverse order to creation
	// t1 destroyed


// when you return an object
mytype get_object()
{
	mytype t; // default constructor creates t
	return t; // copy constructor creates a temporary
} 			  // t destroyed
void h()
{
	test tt = get_object(); // copy constructor creates tt
							// temporary destroyed, tt destroyed
}

// An object will be destroyed when you explicitly delete a pointer to an object allocated on the free store.
// In this case, the call to the destructor is deterministic: it is called when your code calls delete

mytype *get_object()
{
	return new mytype; // default constructor called
}

void f()
{
	mytype *p = get_object();
	// use p
	delete p; // object destroyed
}

If a data member in a class is a custom type with a destructor, then when the containing object is destroyed the destructors on the contained objects are called too. Nonetheless, note that this is only if the object is a class member. 

If a class member is a pointer to an object in the free store, then you have to explicitly delete the pointer in the containing object's destructor. However, you need to know where the object the pointer points to is because if it is not in the free store, or if the object is used by other objects, calling delete will cause problems.


Assigning objects
The assignment operator is called when an already created object is assigned to the value of another one. 

The copy assignment operator is typically a public member of the class and it takes a const reference to the object that will be used to provide the values for the assignment.

class buffer
{
	// data members
public:
	buffer(const buffer&); // copy constructor
	buffer& operator=(const buffer&); // copy assignment
};

buffer a, b, c; // default constructors called
// do something with them
a = b = c; // make them all the same value (more clear)
a.operator=(b.operator=(c)); // make them all the same value


key difference between the copy constructor and copy assignment methods:
A copy constructor creates a new object that did not exist before the call. The calling code is aware that if the construction fails, then an exception will be raised.

With assignment, both objects already exist, so you are copying the value from one object to another. This should be treated as an atomic action and all the copy should be performed; it is not acceptable for the assignment to fail halfway through, resulting in an object that is a bit of both objects

Furthermore, in construction, an object only exists after the construction is successful, so a copy construction cannot happen on an object itself, but it is perfectly legal (if pointless) for code to assign an object to itself. The copy assignment needs to check for this situation and take appropriate action.


Move semantics
C++11 provides move semantics through a move constructor and a move assignment operator, which are called when a temporary object is used either to create another object or to be assigned to an existing object. In both cases, because the temporary object will not live beyond the statement, the contents of the temporary can be moved to the other object, leaving the temporary object in an invalid state. The compiler will create these functions for you through the default action of moving the data from the temporary to the newly created (or the assigned to) object.


//  use only move and never to use copy
class mytype
{
	int *p;
public:
	mytype(const mytype&) = delete; 			// copy constructor
	mytype& operator= (const mytype&) = delete; // copy assignment
	mytype&(mytype&&); 							// move constructor
	mytype& operator=(mytype&&); 				// move assignment
};

mytype::mytype(mytype&& tmp)
{
	this->p = tmp.p;
	tmp.p = nullptr;
}

Declaring static members
You can declare a member of a class--a data member or a method--static.

Defining static members
When you use static on a class member it means that the item is associated with the class and not with a specific instance. 
In the case, of data members, this means that there is one data item shared by all instances of the class. Likewise, a static method is not attached to an object, it is not __thiscall and has no this pointer.

A static method is part of the namespace of a class, so it can create objects for the class and have access to their private members
Note that the static function cannot call nonstatic methods on the class because a nonstatic method will need a this pointer, but a nonstatic method can call a static method.

Two ways to call a static method, through an object or through the class name

class mytype
{
public:
	static void f(){}
	void g(){ f(); }
};

mytype c;
c.g(); // call the nonstatic method
c.f(); // can also call the static method thru an object
mytype::f(); // call static method without an object

For static data members, need to define static data members outside of the class

class mytype
{
public:
	static int i;
	static void incr() { i++; }
};

// in a source file
int mytype::i = 42;
// The data member is defined outside of the class at file scope. It is named using the class name, but note that it also has to be defined using the type.

You can also declare a variable in a method that is static. In this case, the value is maintained across method calls, in all objects, so it has the same effect as a static class member, but you do not have the issue of defining the variable outside of the class.



Using static and global objects
A static variable in a global function will be created at some point before the function is first called. Similarly, a static object that is a member of a class will be initialized at some point before it is first accessed.

Static and global objects are constructed before the main function is called, and destroyed after the main function finishes.
The issue is if you have several source files with static objects in each. There is no guarantee on the order in which these objects will be initialized.

Named constructors
This is one application for public static methods. The idea is that since the static method is a member of the class it means that it has access to the private members of an instance of the class, so such a method can create an object, perform some additional initialization, and then return the object to the caller. This is a factory method

class point
{
	double x; double y;
public:
	point(double x, double y) : x(x), y(y){}
	static point polar(double r, double th)		//  use a static method as a named constructor
	{
		return point(r * cos(th), r * sin(th));
	}
}

const double pi = 3.141529;
const double root2 = sqrt(2);
point p11 = point::polar(root2, pi/4);

// polar method could be written (less efficiently) as
point point::polar(double r, double th)
{
	point pt;
	pt.x = r * cos(th);
	pt.y = r * sin(th);
	return pt;
}


Nested classes
You can define a class within a class. If the nested class is declared as public, then you can create objects in the container class and return them to external code. Typically, however, you will want to declare a class that is used by the class and should be private. 

// declares a public nested class
class outer
{
	public:
		class inner
		{
			public:
			void f();
		};
		inner g() { return inner(); }
};

void outer::inner::f()	// Notice how the name of the nested class is prefixed with the name of the containing class.
{
	// do something
}

Accessing const objects

class point
{
	double x; double y;
public:
	double get_x() { return x; }
	double get_y() { return y: }
};

void print_point(const point& p)
{
	cout << "(" << p.get_x() << "," << p.get_y() << ")" << endl;
}

ERROR: cannot convert 'this' pointer from 'const point' to 'point &'

// solution
double get_x() const { return x; }
double get_y() const { return y: }

This effectively means that the this pointer is const. The const keyword is part of the function prototype, so the method can be overloaded on this.

A method marked with const must not alter the data members, not even temporarily, such a method can only call const methods. 
There may be rare cases when a data member is designed to be changed through a const object; in this case the declaration of the member is marked with the mutable keyword.


Using objects with pointers
Objects can be created on the free store and accessed through a typed pointer


Getting pointers to object members

class cartesian_vector
{
public:
	// other items
	double get_magnitude() const
	{
		return std::sqrt((this->x * this->x) + (this->y * this->y));
	}
};

double (cartesian_vector::*fn)() const = nullptr;
fn = &cartesian_vector::get_magnitude;	// getting a pointer to a method on a class that must be called through an object

cartesian_vector vec(1.0, 1.0);
double mag = (vec.*fn)();		// use the pointer to the member operator .* on an object
								// The pointer to the member operator says that the function pointer on the right is called with the object on the left

// syntax for an object pointer
cartesian_vector *pvec = new cartesian_vector(1.0, 1.0);
double mag = (pvec->*fn)();
delete pvec;


Operator overloading
One of behaviors of a type is the operations you can apply to it. C++ allows you to overload the C++ operators as part of a class so that it's clear that the operator is acting upon the type

// inline in point
point operator-() const
{
	return point(-this->x, -this->y);
}

point p1(-1,1);
point p2 = -p1; // p2 is (1,-1)


cartesian_vector point::operator-(point& rhs) const
{
	return cartesian_vector(this->x - rhs.x, this->y - rhs.y);
}

class mytype
{
public:
	mytype& operator++()
	{
		// do actual increment
		return *this;
	}
	mytype operator++(int)
	{
		mytype tmp(*this);
		operator++(); // call the prefix code
		return tmp;
	}
};


Defining function classes
A functor is a class that implements the () operator. This means that you can call an object using the same syntax as a function.


class factor
{
	double f = 1.0;
public:
	factor(double d) : f(d) {}
	double operator()(double x) const { return f * x; }
};

factor threeTimes(3); // create the functor object
double ten = 10.0;
double d1 = threeTimes(ten); // calls operator(double)
double d2 = threeTimes(d1); // calls operator(double)

double d2 = threeTimes.operator()(d1);

This code shows that the functor object not only provides some behavior but it also can have a state.


template<typename Fn>
void print_value(double d, Fn& fn)
{
	double ret = fn(d);
	cout << ret << endl;
}

The C++ Standard Library uses this magic, which means that the algorithms it provides can be called either with a global function or a functor, or a lambda expression.

The Standard Library algorithms use three type of functional classes, generators, and unary and binary functions; that is, functions with zero, one or two parameters.


Defining conversion operators
converting the object into another type

class mytype
{
	int i;
public:
	mytype(int i) : i(i) {}
	explicit mytype(string s) : i(s.size()) {}
	operator int () const { return i; }			//  convert an object back to an int
};

string s = "hello";
mytype t = mytype(s); // explicit conversion
int i = t; // implicit conversion


// using a conversion operator: returning values from a stateful functor

class averager
{
	double total;
	int count;
public:
	averager() : total(0), count(0) {}
	void operator()(double d) { total += d; count += 1; }
	operator double() const
	{
		return (count != 0) ? (total / count) :
			numeric_limits<double>::signaling_NaN();
	}
};

vector<double> vals { 100.0, 20.0, 30.0 };
double avg = for_each(vals.begin(), vals.end(), averager());


Managing resources
Resource Acquisition Is Initialization (RAII):
Put simply, the resource is allocated in the constructor of an object and freed in the destructor, so it means that the lifetime of the resource is the lifetime of the object. Typically, such wrapper objects are allocated on the stack, and this means that you are guaranteed that the resource will be freed when the object goes out of scope regardless of how this happens.


Writing wrapper classes
There are several issues that you must address when writing a class to wrap a resource ...

Using smart pointers
The C++ Standard Library provides several classes to wrap resources accessed through pointers.
The Standard Library has three smart pointer classes: unique_ptr, shared_ptr, and weak_ptr. Each handles how the resource is released in a different way, and how or whether you can copy a pointer.

Managing exclusive ownership
The unique_ptr class is constructed with a pointer to the object it will maintain.

// version 1
void f1()
{
	int* p = new int;
	*p = 42;
	cout << *p << endl;
	delete p;
}

// version 2
void f2()
{
	unique_ptr<int> p(new int);
	*p = 42;
	cout << *p << endl;
	delete p.release();
}

// version 3: deterministic releasing of the resource
void f3()
{
	unique_ptr<int> p(new int);
	*p = 42;
	cout << *p << endl;
	p.reset();
}

void f4()
{
	unique_ptr<int> p(new int);
	*p = 42;
	cout << *p << endl;
} // memory is deleted

void f5()
{
	unique_ptr<int> p = make_unique<int>();
	*p = 42;
	cout << *p << endl;
} // memory is deleted

void f6()
{
	unique_ptr<point> p = make_unique<point>(1.0, 1.0);
	p->x = 42;
	cout << p->x << "," << p->y << endl;
} // memory is deleted

You cannot copy assign unique_ptr smart pointers (the copy assignment operator and copy constructor are deleted), but you can move them by transferring ownership of the resource from the source pointer to the destination pointer.

Sharing ownership
There are occasions when you will need to share a pointer, You need a mechanism where several objects can hold a pointer that will remain valid until all the objects using that pointer have indicated they will no longer need to use it.

C++11 provides this facility with the shared_ptr class. This class maintains a reference count on the resource, and each copy of the shared_ptr for that resource will increment the reference count.

shared_ptr<point> sp1 = make_shared<point>(1.0,1.0);

You can create a shared_ptr object from a unique_ptr object, which means that the pointer is moved to the new object and the reference counting control block created.


Handling dangling pointers
You cannot use a weak_ptr object directly and there is no dereference operator. Instead, you create a weak_ptr object from a shared_ptr object and, when you want to access the resource, you create a shared_ptr object from the weak_ptr object. This means that a weak_ptr object has the same raw pointer, and access to the same control block as the shared_ptr object, but it does not take part in reference counting.

Once created, the weak_ptr object will enable you to test whether the wrapper pointer is to an existing resource or to a resource that has been destroyed
There are two ways to do this: 
1.either call the member function expired or attempt to create a shared_ptr from the weak_ptr
2.create a shared_ptr object from it

	shared_ptr<point> sp1 = make_shared<point>(1.0,1.0);
	weak_ptr<point> wp(sp1);

	// code that may call sp1.reset() or may not

	if (!wp.expired()) { /* can use the resource */}

	shared_ptr<point> sp2 = wp.lock();
	if (sp2 != nullptr) { /* can use the resource */}

	try
	{
		shared_ptr<point> sp3(wp);
		// use the pointer
	}
	catch(bad_weak_ptr& e)
	{
		// dangling weak pointer
	}

Templates
Classes can be templated, which means that you can write generic code and the compiler will generate a class with the types that your code uses

template <int N, typename T>
class simple_array
{
	T data[N];
public:
	const T* begin() const { return data; }
	const T* end() const { return data + N; }
	int size() const { return N; }

	T& operator[](int idx)
	{
		if (idx < 0 || idx >= N)
			throw range_error("Range 0 to " + to_string(N));
		return data[idx];
	}
};

simple_array<4, int> four;
four[0] = 10; four[1] = 20; four[2] = 30; four[3] = 40;
for(int i : four) cout << i << " "; // 10 20 30 40
cout << endl;
four[4] = -99; // throws a range_error exception

//  define a function out of the class declaration
template<int N, typename T>
T& simple_array<N,T>::operator[](int idx)
{
	if (idx < 0 || idx >= N)
		throw range_error("Range 0 to " + to_string(N));
	return data[idx];
}

// have default values for template parameters
template<int N, typename T=int> class simple_array
{
	// same as before
};


// have a specific implementation for a template parameter
template<int N> 
class simple_array<N, char>
{
	char data[N];
public:
	simple_array<N, char>(const char* str)
	{
		strncpy(data, str, N);
	}
	int size() const { return N; }
	char& operator[](int idx)
	{
		if (idx < 0 || idx >= N)
			throw range_error("Range 0 to " + to_string(N));
		return data[idx];
	}
	operator const char*() const { return data; }
};

Note that, with a specialization, you do not get any code from the fully templated class; you have to implement all the methods you want to provide, and, as illustrated here, methods that are relevant to the specialization but not available on the fully templated class.


Using classes


Object-Orientated Programming

Inheritance and composition

concrete classes

reuse class in a new class:
1. composition: add an instance of your utility class as a data member of the classes that will use the routine
2. inheritance: inheritance is when one class extends another class the class being extended is called the base class, parent class, or superclass, and the class doing the extending is called a derived class, child class, or subclass

differences between composition and inheritance:
1.  in composition, the composed object is used by the class and not exposed directly to the client of the class. With inheritance, an object of the derived class is an object of the base class, so usually the client code will see the base class functionality, the derived class can override the base class methods and provide its own version

Inheriting from a class

class os_file
{
	const string file_name;
	int file_handle;
	// other data members
public:
	long get_size_in_bytes();
	// other methods
};

class mp3_file : public os_file		// public inheritance
{
	long length_in_secs;
	// other data members
public:
	long get_length_in_seconds();
	// other methods
};

The encapsulation principle is important in C++. Although an object of child class contains the base class data members, they should only be changed by the base class methods.

When a derived object is created, the base object must be created first (with an appropriate constructor), similarly, when a derived object is destroyed, the derived part of the object is destroyed first (through the destructor of the derived class) before the base class destructor is called

If you do not explicitly call a base class constructor, then the compiler will call the default constructor of the base class as the first action of the derived class constructor. If the member list initializes data members, these will be initialized after any base class constructor is called.

Overriding methods and hiding names
The derived class inherits the functionality of the base class (subject to the access level of the methods), so a base class method can be called through an object of the derived class.

struct base
{
	void f(){ /* do something */ }
	void g(){ /* do something */ }
};

struct derived : base
{
	void f(int i)
	{
		base::f();
		// do more stuff with i
	}
};

derived d;
d.f(42); // OK
d.f(); // won't compile, derived::f(int) hides base::f

derived d;
d.derived::f(42); // same call as above
d.base::f(); // call base class method
derived *p = &d; // get an object pointer
p->base::f(); // call base class method
delete p;


Using pointers and references

A pointer (or a reference) to an instance of a derived class can be implicitly converted to a pointer (or a reference) to a base class object.

// cast base class pointer to a derived class pointer
// bad code
void print_y(base *pb)
{
	// be wary of this
	derived *pd = static_cast<derived*>(pb);
	cout << "y = " << pd->y << endl;
}

void f()
{
	derived d;
	print_y(&d); // implicit cast to base*
}

Access levels

Members declared in the public section can be accessed by code in the class and by code outside the class either on an object or if the member is static, using the class name.
Members declared in the private section can only be accessed by other members in the same class
A derived class can access the private members of the base class but not the private members
Members declared in the protected section can be accessed by methods in the same class or by methods in any derived class and by friends, but not by external code

class base
{
protected:
	void test();
};

class derived : public base
{
public:
	void f() { test(); }
};

base b;
b.test(); // won't compile
derived d;
d.f(); // OK
d.test(); // won't compile

If you are writing a base class that you intend only ever to be used as a base class (client code should not create instances of it), then it makes sense to make the destructor protected

class base
{
public:
	// methods available through the derived object
	protected:
	~base(){}
};


Changing access level through inheritance
When you override a method in the derived class, the access to the method is defined by the derived class. SO the access can be changed by the derived class

class base
{
protected:
	void f();
public:
	void g();
};

class derived : public base
{
public:
	void f();
protected:
	void g();
};

// You can also expose a protected base class from a derived class as a public member with a using statement

class base
{
protected:
	void f(){ /* code */};
};

class derived: public base
{
public:
	using base::f;		// the derived::f method is public without the derived class creating a new method
};


// make a method private so that it is not available to derived classes

class base
{
public:
	void f();
};

class derived: public base
{
protected:
	using base::f;
};

base b;
b.f(); // OK
derived d;
d.f(); // won't compile because the f method is protected


// make the method available only in the derived class and not to in any classes that may derive from it

class derived: public base
{
public:
	void f() = delete;
	void g()
	{
		base::f(); // call the base class method
	}
};

Inheritance access levels
If a base class has private members, and a class inherits using public inheritance: 
the derived class still cannot access the private members; it only has access to public and protected members and objects of the derived class can only access the public members, and a class deriving from this class will only have access to the public and protected members.

If a derived class derives through the protected inheritance: 
it still has the same access to the base class as public and protected members, but the base class public and protected members will now be treated as protected through the derived class, so they can be accessed by a further derived class but are not accessible through an instance. 

If a class derives through private inheritance:
 all base class members become private in the derived class; so, although the derived class can access public and protected members, classes that derive from it cannot access any of the base class members.


One way of looking at protected inheritance is if the derived class had a using statement for each of the public members of the base class in the protected part of the class.
Similarly, private inheritance is as if you have deleted each of the public and protected methods of the base class.

most inheritance will be through public inheritance
private inheritance has a use when you want to access some functionality from a base class but do not want its functionality to be available to classes that derive from your class


Multiple inheritance
C++ allows you to inherit from more than one base class. This is a powerful facility when used with interfaces

class base1 { public: void a(); };
class base2 { public: void b(); };
class derived : public base1, public base2
{
public:
	// gets a and b
};

It is important when you consider multiple inheritances that you carefully review that you need the services via inheritance or whether composition is more appropriate. If a class provides a member that you do not want to be used by instances and you decide that you need to delete it, it is a good sign that you should consider composition.


Object slicing

if you cast a derived class object to a base class object, you create a new object, and that object is the base class object, just the base class object.

It is almost always a better idea to pass objects by reference


Introducing polymorphism

Polymorphism comes from the Greek for many shapes

A derived class pointer can be implicitly converted to a base class pointer, so a base* pointer can point to an instance of base, derived1, derived2, or derived3

The polymorphic aspect is that through pointers (or references), an instance of a class can be treated as an instance of any of the classes in its inheritance hierarchy.


Virtual methods
A base class pointer or reference giving access to just the base class functionality, and makes sense, but it is restrictive

The behavior of calling the derived method through a base class pointer is known as method dispatching. This method dispatching is not applied by default because it involves a little extra cost both in memory and performance.

Methods that can take part in method dispatching are marked with the keyword virtual in the base class, and hence are usually called virtual methods. When you call such a method through a base class pointer, the compiler ensures that the method on the actual object's class is called. Since every method has a this pointer as a hidden parameter, the method dispatching mechanism must ensure that the this pointer is appropriate when the method is called. 


// 1
struct base
{
    // void who() { cout << "base "; }				// output: base base base
    virtual void who() { cout << "base "; }			// output: derived1 derived2 derived3
};

struct derived1 : base
{
    void who() { cout << "derived1 "; }
};

struct derived2 : base
{
    void who() { cout << "derived2 "; }
};

struct derived3 : derived2
{
    void who() { cout << "derived3 "; }
};

void who_is_it(base& p)
{
    p.who();
}


derived1 d1;
who_is_it(d1);
derived2 d2;
who_is_it(d2);
derived3 d3;
who_is_it(d3);
cout << endl;


It is important to point out that the method dispatching is applied only to the methods that virtual has been applied to in the base class
Any other methods in the base class not marked with virtual will be called without method dispatching. 
A derived class will inherit a virtual method and get the method dispatching automatically, it does not have to use the virtual keyword on any methods it overrides, but it is a useful visual indication as to how the method can be called

For a virtual method to be called using method dispatching, the derived class method must match the same signature as the base class' virtual method in terms of the name, parameters, and return type.
The one exception is if two methods differ by return types that are covariant, that is, one type can be converted to the other.


Virtual method tables
When the compiler sees a virtual method on a class, it will create a method pointer table, called the vtable, and put a pointer to each of the virtual methods in the class in the table. There will be a single copy of the vtable for the class. The compiler will also add a pointer to this table, called the vptr, in every instance of the class.

![vptr and vtable illustration for base and derived class](image-24.png)

![vptr and vtable illustration for base and derived class 2](image-25.png)


Multiple inheritance and virtual method tables

![vptr and vtable illustration for base and derived class 3](image-26.png)


Virtual methods, construction, and destruction
You should not call a virtual method in a constructor or a destructor, if you do, the call will resolve to the base class version of the method

In general, a base class destructor should be either protected and non-virtual, or public and virtual.


Containers and virtual methods

// not work
derived1 d1;
derived2 d2;
derived3 d3;
vector<base> vec = { d1, d2, d3 };
for (auto b : vec) b.who();
cout << endl;

// work version
vector<reference_wrapper<base> > vec = { d1, d2, d3 };
for (auto b : vec) b.get().who();						// the get method will return a reference to the wrapped object
cout << endl;

Friends and inheritance
In C++, friendship is not inherited. If a class makes another class (or function) a friend, it means that the friend has access to its private and protected members as if the friend is a member of the class. If you derive from the friend class, the new class is not a friend of the first class, and it has no access to the members of that first class.

class base
{
	int x = 0;
public:
	friend ostream& operator<<(ostream& stm, const base& b)
	{
		// thru b we can access the base private/protected members
		stm << "base: " << b.x << " ";
		return stm;
	}
};

the friend function cannot be called as a virtual method, but it can call virtual methods and get the method dispatching

class base
{
	int x = 0;
protected:
	// The actual work of printing out the object is delegated to a virtual function called output
	virtual void output(ostream& stm) const { stm << x << " "; }
public:
	friend ostream& operator<<(ostream& stm, const base& b)
	{
		b.output(stm);
		return stm;
	}
};

class derived : public base
{
	int y = 0;
protected:
	virtual void output(ostream& stm) const
	{
		base::output(stm);
		stm << y << " ";
	}
};


Override and final

When the compiler sees the override specifier, it knows that you intend to override a virtual method inherited from a base class and it will search the inheritance chain to find a suitable method. If no such method can be found, then the compiler will issue an error

struct base
{
	virtual int f(int i);
};

struct derived: base
{
	// derived::f won't compile because there is no method in the inheritance chain with the same signature
	virtual int f(short i) override;
};

The override specifier gets the compiler to perform some useful checks, so it is a good habit to use it on all derived overridden methods

C++11 also provides a specifier called final, which you can apply to a method to indicate that a derived class cannot override it, or you can apply it to a class to indicate that you cannot derive from it

class complete final { /* code */ };
class extend: public complete{}; // won't compile


Virtual inheritance

diamond problem with multiple inheritance:

	struct base { int x = 0; };
	struct derived1 : base { /*members*/ };
	struct derived2 : base { /*members*/ };
	struct most_derived : derived1, derived2 { /*members*/ };
	// most_derived object will have two copies of the data member x

virtual inheritance:

	struct derived1 : virtual base { /*members*/ };
	struct derived2 : virtual base { /*members*/ };

	Without virtual inheritance, derived classes just call the constructors of their immediate parent.
	When you use virtual inheritance, the most_derived class has the responsibility to call the constructor of the topmost parent class and if you do not explicitly call the base class constructor, the compiler will automatically call the default constructor

	derived1::derived1() : base(){}
	derived2::derived2() : base(){}
	most_derived::most_derived() : derived1(), derived2(), base(){}


Abstract classes
A class with virtual methods is still a concrete class--you can create instances of the class.
You need a mechanism to force a derived class to provide an implementation of those virtual methods.

C++ provides a mechanism called pure virtual methods that indicates that the method should be overridden by a derived class. 

struct abstract_base
{
	// the = 0 syntax indicates that the method body is not provided by the abstract class
	virtual void f() = 0;
	void g()
	{
		cout << "do something" << endl;
		f();
	}
};

abstract_base b;	// can not compile

// can create pointers or references to the class and call code on them
void call_it(abstract_base& r)
{
	r.g();
}

// you can call the pure virtual function outside the class too
void call_it2(abstract_base& r)
{
	r.f();
}

By declaring a pure virtual function, you make the class abstract, which means that you cannot create instances. You can, however, create pointers or references to the class and call code on them

The only way to use an abstract class is to derive from it and implement the pure virtual functions:

struct derived1 : abstract_base
{
	virtual void f() override { cout << "derived1::f" << endl; }
};

struct derived2 : abstract_base
{
	virtual void f() override { cout << "derived2::f" << endl; }
};

derived1 d1;
call_it(d1);
derived2 d2;
call_it(d2);


// the abstract base class can also provide a body for the method
struct abstract_base
{
	virtual int h() = 0 { return 42; }
};

// this class cannot be instantiated, you must derive from it and you must implement the method to be able to instantiate an object
struct derived : abstract_base
{
	virtual int h() override { return abstract_base::h() * 10; }
};

The derived class can call the pure virtual function defined in the abstract class, but when external code calls such a method, it will always result (through method dispatching) in a call to the implementation of the virtual method on the derived class


Obtaining type information
C++ provides type information, that is, you can get information that is unique to that type and, which identifies it.

Runtime Type Information (RTTI): obtain type information at runtime

string str = "hello";
const type_info& ti = typeid(str);
cout << ti.name() << endl;


// 
struct base {};
struct derived { void f(); };
void call_me(base *bp)
{
	derived *dp = (typeid(*bp) == typeid(derived))
		? static_cast<derived*>(bp) : nullptr;
	if (dp != nullptr) dp->f();
}

int main()
{
	derived d;
	call_me(&d);
	return 0;
}

// new call_me function
void call_me(base *bp)
{
	derived *dp = dynamic_cast<derived*>(bp);
	if (dp != nullptr) dp->f();
}

The dynamic_cast operator can be used for casts other than downcasts


//
struct base1 { void f(); };
struct base2 { void g(); };
struct derived : base1, base2 {};

void call_me(base1 *b1)
{
	base2 *b2 = dynamic_cast<base2*>(b1);
	if (b2 != nullptr) b2->g();
}


Smart pointers and virtual methods
If you want to use dynamically created objects, you will want to use smart pointers to manage their lifetime. The good news is that virtual method dispatching works through smart pointers (they are simply wrappers around object pointers), and the bad news is that the class relationships are lost when you use smart pointers.


Interfaces
Pure virtual functions and virtual method dispatching leads to an incredibly powerful way of writing object-orientated code, which is called interfaces. 
An interface is a class that has no functionality; it only has pure virtual functions. 
The purpose of an interface is to define a behavior. 
A concrete class that derives from an interface must provide an implementation of all of the methods on the interface, and hence this makes the interface a kind of contract.

// the define makes it more obvious that we are defining abstract classes as interfaces
#define interface struct
interface IPrint
{
	virtual void set_page(/*size, orientation etc*/) = 0;
	virtual void print_page(const string &str) = 0;
};

class inkjet_printer : public IPrint
{
public:
	virtual void set_page(/*size, orientation etc*/) override
	{
	// set page properties
	}

	virtual void print_page(const string &str) override
	{
	cout << str << endl;
	}
};

void print_doc(IPrint *printer, vector<string> doc);

inkjet_printer inkjet;
IPrint *printer = &inkjet;
printer->set_page(/*properties*/);
vector<string> doc {"page 1", "page 2", "page 3"};
print_doc(printer, doc);


interface IScan
{
	virtual void set_page(/*resolution etc*/) = 0;
	virtual string scan_page() = 0;
};

class inkjet_printer : public IPrint, public IScan
{
public:
	// The class already implements a method called set_page, We can address this with two different methods and qualifying their names
	virtual void IPrint::set_page(/*etc*/) override { /*etc*/ }
	virtual void print_page(const string &str) override
	{
		cout << str << endl;
	}
	virtual void IScan::set_page(/*etc*/) override { /*etc*/ }
	virtual string scan_page() override
	{
		static int page_no;
		string str("page ");
		str += to_string(++page_no);
		return str;
	}
};

void scan_doc(IScan *scanner, int num_pages);

inkjet_printer inkjet;
IScan *scanner = &inkjet;
scanner->set_page(/*properties*/);
scan_doc(scanner, 5)

IPrint *printer = dynamic_cast<IPrint*>(scanner);
if (printer != nullptr)
{
	printer->set_page(/*properties*/);
	vector<string> doc {"page 1", "page 2", "page 3"};
	print_doc(printer, doc);
}


The advantage of using interfaces because the class implementation can change completely, but as long as it continues to implement the interfaces that the client code uses, users of the class can continue to use the class (although a recompile will be needed)


interface inheritance: derive from the interface that needs changing and create a new interface

	interface IPrint2 : IPrint
	{
		virtual void print_doc(const vector<string> &doc) = 0;
	};

	class inkjet_printer : public IPrint2, public IScan
	{
	public:
		virtual void print_doc(const vector<string> &doc) override {
			/* code*/
		}
		// other methods
	}


Class relationships

inheritance offers some benefits, but it should not be treated as the best or only solution

At the highest level, you should be aware of three main issues to avoid:
	Rigidity: It is too hard to change a class because any change will affect too many other classes.
	Fragility: When you change your class, it could cause unexpected changes in other classes.
	Immobility: It is hard to reuse the class because it is too dependent on other classes

In general, you should design your classes to avoid tight coupling between classes and interface programming is an excellent way to do this because an interface is simply a behavior and not an instance of a specific class.
Another principle is that in general you should design your classes to be extendable. A more lightweight form of refining an algorithm is to pass a method pointer (or a functor), or an interface pointer to the method of a class for that method to call at an appropriate time to refine how it works. (For example, most sort algorithms require that you pass a method pointer to perform comparisons of two objects of the type that it is sorting.)



Using mixin classes
The mixin technique allows you to provide extensibility to classes without the lifetime issues of composition or the heavyweight aspect of raw inheritance.

Instead of the developer deriving from a base class provided by the library and extending the functionality provided, the mixin class provided by the library is derived from a class provided by the developer.


C++ allows you to provide a type through a template parameter so that the class is instantiated using this type at compile time. With mixin classes, the type passed through a template parameter is the name of a type that will be used as the base class. The developer simply provides a class with the specific methods and then creates a specialization of the mixin class using their class as the template parameter


// Library code
template <typename BASE>
class mixin : public BASE
{
public:
	void something()
	{
		cout << "mixin do something" << endl;
		// a client developer using the functionality of the mixin class must implement a method with this name and with the same prototype, otherwise the mixin class cannot be used
		BASE::something();
		cout << "mixin something else" << endl;
	}
};

// Client code to adapt the mixin class
class impl
{
public:
	void something()
	{
		cout << "impl do something" << endl;
	}
};

mixin<impl> obj;
obj.something();

output:
mixin do something
impl do something
mixin something else


Using the Standard Library Containers

Working with pairs and tuples

associate two items together

template <typename T1, typename T2>
struct pair
{
	T1 first;
	T2 second;
	// other members
};

auto name_age = make_pair("Richard", 52);

pair <int, int> a(1, 1);
pair <int, int> a(1, 2);
cout << boolalpha;
cout << a << " < " << b << " " << (a < b) << endl;


int i1 = 0, i2 = 0;
pair<int&, int&> p(i1, i2);
++p.first; // changes i1

In C++11 you can use the ref function (in <functional>) to specify that the pair will be for references
	auto p2 = make_pair(ref(i1), ref(i2));
	++p2.first; // changes i1

The pair class allows you to return two values in one object.


auto p = minmax(20,10);
cout << "{" << p.first << "," << p.second << "}" << endl;

The Standard Library provides the tuple class that you can have any number of parameters of any type. 

tuple<int, int, int> t3 { 1,2,3 };
cout << "{"
	<< get<0>(t3) << "," << get<1>(t3) << "," << get<2>(t3)
	<< "}" << endl; // {1,2,3}


int& tmp = get<0>(t3);
tmp = 42;				// changes the first item to 42
get<1>(t3) = 99;		// changes the second item to 99

// extract all the items with one call
int i1, i2, i3;
tie(i1, i2, i3) = t3;
cout << i1 << "," << i2 << "," << i3 << endl;

//
tuple<int&, int&, int&> tr3 = tie(i1, i2, i3);
tr3 = t3;


Containers
The Standard Library containers allow you to group together zero or more items of the same type and access them serially through iterators. Every such object has a begin method that returns an iterator object to the first item and an end function that returns an iterator object for the item after the last item in the container. 


// 1
vector<int> primes{1, 3, 5, 7, 11, 13};
for (size_t idx = 0; idx < primes.size(); ++idx)
{
	cout << primes[idx] << " ";
}
cout << endl;


// better version for 1 since not all containers allow random access
template<typename container> void print(container& items)
{
	for (container::iterator it = items.begin();
		it != items.end(); ++it)
	{
		cout << *it << " ";
	}
	cout << endl;
}


Sequence containers
Sequence containers store a series of items and the order that they are stored in, and, when you access them with an iterator, the items are retrieved in the order in which they were put into the container. After creating a container, you can change the sort order with library functions.

List
As the name suggests, a list object is implemented by a doubly linked list in which each item has a link to the next item and the previous one.

list<int> primes{ 3,5,7 };
primes.push_back(11);
primes.push_back(13);
primes.push_front(2);
primes.push_front(1);

int last = primes.back(); // get the last item
primes.pop_back(); // remove it

auto start = primes.begin(); // 1
start++; // 2
auto last = start; // 2
last++; // 3
last++; // 5
primes.erase(start, last); // remove 2 and 3

list<int> planck{ 6,6,2,6,0,7,0,0,4,0 };
planck.remove(6); // {2,0,7,0,0,4,0}

list<int> planck{ 6,6,2,6,0,7,0,0,4,0 };
auto it = planck.begin();
++it;
++it;
planck.insert(it, -1); // {6,6,-1,2,6,0,7,0,0,4,0}

//
struct point
{
	double x = 0, y = 0;
	point(double _x, double _y) : x(_x), y(_y) {}
};

list<point> points;
point p(1.0, 1.0);
points.push_back(p);
points.emplace_back(2.0, 2.0);

//
list<int> num1 { 2,7,1,8,2,8 }; // digits of Euler's number
list<int> num2 { 3,1,4,5,6,8 }; // digits of pi
num1.swap(num2);

//
list<int> num1 { 2,7,1,8,2,8 }; // digits of Euler's number
list<int> num2 { 3,1,4,5,6,8 }; // digits of pi
num1.sort(); // {1,2,2,7,8,8}
num2.sort(); // {1,3,4,5,6,8}
num1.merge(num2); // {1,1,2,2,3,4,5,6,7,8,8,8}

num1.unique(); // {1,2,3,4,5,6,7,8}


Forward list
The forward_list class is like the list class, but it only allows items to insert and remove items from the front of the list

forward_list<int> euler { 2,7,1,8,2,8 };
euler.push_front(-1); 		// { -1,2,7,1,8,2,8 }
auto it = euler.begin(); 	// iterator points to -1
euler.insert_after(it, -2); // { -1,-2,2,7,1,8,2,8 }
euler.pop_front(); 			// { -2,2,7,1,8,2,8 }
euler.remove_if([](int i){return i < 0;});
							// { 2,7,1,8,2,8 }


Vector
The vector class has the behavior of a dynamic array; that is, there is indexed random access to items and the container will grow as more items are inserted into it. You can create a vector object with an initialization list, and with a specified number of copies of an item.

vector<int> distrib(10); // ten intervals
for (int count = 0; count < 1000; ++count)
{
	int val = rand() % 10;
	++distrib[val];
}
for (int i : distrib) cout << i << endl;


Deque
The name deque means double-ended queue, which means that it can grow from both ends, and, although you can insert items in the middle, it is more expensive. As a queue, it means that the items are ordered, but, because the items can be put into the queue from either end, the order is not necessarily the same order in which you put items into the container.


Associative containers
An associative container allows you to provide indexes that are not numeric; these are the keys, and you can associate values with them. As you insert key-value pairs into the container, they will be ordered so that the container can subsequently efficiently access the value by its key.

Maps and multimaps
A map container stores two different items, a key and a value, and it maintains the items in an sort order according to the key. A sorted map means that it is quick to locate an item.

map<string, int> people;
people.emplace("Washington", 1789);
people.emplace("Adams", 1797);
people.emplace("Jefferson", 1801);
people.emplace("Madison", 1809);
people.emplace("Monroe", 1817);
auto it = people.begin();
pair<string, int> first_item = *it;
cout << first_item.first << " " << first_item.second << endl;

Once you have filled the map you can search for a value using the following:
	The at method, which is passed a key and returns a reference to the value for that key
	The [] operator, which when passed a key returns a reference to the value for that key
	The find function, which will use the predicate specified in the template (unlike the global find function, mentioned later) and it will give you an iterator to the entire item as a pair object
	The begin method will give you an iterator to the first item and the end method will give you an iterator after the last item
	The lower_bound method returns an iterator to the item that has a key equal to or greater than the key that you pass as a parameter
	The upper_bound method returns an iterator of the first item in the map that has a key greater than the key provided
	The equal_range method returns both the lower and upper bounds values in a pair object

Sets and multisets
Sets behave as if they are maps, but the key is the same as the value;

set<string> people{
	"Washington","Adams", "Jefferson","Madison","Monroe",
	"Adams", "Van Buren","Harrison","Tyler","Polk"};
for (string s : people) cout << s << endl;


Unordered containers


Special purpose containers

queue<int> primes;
primes.push(1);
primes.push(2);
primes.push(3);
primes.push(5);
primes.push(7);
primes.push(11);
while (primes.size() > 0)
{
	cout << primes.front() << ",";
	primes.pop();
}
cout << endl; // prints 1,2,3,5,7,11

//
struct task
{
	string name;
	int priority;
	task(const string& n, int p) : name(n), priority(p) {}
	bool operator <(const task& rhs) const {
		return this->priority < rhs.priority;
	}
};

priority_queue<task> to_do;
to_do.push(task("tidy desk", 1));
to_do.push(task("check in code", 10));
to_do.push(task("write spec", 8));
to_do.push(task("strategy meeting", 8));
while (to_do.size() > 0)
{
	cout << to_do.top().name << " " << to_do.top().priority << endl;
	to_do.pop();
}


Using iterators

iterators behave like pointers, they are usually objects of iterator classes

All iterators have the following behaviors:
Operator 						Behaviors
* 								Gives access to the element at the current position
++ 								Moves forward to the next element (usually you will use the prefix operator)(this is only if the iterator allows forward movement)
-- 								Moves backward to the previous element (usually you will use the prefix operator)(this is only if the iterator allows backward movement)
== and != 						Compares if two iterators are in the same position
= 								Assigns an iterator


nput and output iterators
As the name suggests, an input iterator will only move forward and will have read access, and an output iterator will only move forward but will have write access. 


Stream iterators
These are adapter classes in <iterators> that can be used to read items from an input stream or write items to an output stream.

vector<int> data { 1,2,3,4,5 };
ostream_iterator<int> my_out(cout, " ");
copy(data.cbegin(), data.cend(), my_out);
cout << endl;


Using iterators with the C Standard Library


Algorithms
The Standard Library has an extensive collection of generic functions in the <algorithm> header file. By generic we mean that they access data via iterators without knowing what the iterators refer to and so it means that you can write generic code to work for any appropriate container. However, if you know the container type and that container has a member method to perform the same action, you should use the member.


Iteration of items
//
vector<int> vec;
vec.resize(5);
fill(vec.begin(), vec.end(), 42);

//
vector<int> vec(5);
generate(vec.begin(), vec.end(),
	[]() {static int i; return ++i; });

//
vector<int> vec { 1,4,9,16,25 };
for_each(vec.begin(), vec.end(),
	[](int i) { cout << i << " "; });
cout << endl;

//
vector<int> vec { 1,2,3,4,5 };
for_each(vec.begin(), vec.end(),
	[](int& i) { i *= i; });

//
vector<int> vec { 1,2,3,4,5 };
vector<int> results;
for_each(vec.begin(), vec.end(),
	[&results](int i) { results.push_back(i*i); });

//
vector<int> vec1 { 1,2,3,4,5 };
vector<int> vec2 { 5,4,3,2,1 };
vector<int> results;
transform(vec1.begin(), vec1.end(), vec2.begin(),
	back_inserter(results), [](int i, int j) { return i*j; });


Getting information

vector<int> planck{ 6,6,2,6,0,7,0,0,4,0 };
auto number = count(planck.begin(), planck.end(), 6);


Comparing containers

vector<int> v1 { 1,2,3,4 };
vector<int> v2 { 1,2 };
vector<int> v3 { 5,6,7 };
cout << boolalpha;
cout << (v1 > v2) << endl; // true
cout << (v1 > v3) << endl; // false


Changing Items

vector<int> planck{ 6,6,2,6,0,7,0,0,4,0 };
vector<int> result(4); // we want 4 items
auto it1 = planck.begin(); // get the first position
it1 += 2; // move forward 2 places
auto it2 = it1 + 4; // move 4 items
move(it1, it2, result.begin()); // {2,6,0,7}


ector<int> planck{ 6,6,2,6,0,7,0,0,4,0 };
vector<int> result;
remove_copy(planck.begin(), planck.end(),
	back_inserter(result), 6);


vector<int> planck{ 6,6,2,6,0,7,0,0,4,0 };
vector<int> temp;
unique_copy(planck.begin(), planck.end(), back_inserter(temp));
planck.assign(temp.begin(), temp.end());


Finding Items

vector<int> planck{ 6,6,2,6,0,7,0,0,4,0 };
auto imin = min_element(planck.begin(), planck.end());
auto imax = max_element(planck.begin(), planck.end());
cout << "values between " << *imin << " and "<< *imax << endl;


// search for duplicates and get the position of those duplicates
vector<int> vec{0,1,2,3,4,4,5,6,7,7,7,8,9};
vector<int>::iterator it = vec.begin();
do
{
	it = adjacent_find(it, vec.end());
	if (it != vec.end())
	{
		cout << "duplicate " << *it << endl;
		++it;
	}
} while (it != vec.end());


Sorting items

vector<int> vec{45,23,67,6,29,44,90,3,64,18};
auto middle = vec.begin() + 5;
partial_sort(vec.begin(), middle, vec.end());
cout << "smallest items" << endl;
for_each(vec.begin(), middle, [](int i) {cout << i << " "; });
cout << endl; // 3 6 18 23 29
cout << "biggest items" << endl;
for_each(middle, vec.end(), [](int i) {cout << i << " "; });
cout << endl; // 67 90 45 64 44


Using the numeric libraries
The Standard Library has several libraries of classes to perform numeric manipulations.

double radius_nm = 10.0;
double volume_nm = pow(radius_nm, 3) * 3.1415 * 4.0 / 3.0;
cout << "for " << radius_nm << "nm "
"the volume is " << volume_nm << "nm3" << endl;
double factor = ((double)nano::num / nano::den);
double vol_factor = pow(factor, 3);
cout << "for " << radius_nm * factor << "m "
"the volume is " << volume_nm * vol_factor << "m3" << endl;


//
template<typename units>
class dist_units
{
	double data;
public:
	dist_units(double d) : data(d) {}
	
	template <class other>
	dist_units(const dist_units<other>& len) : data(len.value() *
		ratio_divide<units, other>::type::den /
		ratio_divide<units, other>::type::num) {}
	
	double value() const { return data; }
};

dist_units<kilo> earth_diameter_km(12742);
cout << earth_diameter_km.value() << "km" << endl;
dist_units<ratio<1>> in_meters(earth_diameter_km);
cout << in_meters.value()<< "m" << endl;
dist_units<ratio<1609344, 1000>> in_miles(earth_diameter_km);
cout << in_miles.value()<< "miles" << endl;


Complex numbers

complex<double> a(1.0, 1.0);
complex<double> b(-0.5, 0.5);
complex<double> c = a + b;
cout << a << " + " << b << " = " << c << endl;
complex<double> d = polar(1.41421, -3.14152 / 4);
cout << d << endl;


Using Strings

Using the string class as a container

string s = "hellon";
copy(s.begin(), s.end(), ostream_iterator<char>(cout));

vector<char> v(s.begin(), s.end());
copy(v.begin(), v.end(), ostream_iterator<char>(cout));


Getting information about a string

Altering strings

Searching strings

// 
string str = "012the678the234the890";
string::size_type pos = 0;
while(true)
{
	pos++;
	pos = str.find("the",pos);
	if (pos == string::npos) break;
	cout << pos << " " << str.substr(pos) << "n";
}
// 3 the678the234the890
// 9 the234the890
// 15 the890

//
string str = "012the678the234the890";
string::size_type pos = string::npos;
while(true)
{
	pos--;
	pos = str.rfind("the",pos);
	if (pos == string::npos) break;
	cout << pos << " " << str.substr(pos) << "n";
}
// 15 the890
// 9 the234the890
// 3 the678the234the890

//
string str = "012the678the234the890";
string::size_type pos = str.find_first_of("eh");
if (pos != string::npos)
{
	cout << "found " << str[pos] << " at position ";
	cout << pos << " " << str.substr(pos) << "n";
}
// found h at position 4 he678the234the890

//
string str = "012the678the234the890";
string::size_type pos = str.find_first_not_of("0123456789");
cout << "found " << str[pos] << " at position ";
cout << pos << " " << str.substr(pos) << "n";
// found t at position 3 the678the234the890

//
string str = " hello ";
cout << "|" << str << "|n"; // | hello |
string str1 = str.substr(str.find_first_not_of(" trn"));
cout << "|" << str1 << "|n"; // |hello |
string str2 = str.substr(0, str.find_last_not_of(" trn") + 1);
cout << "|" << str2 << "|n"; // | hello|


Internationalization

Using facets Internationalization rules are known as facets. A locale object is a container of facets, and you can test if the locale has a specific facet using the has_facet function; if it does, you can get a const reference to the facet by calling the use_facet function.


Converting strings to numbers

string str = "49.5 red balloons";
size_t idx = 0;
double d = stod(str, &idx);
d *= 2;
string rest = str.substr(idx);
cout << d << rest << "n"; // 99 red balloons


Converting numbers to strings

Using stream classes

Outputting floating point numbers

Outputting time and money

Converting numbers to strings using streams

Reading numbers from strings using streams

Using regular expressions

regex rx("[at]"); // search for either a or t
cout << boolalpha;
cout << regex_match("a", rx) << "n"; // true
cout << regex_match("a", rx) << "n"; // true
cout << regex_match("at", rx) << "n"; // false


//
string str("trumpet");
regex rx("(trump)(.*)");
match_results<string::const_iterator> sm;
if (regex_match(str, sm, rx))
{
	cout << "the matches were: ";
	for (unsigned i = 0; i < sm.size(); ++i)
	{
		cout << "[" << sm[i] << "," << sm.position(i) << "] ";
	}
	cout << "n";
} // the matches were: [trumpet,0] [trump,0] [et,5]


//
regex rx("bd{2}b");
smatch mr;
string str = "1 4 10 42 100 999";
string::const_iterator cit = str.begin();
while (regex_search(cit, str.cend(), mr, rx))
{
	cout << mr[0] << "n";
	cit += mr.position() + mr.length();
}


//
string str("trumpet");
regex rx("(trump)(.*)");
match_results<string::const_iterator> sm;
if (regex_match(str, sm, rx))
{
	string fmt = "Results: [$1] [$2]";
	cout << sm.format(fmt) << "n";
} // Results: [trump] [et]

//
string str = "the cat sat on the mat in the bathroom";
regex rx("(b(.at)([^ ]*)");
regex_iterator<string::iterator> next(str.begin(), str.end(), rx);
regex_iterator<string::iterator> end;

for (; next != end; ++next)
{
	cout << next->position() << " " << next->str() << ", ";
}
cout << "n";
// 4 cat, 8 sat, 19 mat, 30 bathroom


Replacing strings

string str = "use the list<int> class in the example";
regex rx("b(list)(<w*> )");
string result = regex_replace(str, rx, "vector$2");
cout << result << "n"; // use the vector<int> class in the example


Diagnostics and Debugging

Using pragmas
Pragmas are compiler-specific and often are concerned with the technical details about the code sections in the object files.



// How to easily make std::cout thread-safe
//There are other ways, but stringstream uses << just like cout.. 
	std::stringstream msg;
	msg << "Error:" << Err_num << ", " << ErrorString( Err_num ) << "\n"; 
	std::cout << msg.str();




```

[**Standard C++ Library reference**](https://cplusplus.com/reference/)

[**Working Draft Programming Languages — C++**](https://eel.is/c++draft/) #online

[OOPS Guidebook](https://github.com/MadhavBahl/OOPS/tree/master) #github

![Object Oriented Programming Using C++](./assets/ObjectOrientedProgrammingUsingCPlusPlus.pdf) #pdf
![OOB Demystified](./assets/OOP_Demystified.pdf) #pdf

[Demystified Object Oriented Programming with CPP](https://github.com/PacktPublishing/Demystified-Object-Oriented-Programming-with-CPP) #github

[Standard for Programming Language C++](https://open-std.org/jtc1/sc22/wg21/docs/papers/2016/n4594.pdf) #online

[**C++ - Standards**](https://www.open-std.org/jtc1/sc22/wg21/docs/standards)

[**LEARN C++**](https://www.learncpp.com/) #online

[C++ By Example](https://cppbyexample.com/) #online

- [**C++ Online Compiler**](https://www.mycompiler.io/new/cpp)

- [**C/C++ Programming**](https://www3.ntu.edu.sg/home/ehchua/programming/#Cpp)


- [OOPS Concepts in C++ with Examples](https://www.prepbytes.com/blog/cpp-programming/oops-concepts-in-c-with-examples/)

- [Learn C++ by Example](https://www.manning.com/books/learn-c-plus-plus-by-example)
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
	- [**Beginning C++ Programming**](https://notalentgeek.github.io/note/note/project/project-independent/pi-brp-beginning-c-programming/document/20170807-1504-cet-1-book-and-source-1.pdf) #pdf
	- [**code repository for Beginning C++ Programming**](https://github.com/PacktPublishing/Beginning-Cpp-Programming)
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