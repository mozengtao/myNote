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


```

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