[CIS198 Rust Programming Slides](https://github.com/cis198-2016s/slides)  
[Rust Lifetimes: A Complete Guide to Ownership and Borrowing](https://earthly.dev/blog/rust-lifetimes-ownership-burrowing/)  
[What are Lifetimes in Rust?](https://www.freecodecamp.org/news/what-are-lifetimes-in-rust-explained-with-code-examples/)  
[Effective Rust](https://effective-rust.com/title-page.html)  
[The Rust Programming Language](https://doc.rust-lang.org/book/)  
[Rust Documentation](https://web.mit.edu/rust-lang_v1.25/arch/amd64_ubuntu1404/share/doc/rust/html/)  
[]()  
[]()  
[]()  
[]()  
[]()  

## lifetime
- Lifetimes in Rust are mechanisms for ensuring that all borrows that occur within your code are valid. 
- A variable's lifetime is how long it lives within the program's execution, starting from when it's initialized and ending when it's destroyed in the program.
```rust
max<'a>			// max should live at most as long as 'a

max<'a, 'b>		// max should live at most as long as 'a and 'b

// max should live at most as log as the lifetimes of s1 or s2, it also indicates max returns a reference that lives as long as s1
fn max<'a>(s1: &'a str, s2: &'a str) -> &'a str {
	// return the longest string out of the two
	if s1.len() > s2.len() {
		s1
	} else {
		s2
	}
}

// only need to specify lifetimes if a function returns a reference from one of its arguments that is a borrowed reference
fn print_longest(s1: &str, s2: &str) {
	if s1.len() > s2.len() {
		println!("{s1} is longer than {s2}")
	} else {
		println!("{s2} is longer than {s1}")
	}
}

fn joint_strs(s1: &str, s2: &str) -> String {
	let mut joint_string = String::from(s1);
	joint_string.push_str(s2);
	return joint_string;
}

// structs require explicit lifetime annotations when any of their fields are references
// this allows the borrow checker to ensure that the references in the struct's fields live longer than the struct
struct Strs<'a, 'b> {
	x: &'a str,
	y: &'b str,
}

// lifetime annotations concerning methods can be done as annotations to standalone methods, impl blocks, or traits
// standalone methods
ippl Struct {
	fn max<'a>(self: &Self, s1: &'a str, s2: &'a str) -> &'a str {
		if s1.len() > s2.len() {
			s1
		} else {
			s2
		}
	}
}
// impl blocks
struct Strs<'a> {
	x: &'a str,
	y: &'a str,
}

impl<'a> Strs<'a> {
	fn max(self: &Self) -> &'a str {
		if self.y.len() > self.x.len() {
			self.y
		} else {
			self.x
		}
	}
}
// traits
// Lifetime annotations in traits are dependent on the methods that the trait defines
//  A method inside a trait definition can use explicit lifetime annotations as a standalone method, and the trait definition won't require explicit lifetime annotations
trait Max {
	fn longest_str<'a>(s1: &'a str, s2: &'a str) -> &'a str;
}

impl<'a> Max for Struct<'a> {
	fn longest_str(s1: &'a str, s2: &'a str) {
		if s1.len() > s2.len() {
			s1
		} else {
			s2
		}
	}
}
// if a trait method requires references from the struct its associated with, the trait's definition would require explicit lifetime annotations.
struct Strs<'a> {
	x: &'a str,
	y: &'a str,
}

trait Max<'a> {
	fn max(self: &Self) -> &'a str;
}

impl<'a> Max<'a> for Strs<'a> {
	fn max(self: &Self) ->. &'a str {
		if self.y.len() > self.x.len() {
			self.y
		} else {
			self.x
		}
	}
}

// lifetime annotations in enums
enum Either<'a> {
	Str(String),
	Ref(&'a String),
}


```

## Basic
```rust
// hello_world
fn main() {
    println!("Hello, world!");
}

// variable bindings
let x = 17;			// bindings are implicitly-typed: the compiler infers based on context
let x: i16 = 17;	// add type annotations when compiler can't determine variable type

// variables are inherently immutable
let x = 5;
x += 1;			// error: variables are inherently immutable

// varialble bindings may be shadowed
let x =1;
let x ="hello";		// x is not mutable, but we're able to re-bind it
					// the shadowed bindings for variable lasts until it goes out of scope

// declare variables using patterns
let (a, b) = ("foo", 12);

// expressions
// Everything is an expression: something which returns a value
	// exception: variable bindings are not expressions

// "unit" is "nothing" type: ()
	// the type () has only one value: ()
	// () is the default return type

// appending a semicolon can be used to discard an expression's value, now it returns ()
fn foo() -> i32 { 5 }
fn bar() -> () { () }
fn baz() -> () { 5; }
fn qux() { 5; }

// we can bind many things to variable names because everything is an expression
let x = 5;
let y = if x > 0 { "greater" } else { "less" };
println!("x = {} is {} than zero", x, y);
// {} is string interpolation operator

/// types
// prititive types
bool: true/false
char: 'c', '😺' (chars are Unicode)
i8, i16, i32, i64, isize
u8, u16, u32, u64, usize
f32, f64
isize, usize (size of pointers, machine-dependent)
// literals
10i8, 10u16, 10.0f32, 10usize
// type inference for non-specific literals default to i32 or f64
10		// i32
10.0	// f64
// arrays
// slices
// str
// tuples

// Array
// arrays are generically of type [T; N], N is a compile-time constant, array can not be resized
let arr1 = [1, 2, 3];	// array of 3 elements
let arr2 = [2; 32];		// array of 32 `2`s
println!("{}", arr1[0])
println!("{:?}", arr1);

// slices
// slices are generically of type &[T]
// A "view" into an array by reference
// Not created directly, but are borrowed from other variables
// Mutable or immutable
let arr = [0, 1, 2, 3, 4, 5];
let total_slice = &arr;			// slice all of `arr`
let total_slice = &arr[..]		// Same, but more explicit
let partial_slice = &arr[2..5]	// [2, 3, 4]

// strings
// two types of strings: String and &str
// String i a heap-allocated, growable vector of characters
// &str is a type that's used to slice into String s
// string literals like "foo" are of type &str
let s: &str = "galaxy";
let s2: String = "galaxy".to_string();
let s3: String = String:from("galaxy");
let s4: &str = &s3;
// str is an unsized type, which doesn't have a compile-time known size, and therefore cannot exist by itself

// tuple
// fixed-size, ordered, heterogeneous lists
// indexed into tupes with foo.0, foo.1, etc.
// can be destructured in let bindings
let foo: (i32, char, f64) = (72, 'H', 5.1);
let (x, y, z) = (72, 'H', 5.1);
let (a, b, c) = foo;	// a = 72, b = 'H', c = 5.1

// casting
// cast between types with as
let x: i32 = 100;
let y: u32 = x as u32;

// vector
// A Vec<T> is a heap-allocated growable array, T denotes a generic type
let v0: Vec<i32> = Vec:new();

// v1 and v2 are equal
let mut v1 = Vec::new();
v1.push(1);
v1.push(2);
v1.push(3);

let v2 = vec![1, 2, 3];

// v3 and v4 are equal
let v3 = vec![0; 4];
let v4 = vec![0, 0, 0, 0];

// Vec::new() is namespacing, new is a function defined for Vec struct

let v2 = vec![1, 2, 3];
let x = v2[2];	// 3
// you can't index a vector with an i32/i64/etc.
// you must use a usize because usize is guaranteed to be the same size as a pointer
// other integers can be cast to usize
let i: i8 = 2;
let y = v2[i as usize];

// reference
let x = 12;
let ref_x = &x;
println!("{}", *ref_x);		// 12

// control flow
// if
if x > 0 {
	10
} else if x == 0 {
	0
} else {
	println!("Not greater than zero!");
	-10
}
// no parens necessary
// entire if statement evaluates to one expression, so every arm must end with a expression of the same type, that type can be unit ()
if x <= 0 {
	println!("Too small!");
}

// loops
// while
let mut x = 0;
while x < 100 {
	x += 1;
	println!("x: {}", x);
}

// loop
let mut x = 0;
loop {
	x += 1;
	if x == 10 {
		break;
	}
}
println!("x: {}", x);

// for
// loops from 0 to 9
for x in 0..10 {
	println!("{}", x);
}

let xs = [0, 1, 2, 3, 4];
// loop through elements in a slice of `xs`
for x in &xs {
	println!("{}", x);
}

// functions
fn foo(x: T, y: U, z: V) -> T {
	// ...
}
// the final expression in a function is its return value
	// use return for early returns from a function

fn square(n: i32) -> i32 {
	n * n
}

fn squareish(n: i32) -> i32 {
	if n < 5 { return n; }
	n * n
}

// compile failed
fn square_bad(n: i32) -> i32 {
	n * n;
}

// function objects
// function objects: function pointers, closures
let x: fn(i32) -> i32 = square;

// pass by reference
fn apply_twice(f: &Fn(i32) -> i32, x: i32) -> i32 {
	f(f(x))
}

fn apply_twice<F>(f: F, x: i32) -> i32
where
	F: Fn(i32) -> i32,
{
	f(f(x))
}

let y = apply_twice(&square, 5);

// marcos
// macros generate code at compile time
// macros are defined with "macro_rules! macro_name" blocks
// print! & println!
// {} for general string interpolation, {:?} for debug printing, some types such as array and Vec can only be printed with {:?}
print!("{}, {}, {}", "foo", 3, true);
println!("{:?}, {:?}", "foo", [1, 2, 3]);

// format!
let fmted = format!("{}, {:x}, {:?}", 12, 155, Some("foo"));
println!("{}", fmted);

// panic!
if x < 0 {
	panic!("Oh noes!")
}

// assert! && assert_eq!
// assert!(condition) panics if condition is false
// assert_eq(left, right) panics if left != right
// used for testing and catching illegal conditions
#[test]
fn test_something() {
	let actual = 1 + 2;
	assert!(actual == 3);
	assert_eq!(3, actual);
}

// unreachable!()
// used to indicate that some code should not be reached, panic! when reached
if false {
	unreachable!();
}

// unimplemented!()
// shorthand for panic!("not yet implemented")
fn sum(x: Vec<i32>) -> i32 {
	// TODO
	unimplemented!();
}

// match statement
let x = 3;

match x {
	1 => println!("one fish"),	// comma required
	2 => {
		println!("two fish");
		println!("two fish");
	},		// comma optional when using braces
	_ => println!("no fish for you"),	// otherwise case
}

// match takes an expression and braches on a list of "value => expression" statements
// the entire match evaluates to one expression. (Like if, all arms must evaluate to the same type)
// _ is commonly used as a catch-all

let x = 6;
let y = 1;

match (x, y) {
	(1, 1) => println!("one"),
	(2, j) => println!("two, {}", j),
	(_, 3) => println!("three"),
	(i, j) if i > 5 && j < 0 => println!("On guard"),
	(_, _) => println!(":<"),
}

// the matched expression can be any expression(l-value), including tuples and function calls
	// matches can bind variables. _ is a throw-away variable name
// you must write an exhaustive match in order to compile
// use if-guards to constrain a match to certain conditions
// pattern can get very complex

// rust environment & tools
// rustc
// cargo
cargo new project_name (binary)
cargo new project_name --bin (executable)
cargo build
cargo test
// cargo.toml
// cargo test
#[test]
fn it_works() {
	// ...
}
// cargo check
```

## Ownership
```rust
// a variable binding takes ownership of its data
	// a piece of data can only have one owner at a time
// when a binding goes out of scope, the bound data is released automatically
	// for heap-allocated data, this means de-allocation
// data must be guaranteed to outlive its references
fn foo() {
	// creates a Vec object
	// gives ownership of the Vec object to v1
	let mut v1 = vec![1, 2, 3];

	v1.pop();
	v1.push(4);
	println!("{:?}", v1);

	// at the end of the scope, v1 goes out of scope
	// v1 still owns the Vec object, so it can be cleaned up
}

// move semantics
let v1 = vec![1, 2, 3];

// ownership of  the Vec object moves to v2
let v2 = v1;

println!("{}", v1[2]); // error: use of moved value "v1"

// borrowing
// a varialbe's data can be borrowed by taking a reference to the variable; ownership doesn't change
	// when a reference goes out of scope, the borrow is over
	// the original variable retains ownership throughout
let v = vec![1, 2, 3];
let v_ref = &v;
assert_eq!(v[1], v_ref[1]);

// how borrowing works
// you can take a reference to the original variable and use it to access the data
// when a reference goes out of scope, the borrow is over
// however, the original variable retains ownership during the borrow and afterwards

let v = vec![1, 2, 3];
let v_ref = &v;
// moving ownership to v_new would invalidate v_ref
// error: cannot move out of "v" because it is borrowed
let v_new = v;

// ownership cannot be transfered from a variable while references to it exist, that would invalidate the reference

//// 'length' only needs 'vector' temporarily, so it is borrowed
fn length(vec_ref: &Vec<i32>) -> usize {
	// vec_ref is auto-dereferenced when you call methods on it
	vec_ref.len()
	// you can also explicitly dereference
	// (*vec_ref).len()
}

fn main() {
	let vector = vec![];
	length(&vector);
	println("{:?}", vector);
}

// the type of length: vec_ref is passed by reference, so it's now an &Vec<i32>
// references, like bindings, are immutable by default
// the borrow is over after the reference goes out of scope (at the end of length)

/// 'push' needs to modify 'vector' so it is borrowed mutably
fn push(vec_ref: &mut Vec<i32>, x: i32) {
	vec_ref.push(x);
}

fn main() {
	let mut vector: Vec<i32> = vec![];
	let vector_ref: &mut Vec<i32> = &mut vector;
	push(vector_ref, 4);
	println!("{:?}", vector);
}

// variable can be borrowed by mutable reference: &mut vec_ref
	// vec_ref is a reference to a mutable Vec
	// the type is &mut Vec<i32>, not &Vec<i32>

/// 'push' needs to modify 'vector' so it is borrowed mutably
fn push2(vec_ref: &mut Vec<i32>, x: i32) {
	// error: cannot move out of borrowed content
	let vector = *vec_ref;
	vector.push(x);
}

fn main() {
	let mut vector: Vec<i32> = vec![];
	push2(&mut vector, 4);
}

// you cannot dereference vec_ref into a variable binding because that would change the ownership of the data

/// `length` only needs `vector` temporarily, so it is borrowed.
fn length(vec_ref: &&Vec<i32>) -> usize {
    // vec_ref is auto-dereferenced when you call methods on it.
    println!("Length of the vector is: {}", vec_ref.len());
    vec_ref.len()
}

fn main() {
    let vector = vec![];
    length(&&&&&&&&&&&&vector);
}

// rust will auto-dereference varialbes
	// when making method calls on a reference
	// when passing a reference as a function argument

let mut a = 5;
let ref_a = &mut a;
*ref_a = 4;
println!("{}", *ref_a + 4);

// you have to dereference varialbes
	// when writing into them
	// other times that usage may be ambiguous


// ref
let mut vector = vec![0];

{
	// these are equivalent
	let ref1 = &vector;
	let ref ref2 = vector;
	assert_eq!(ref1, ref2);
}

let ref mut ref3 = vector;
ref3.push(1);

println!("{:?}", vector);

// when binding a variable, ref can be applied to make the variable a reference to the assigned value
	// take a mutable reference with "ref mut"
// this is most useful in match statements when destructuring patterns.

let mut vectors = (vec![0], vec![1]);
match vectors {
	(ref v1, ref mut v2) => {
		v1.len();
		v2.push(2);
	}
}
println!("{:?}, {:?}", vectors.0, vectors.1);

// use ref and ref mut when binding variables inside match statements

// copy types
// copy is a trait that signifies a type may be copied instead whenever it would be moved
let x: i32 = 12;
let y = x;		// i32 is 'copy', so it is not moved
println!("x: {}, y: {}", x, y);

// borrowing rules
	// you cannot keep borrowing something after it stops existing
	// one object may have many immutable references to it (&T), or exactly one mutable reference (&mut T)(not both)

let mut vs = vec![1, 2, 3, 4];
for v in $vs {
	vs.pop();
	// ERROR: cannot borrow 'vs' as mutable because it is also borrowed as immutable
}
// borrowing prevents: iterator invalidation due to mutating a collection you're iterating over

let y: &i32;
{
	let x = 5;
	y = &x;		// ERROR: 'x' does not live long enough
}
// borrowing prevents: use-after-free

// iterate over Vec
let mut vs = vec![0, 1, 2, 3];

// borrow immutably
for v in &vs {	// can also write 'for v in vs.iter()'
	println!("I'm borrowing {}", v);
}

// borrow mutably
for v in &mut vs {	// can also write 'for v in vs.into_iter()'
	*v = *v + 1;
	println!("I'm mutably borrowing {}", v);
}

// take ownership of the whole vector
for v in vs {	// can also write 'for v in vs.into_iter()'
	println!("I now own {}", v);
}
// 'vs' is no longer valid

// structured data
	// structs
	// enums
// structs and enums may have one or more implementation blocks(impl s) which define methods for the data type

// structs
struct Point {		// by convention, structs have CamelCase names, fields have snake_case names
	x: i32,			// fields are declared with 'name: type'
	y: i32,
}

let origin = Point { x: 0, y: 0 };	// structs may be instantiated with fields fields assigned in braces

// you must assign all fields upon creation, or declare an uninitialized struct that you initialiaze later
let mut p = Point { x: 1, y: 2 };
p.x += 1;	// struct fields may be accessed with dot notation
p.y -= 1;

struct Point {
	x: i32,
	mut y: i32,		// Illegal
}
// structs donot have field-level mutability
// mutability is a property of the variable binding, not the type
// field-level mutability(interior mutability) can be achieved via Cell types

mod foo {
	pub struct Point {
		pub x: i32,
		y: i32,
	}
}

fn main() {
	let b = foo::Point { x: 12, y: 12 };
	// error: field 'y' of struct 'foo:Point' is private
}

// structs are namespaced with their module name, ex. foo::Point
// struct fields are private by default, which may be made public with the pub keyword
// private fields may only be accessed from within the module where the struct is declared

mod foo {
    pub struct Point {
        pub x: i32,
        y: i32,
    }

    // create and return a new Point
    pub fn new(x: i32, y: i32) -> Point {
        Point { x, y }
    }
}
// new is inside the same module as Point, so accessing private fields is allowed

// struct matching
pub struct Point {
    x: i32,
    y: i32,
}

fn main() {
    let p = Point { x: 12, y: 13 };

    match p {
        Point { x, y } => {
            println!("({}, {})", x, y);
        }
    }

    match p {
        Point { y, .. } => {
            println!("y = {}", y);
        }
    }
}
// destructure structs with match statements
// fields do not need to be in order
// list fields inside braces to bind struct members to those variable names
	// use 'struct_field: new_var_binding' to change the variable it's bound to
// omit fields: use .. to ignore all unnamed fields

#[derive(Debug)]
struct Foo {
    a: i32,
    b: i32,
    c: i32,
    d: i32,
    e: i32
}

fn main() {
    let mut x = Foo { a: 1, b: 2, c: 3, d: 4, e: 5 };
    let x2 = Foo { e: 4, .. x };

    x = Foo { a: 2, b: 2, e: 2, .. x };
    println!("x = {:?}", x);
    println!("x2 = {:?}", x2);
}

// a struct initializer can contain '.. s' to copy some or all fields from s
// any fields you donot specify in the initializer get copied over from the target struct
// the struct used must be of the same type as the target struct
	// No copying same-type fields from different-type structs

// tuple struct
// variant on structs that has a name, but no named fields
// have numbered field accessors, like tuples (e.g. x.0, x.1, etc)
// can also match these
struct Color(i32, i32, i32);

fn main() {
    let mut c = Color(0, 0, 0);
    c.0 = 255;
    match c {
        Color(r, g, b) => {
            println!("r = {}, g = {}, b = {}", r, g, b);
        }
    }
}

// create a new type that's not just an alias
	// often referred to as the "newtype" pattern
// these two types are structurely identical, but not equatable
// not equatable
struct Meters(i32);
struct Yards(i32);

// may be compared using the == operator, added with '+', etc.
type MetersAlias = i32;
type YardsAlias = i32;

// unit structs (zero-sized types)
// Structs can be declared to have zero size.
	// This struct has no fields!
// We can still instantiate it.
// It can be used as a "marker" type on other data structures.
	// Useful to indicate, e.g., the type of data a container is storing.
struct Unit;
let u = Unit;

// enums
// enum is a way to express some data that may be one of several things
// each enum variant can have
	// no data (unit variant)
	// named data (struct variant)
	// unnamed ordered data (tuple variant)
enum Resultish {
	Ok,
	Warning { code: i32, message: String },
	Err(String)
}

match make_request() {
	Resultish::Ok =>
		println!("Success!"),
	Resultish::Warning { code, message } =>
		println!("Warning: {}!", message),
	Resultish::Err(s) =>
		println!("Failed with error: {}", s),
}

// enum variants are namespaced by enum type: Resultish:Ok
	// you can import all variants with 'use Resultish::*'
// enums can be matched on like any other data type

// recursive types
enum List {
    Nil,
    Cons(i32, List),
}
// error: invalid recursive enum type
// help: wrap the inner value in a box to make it representable

// box
// a box is a general term for one of Rust's ways of allocating data on the heap
// a Box<T> is a heap pointer with exactly one owner
	// a Box owns its data (the T) uniquely -- it can bge aliased
// Box es are automatically destructed when they go out of scope
let boxed_file = Box::new(5);

enum List {
	Nil,
	Cons(i32, Box<List>),	// Ok
}

// methods
struct Point {
    x: i32,
    y: i32,
}

impl Point {
    pub fn distance(&self, other: Point) -> f32 {
        let (dx, dy) = (self.x - other.x, self.y - other.y);
        ((dx.pow(2) + dy.pow(2)) as f32).sqrt()
    }
}

fn main() {
    let p = Point { x: 1, y: 2 };
    let p2 = Point { x: 3, y: 4 };
    println!("distance = {}", p.distance(p2));
}
// methods can be implemented for structs and enums in an 'impl' block
// like fields, methods may be accessed via dot notation
// methods can be made public with 'pub'
	// 'impl' blocks themselves don't need to be made 'pub'
// works for enums in exactly the same way they do for structs

impl Point {
    pub fn distance(&self, other: Point) -> f32 {		// distance needs to access but not modify fields
        let (dx, dy) = (self.x - other.x, self.y - other.y);
        ((dx.pow(2) + dy.pow(2)) as f32).sqrt()
    }

    pub fn translate(&mut self, dx: i32, dy: i32) {		// translate modifies the struct fields
        self.x += dx;
        self.y += dy;
    }

    fn mirror_y(self) -> Point {						// mirror_y returns an entirely new struct, consuming the old one
        Point { x: -self.x, y: self.y }
    }
}

// the first argument to a method, named 'self', determines what kind of ownership the method requires
// &self: the method borrows the value
	// use this unless you need a different ownership model
// &mut self: the method mutably borrows the value
	// the function needs to modify the struct it's called on
// self: the method takes ownership
	// the function consumes the value and may return something else


// associated functions
impl Point {
    fn new(x: i32, y: i32) -> Point {
        Point { x, y }
    }
}

fn main() {
    let p = Point::new(1, 2);
}
// associated function, like a method, but does not take 'self'
	// called with namespacing syntax: Point::new()
// a constructor-like function is usually named 'new'
	// no inherent notion of constructors, no automatic construction

// implementations
// methods, associated functions, and functions in general may be not overloaded
	// e.g. Vec::new() and Vec:with_capacity(capacity: usize) are both constructors for Vec
// methods may not be inherited
	// strucs & enums must be composed instead

// patterns
let x = 17;

match x {
	0 ... 5 => println!("x is between 0 and 5"),
	_ => println!("x is something else"),
}

// use '...' to specify a range of values. useful for numerics and chars
// use '_' to bind against any value (like any variable binding) and discard the binding

let x = 17;

match x {
    ref x_ref => println!("Of type &i32: {}", x_ref),
}
// get a reference to a variable by asking for it with 'ref'

let mut x = 17;

match x {
    ref x_ref if x == 5 => println!("x is {}", x_ref),
    ref mut x_mut_ref => *x_mut_ref = 20
}
// get a mutable reference with 'ref mut'
	// only if the variable was declared 'mut'

// if-let statements
// if you only need a single match arm, it often makes more sense to use 'if-let' construct
enum Resultish {
    Ok,
    Warning { code: i32, message: String },
    Err(String),
}

// Suppose we want to report an error but do nothing on Warning s and Ok s.
match make_request() {
    Resultish::Err(_) => println!("Total and utter failure."),
    _ => println!("ok."),
}

// can be simplified as

let result = make_request();

if let Resultish::Err(s) = result {
    println!("Total and utter failure: {}", s);
} else {
    println!("ok.");
}

// while-let statements
// works like 'if-let', but iterates until the condition fails to match
while let Resultish::Err(s) = make_request() {
	println!("Total and utter failure: {}", s);
}

// inner bindings
// with more complicated data structures, use '@' to create variable bindings for inner elements
#[derive(Debug)]
enum A { None, Some(B) }
#[derive(Debug)]
enum B { None, Some(i32) }

fn foo(x: A) {
    match x {
        a @ A::None              => println!("a is A::{:?}", a),
        ref a @ A::Some(B::None) => println!("a is A::{:?}", *a),
        A::Some(b @ B::Some(_))  => println!("b is B::{:?}", b),
    }
}

foo(A::None);             // ==> x is A::None
foo(A::Some(B::None));    // ==> a is A::Some(None)
foo(A::Some(B::Some(5))); // ==> b is B::Some(5)

// lifetimes
// ordinarily, references have an implicit lifetime that do don't need to care about
fn foo(x: &i32) {
	// ...
}

// explicitly provide the lifetime
fn bar<'a>(x: &'a i32) {
	// ...
}
// 'a, pronounced as 'tick-a' or 'the lifetime a' is a named lifetime parameter
	// <'a> declares generic paramters, including lifetime parameters
	// the type &'a i32 is a reference to an i32 that lives at least as long as the lifetime 'a

// multiple lifetime paramters
fn borrow_x_or_y<'a>(x: &'a str, y: &'a str) -> &'a str;
// all input/output references all have the same lifetime
	// x and y are borrowed (the reference is alive) as long as the returned reference exists

fn borrow_p<'a, 'b>(p: &'a str, q: &'b str) -> &'a str;
// the output reference has the same lifetime as p
	// q has a separate lifetime with no constrained relationship to p
	// p is borrowed as long as the returned reference exists

// if a reference R has a lifetime 'a, it is guaranteed that it will not outlive the owner of its underlying data (the value at *R)
// if a reference R has a lifetime 'a, anything else with the lifetime 'a is guaranteed to live as long as R

struct Pizza(Vec<i32>);
struct PizzaSlice<'a> {
    pizza: &'a Pizza,  // references in structs must ALWAYS have explicit lifetimes
    index: u32,
}

let p1 = Pizza(vec![1, 2, 3, 4]);
{
    let s1 = PizzaSlice { pizza: &p1, index: 2 }; // this is okay
}

let s2;
{
    let p2 = Pizza(vec![1, 2, 3, 4]);
    s2 = PizzaSlice { pizza: &p2, index: 2 };
    // no good - why?
}
// structs (and struct members) can have lifetime parameters

struct Pizza(Vec<i32>);
struct PizzaSlice<'a> { pizza: &'a Pizza, index: u32 }
struct PizzaConsumer<'a, 'b: 'a> { // says "b outlives a"
    slice: PizzaSlice<'a>, // <- currently eating this one
    pizza: &'b Pizza,      // <- so we can get more pizza
}

fn get_another_slice(c: &mut PizzaConsumer, index: u32) {
    c.slice = PizzaSlice { pizza: c.pizza, index: index };
}

let p = Pizza(vec![1, 2, 3, 4]);
{
    let s = PizzaSlice { pizza: &p, index: 1 };
    let mut c = PizzaConsumer { slice: s, pizza: &p };
    get_another_slice(&mut c, 2);
}
// lifetimes can be constrained to 'outlive' others (<'b: 'a>)

// 'static is one reserved, special lifetime, which means that a reference may be kept(and will be valid) for the lifetime of the entire program (the data referred to will never go out of scope)
// all &str literals have the 'static lifetime
let s1: &str = "hello";
let s2: &'static str = "world";

// any struct or enum that contains a reference must have an explicit lifetime
// nomral lifetime rules otherwise apply
struct Foo<'a, 'b> {
	v: &'a Vec<i32>,
	s: &'b str,
}

// lifetimes in 'impl' blocks
impl<'a, 'b> Foo<'a, 'b> {
	fn new(v: &'a Vec<i32>, s: &'b str) -> Foo<'a, 'b> {
		Foo {
			v: v,
			s: s,
		}
	}
}
// implementing methods on Foo struct requires lifetime annotations too
// the block can be read as "the implementation using the lifetimes 'a and 'b for the struct Foo using the lifetimes 'a and 'b"

// generics & traits
enum Result {
	Ok(String),
	Err(String),
}

// generics
enum Result<T, E> {
	Ok(T),
	Err(E),
}
// T and E stand in for any generic type, not only String
// any CamelCase identifier can be used for generic types

// generic structs
struct Point<T> {
	x: T,
	y: T,
}

enum List<T> {
	Nil,
	Cons(T, Box<List<T>>),
}

// generic implementations
impl<T, E> Result<T, E> {
	fn is_ok(&self) -> bool {
		match *self {
			Ok(_) => true,
			Err(_) => false,
		}
	}
}
// to define implementations for structs & enums with generic types, declare the generics at the beginning of the impl block

// implementing functions on a per-type basis to pretty-print, compute equality, etc. is fine, but unstructured
struct Point {
	x: i32,
	y: i32,
}

impl Point {
	fn format(&self) -> String {
		format!("({}, {})", self.x, self.y)
	}

	fn equals(&self, other: Point) -> bool {
		self.x == other.x && self.y == other.y
	}
}

// traits
trait PrettyPrint {
	fn format(&self) -> String;
}
// trait gives function definitions for the required methods
// mostly only contains method signatures without definitions

//
impl PrettyPrint for Point {
	fn format(&self) -> String {
		format!("({}, {})", self.x, self.y)
	}
}
// use 'impl Trait for Type' to implement a trait
	// all methods specified by the trait must be implemented
// one impl block per type per trait
// self/&self can be used inside the trait impl block as usual

// generic functions
fn foo<T, U>(x: T, y: U) {
	// ...
}
// "the function foo, for all types T and U, of two arguments: x of type T and y of type U"
// <T, U> declares the type parameters for foo
	// x: T, y: U uses those type parameters

// trait bounds
// trait bounds can be used to constrain generic types
// trait bounds can be specified with 'T: SomeTrait' or with a 'where' clause
	// "where T is Clone"
fn cloning_machine<T: Clone>(t: T) -> (T, T) {
	(t.clone(), t.clone())
}

fn cloning_machine_2<T>(t: T) -> (T, T)
		where T: Clone {
	(t.clone(), t.clone())
}

// multiple trait bounds are specified like 'T: Clone + Ord'
// there's no way (yet) to specify negative trait bounds
	// e.g. you can't stipulate that a T must not be Clone
fn clone_and_compare<T: Clone + Ord>(t1: T, t2: T) -> bool {
	t1.clone() > t2.clone()
}

// generic types with trait bounds
enum Result<T, E> {
	Ok(T),
	Err(E),
}

trait PrettyPrint {
	fn format(&self) -> String;
}

impl<T: PrettyPrint, E: PrettyPrint> PrettyPrint for Result<T, E> {
	fn format(&self) -> String {
		match *self {
			Ok(t) => format!("Ok({})", t.format()),
			Err(e) => format!("Err({})", e.format()),
		}
	}
}
// be sure to declare all of your generic types in the struct header and the impl block header
// only the impl block header needs to specify trait bounds
	// this is useful if you want to have multiple impls for a struct each with different trait bounds

// just an example
trai Equals {
	fn equals(&self, other: &Self) -> bool;
}

impl<T: Equals, E: Equals> Equals for Result<T, E> {
	fn equals(&self, other: &Self) -> bool {
		match (*self, *other) {
			Ok(t1), Ok(t2) => t1.equals(t2),
			Err(e1), Err(e2) => e1.equals(e2),
			_ => false
		}
	}
}
// Self is a special type which refers to the type of self

// trait inheritance
// some trait may require other traits to be implemented first
trait Parent {
	fn foo(&self) {
		// ...
	}
}

trait Child: Parent {
	fn bar(&self) {
		self.foo();
		// ...
	}
}

// trait: default methods
// implementors of the trait can overwrite default implementations, but make sure you have a good reason to !
	// e.g. never define ne so that it violates the relationship between eq and ne
trait PartialEq<Rhs: ?Sized = Self> {
	fn eq(&self, other: &Rhs) -> bool;

	fn ne(&self, other: &Rhs) -> bool {
		!self.eq(other)
	}
}

trait Eq: PartialEq<Self> {}

// deriving
// many traits are so straightforward that the compiler can often implement them for you
// a '#[derive(...)]' attribute tells the compiler to insert a default implementation for whatever traits you tell it to
// this removes the tedium of repeatedly manually implementing traits like 'Clone' yourself
#[derive(Eq, PartialEq, Debug)]
enum Result<T, E> {
	Ok(T),
	Err(E)
}

// core traits:
	Clone, Copy
	Debug
	Default
	Eq, PartialEq
	Hash
	Ord, PartialOrd
// can only derive a trait on a data type when all of its members can have derived the trait
	// e.g., Eq can't be derived on a struct containing only f32 s, since f32 is not Eq

// trait 'Clone' defines how to duplicate a value of type T
pub trait Clone: Sized {
	fn clone(&self) -> Self;

	fn clone_from(&mut self, source: &Self) { ... }
}

//
#[derive(Clone)]
struct Foo {
	x; i32,
}

#[derive(Clone)]
struct Bar {
	x; Foo,
}

//
pub trait Copy: Clone { }
// trait 'Copy' denotes that a type has 'cpy semantics' instead of 'move semantics'
// type must be able to be copied by copying bits (memcpy)
	// types that contain references cannot be Copy
// Marker trait: does not implement any methods, but defines behavior instead
// in general, if a type can be Copy, it should be Copy

//
pub trait Debug {
	fn fmt(&self, &mut Formatter) -> Result;
}
// Debug defines output for the {:?} formatting option
// generates debug output, not pretty printed
// generally speaking, you should always derive this trait

#[derive(Debug)]
struct Point {
	x: i32,
	y: i32,
}

let origin = Point { x: 0, y: 0 };
println!("The origin is: {?}", origin);
// The origin is: Point { x: 0, y: 0 }

// Default trait defines a default value for a type
pub trait Default: Sized {
	fn default() -> Self;
}

// Eq, PartialEq define equality via the == operator
pub trait PartialEq<Rhs: ?Sized = Self> {
	fn eq(&self, other; &Rhs) -> bool;

	fn ne(&self, other: &Rhs) -> bool { ... }
}

pub trait Eq: PartialEq<Self> {}
// PartialEq represents a partial equivalance relation
	// symmetric: if a == b then b == a
	// transitive: if a == b and b == c then a == c
// ne has a default implementation in terms of eq
// Eq represents a total equivalence relation
	// symmetric:if a == b then b == a
	// transitive: if a == b and b == c then a == c
	// reflexive: a == a
// Eq does not define any aditional methods
	// it is also a Marker trait

//
pub trait Hash {
	fn hash<H: Hasher>(&self, state: &mut H);

	fn hash_slice<H: Hasher>(data: &[Self], state: &mut H)
		where Self: Sized { ... }
}
// a hashable type
// the H type parameter is an abstract hash state used to compute the hash
// if you also implement Eq, there is an additional, important property:
	// k1 == k2 -> hash(k1) == hash(k2)

// PartialOrd: for values that can be compared for a sort-order
pub trait PartialOrd<Rhs: ?Sized = Self>: PartialEq<Rhs> {
	// ordering is one of Less, Equal, Greater
	fn partial_cmp(&self, other: &Rhs) -> Option<Ordering>;

	fn lt(&self, other: &Rhs) -> bool { ... }
	fn le(&self, other: &Rhs) -> bool { ... }
	fn gt(&self, other: &Rhs) -> bool { ... }
	fn ge(&self, other: &Rhs) -> bool { ... }
}
// Ord vs PartialOrd
// the comparison must satisfy, for all a, b and c
	// antisymmetry: if a < b then !(a > b), as well as a > b implying !(a < b)
	// transitivity: a < b and b < c implies a < c. the same must hold for both == and >
// lt, le, gt, ge have default implementations based on partial_cmp

pub trait Ord: Eq + PartialOrd(Self) {
	fn cmp(&self, other: &Self) -> Ordering;
}
// trait for types that form a total order
// an order is a total order if it is (for all a, b and c)
	// total and antisymmetric: exactly one of a < b, a == b or a > b is true
	// transitive, a < b and b < c implies a < c. The same must hold for both == and >
// when this trait is derived, it produces a lexicographic ordering

// associated types
// type definitions inside a trait block indicate associated generic types on the trait
// an implementor of the trait may specify what the associated types correspond to
trait Graph {
	type N;
	type E;

	fn edges(&self, &Self::N) -> Vec<Self::E>;
}

impl Graph for MyGraph {
	type N = MyNode;
	type E = MyEdge;

	fn edges(&self, n: &MyNode) -> Vec<MyEdge> { /* ... */ }
}

// trait scope
// trait scope rules for implementing traits
	// you need to use a trait in order to access its methods on types, even if you have access to the type
	// in order to write an impl, you need to own(i.e. have yourself defined) either the trait or the type

// bad practice
trait Foo {
	fn bar(&self) -> bool;
}

impl Foo for i32 {
	fn bar(&self) -> bool {
		true
	}
}

//
pub trait Display {
	fn fmt(&self, &mut Formatter) -> Result<(), Error>;
}

impl Display for Point {
	fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
		write!(f, "Point {}, {}", self.x, self.y)
	}
}

//
pub trait Drop {
	fn drop(&mut self);
}

// Sized indicates that a type has a constant size known at compile time
// ?Sized indicates that a type might be sized
// by default, all types are implicitly Sized, and ?Sized undoes this
	// types like [T] and str (no &) are ?Sized
	// e.g. Box<T> allows 'T: ?Sized'

// trait objects
//
trait Foo { fn bar(&self); }

impl Foo for String {
	fn bar(&self) { /*...*/ }
}

impl Foo for usize {
	fn bar(&self) { /*...*/ }
}

// call bar via static dispatch using any type with bounds 'T: Foo'
// when the code is compiled, the compiler will insert calls to specialized versions of bar
	// one function is generated for each implementator of the Foo trait
fn blah(x: T) where T: Foo {
	x.bar()
}

fn main() {
	let s = "Foo".to_string();
	let u = 12;

	blah(s);
	blah(u);
}

// dynamic dispatch through the use of trait objects
// a trait object is something like 'Box<Foo>' or '&Foo'
// the data behind the reference/box must implement the trait Foo
// the concrete type underlying the trait is erased; it can't be determined
trait Foo { /*...*/ }

impl Foo for char { /*...*/ }
impl Foo for i32 { /*...*/ }

fn use_foo(f: &Foo) {
	// No way to figure out if we got a 'char' or an 'i32'
	// or anything else
	match *f {
		// what type do we have ? I dunno...
		// error: mismatched types: expected 'Foo', found '_'
		198 => println!("CIS 198"),
		'c' => println!("See?"),
		_ => println!("Something else..."),
	}
}

use_foo(&'c');	// these coerce into '&Foo's
use_foo(&198i32);

// a trait is object-safe if:
	// It does not require that Self: Sized
	// Its methods must not use Self
	// Its methods must not have any type parameters
	// Its methods do not require that Self: Sized


// closures
let foo_v1 = |x: i32| { x * x };
let foo_v2 = |x: i32, y: i32| x * y;
let foo_v1 = |x: i32| {
	// very important arithmatic
	let y = x * 2;
	let z = 4 + y;
	x + y + z
};
let foo_v4 = |x: i32| if x == 0 { 0 } else { 1 };
// closure syntax
// specify arguments in ||, followed by the return expression
	// the return expression can be a series of expressions in {}

// closure vs function
	// let instead of fn
	// arguments in pipes
	// braces are optional

// closure: type inference
let square_v4 = |x: u32| { (x * x) as i32 };

let square_v4 = |x| -> i32 { x * x };
let square_v4 = |x|		   { x * x };
// unlike functions, we don't need to specify the return type or argument types of a closure
// having concrete function types for type inference and self-documentation. for closures, ease of use is more important

// closure environment
	// closure close over (contain) their environment
let magic_num = 5;
let magic_johnson = 32;
let plus_magic = |x: i32| x + magic_num;
// The closure plus_magic is able to reference magic_num even though it's not passed as an argument.
	// magic_num is in the "environment" of the closure.
	// magic_johnson is not borrowed!

//
let mut magic_num = 5;
let magic_johnson = 32;
let plus_magic = |x: i32| x + magic_num;

let more_magic = &mut magic_num; // Err!
println!("{}", magic_johnson); // Ok!

//
let mut magic_num = 5;
{
    let plus_magic = |x: i32| x + magic_num;
} // the borrow of magic_num ends here

let more_magic = &mut magic_num; // Ok!
println!("magic_num: {}", more_magic);

// move closures
// you can force a closure to take ownership of all environment variables by using the move keyword
	// taking ownership can mean taking a copy, not just moving
let mut magic_num = 5;
let own_the_magic = move |x: i32| x + magic_num;
let more_magic = &mut magic_num;

// move closures are necessary when the closure f needs to outlive the scope in which it was created.
	// e.g. when you pass f into a thread, or return f from a function.
	// move essentially disallows bringing references into the closure.
fn make_closure(x: i32) -> Box<Fn(i32) -> i32> {
    let f = move |y| x + y; // ^ more on this in 15 seconds
    Box::new(f)
}

let f = make_closure(2);
println!("{}", f(3));

// closure ownership
// Sometimes, a closure must take ownership of an environment variable to be valid. This happens automatically (without move):
	// If the value is moved into the return value.
let lottery_numbers = vec![11, 39, 51, 57, 75];
{
    let ticket = || { lottery_numbers };
}
// The braces do no good here.
println!("{:?}", lottery_numbers); // use of moved value
	// Or moved anywhere else
let numbers = vec![2, 5, 32768];
let alphabet_soup = || { numbers; vec!['a', 'b'] };
                      // ^ throw away unneeded ingredients
println!("{:?}", numbers); // use of moved value
// If the type is not Copy, the original variable is invalidated.

// Closures which own data and then move it can only be called once
	// move behavior is implicit because alphabet_soup must own numbers to move it.
let numbers = vec![2, 5, 32768];
let alphabet_soup = || { numbers; vec!['a', 'b'] };
                      // ^ throw away unneeded ingredients
alphabet_soup();
alphabet_soup(); // use of moved value

// Closures which own data but don't move it can be called multiple times
let numbers = vec![2, 5, 32768];
let alphabet_soup = move || { println!("{:?}", numbers) };
alphabet_soup();
alphabet_soup(); // Delicious soup

// The same closure can take some values by reference and others by moving ownership (or Copying values), determined by behavior.

// closure traits
// closure are actually based on a set of traits under the hood
	// Fn, FnMut, FnOnce - method calls are overloadable operators
pub trait Fn<Args> : FnMut<Args> {
    extern "rust-call"
      fn call(&self, args: Args) -> Self::Output;
}

pub trait FnMut<Args> : FnOnce<Args> {
    extern "rust-call"
      fn call_mut(&mut self, args: Args) -> Self::Output;
}

pub trait FnOnce<Args> {
    type Output;

    extern "rust-call"
      fn call_once(self, args: Args) -> Self::Output;
}
// Fn, FnMut, FnOnce differ in the way they take self:
	// Fn borrows self as &self
	// FnMut borrows self mutably as &mut self
	// FnOnce takes ownership of sel
// Fn is a superset of FnMut, which is a superset of FnOnce
// Functions also implement these traits
// "The || {} syntax for closures is sugar for these three traits. Rust will generate a struct for the environment, impl the appropriate trait, and then use it."

// closures as arguments
// passing closures works like function pointers

// self = Vec<A>
fn map<A, B, F>(self, f: F) -> Vec<B>
	where F: FnMut(A) -> B;
// map takes an argument f: F, where F is an FnMut trait object

// returning closures
// since closures are implicitly trait objects, they're unsized
fn i_need_some_closure() -> (Fn(i32) -> i32) {
    let local = 2;
    |x| x * local
}
// error: Fn(i32) -> i32 doesn't have a size known at compile-time

// fix: wrap the Fn in a layer of indirection and return a reference

fn i_need_some_closure_by_reference() -> &(Fn(i32) -> i32) {
    let local = 2;
    |x| x * local
}
// error: missing lifetime specifier

fn box_me_up_that_closure() -> Box<Fn(i32) -> i32> {
    let local = 2;
    Box::new(|x| x * local)
}
// error: closure may outlive the current function

fn box_up_your_closure_and_move_out() -> Box<Fn(i32) -> i32> {
    let local = 2;
    Box::new(move |x| x * local)
}
// ok





```


```rust
// 打印变量类型
fn print_type_of<T>(_: &T) {
    println!("{}", std::any::type_name::<T>())
}

print_type_of(&32.90);          // prints "f64"

Rust 的缩进风格使用 4 个空格，而不是 1 个制表符（tab）
Rust 是一种 预编译静态类型（ahead-of-time compiled）语言，这意味着你可以编译程序，并将可执行文件送给其他人，他们甚至不需要安装 Rust 就可以运行
Cargo帮助你编写真实世界中的 Rust 程序，用来管理你项目的方方面面，并让代码易于分享
cargo --version
cargo new hello_cargo
cd hello_cargo/
cargo build
cargo run
cargo check
cargo build --release

把 Cargo 当作习惯

示例1：
//将依赖的库引入当前作用域
use rand::Rng;
use std::cmp::Ordering;
use std::io;

fn main() {
	println!("Guess the number!");

	let secret_number = rand::thread_rng().gen_range(1..=100);

	loop {
		println!("Please input your guess.");

		// 在rust中变量默认是不可变的，一般给变量赋值后就不可修改
		// =号用来将变量绑定在=号右边的值上
		// 在变量明前使用mut可使一个变量可变，即引入可变变量
		// ::语法表示new是String类型的一个关联函数，关联函数是针对类型实现的
		let mut guess = String::new();

		// read_line 将用户在标准输入中键入的内容追加（不会覆盖其原有内容）到一个字符串中，因此它需要字符串作为参数
		// & 表示这个参数是一个 引用（reference），它允许多处代码访问同一处数据，而无需在内存中多次拷贝
		// read_line返回值为Result类型，Result是一种枚举类型，Result类型的作用是编码错误处理信息，Result实例有expect方法
	
		io::stdin()
			.read_line(&mut guess)
			.expect("Failed to read line");

		let guess: u32 = match guess.trim().parse() {
			Ok(num) => num,
			Err(_) => continue,
		};

		// {} 是预留在特定位置的占位符
		println!("You guessed: {guess}");

		match guess.cmp(&secret_number) {
			Ordering::Less => println!("Too small!"),
			Ordering::Greater => println!("Too big!"),
			Ordering::Equal => {
				println!("You win!");
				break;
			}
		}
	}
}

rust变量默认是不可改变的
// 常量
常量 (constants) 是绑定到一个名称的不允许改变的值,它总是不可变声明常量使用 const 关键字而不是 let，并且 必须 注明值的类型常量只能被设置为常量表达式，而不可以是其他任何只能在运行时计算出的值

// 数据类型
i8, i16, i32, i64, i128, isize
u8, u16, u32, u64, u128, usize
整型字面值
98_222, 0xff, 0o77, 0b1111_0000, b'A'

浮点数:
f32, f64

布尔型bool
true, false

字符类型char
Rust 的 char 类型的大小为四个字节 (four bytes)，并代表了一个 Unicode 标量值（Unicode Scalar Value）

复合类型
复合类型（Compound types）可以将多个值组合成一个类型。Rust 有两个原生的复合类型：元组（tuple）和数组（array）

元组是一个将多个其他类型的值组合进一个复合类型的主要方式。元组长度固定：一旦声明，其长度不会增大或缩小
我们使用包含在圆括号中的逗号分隔的值列表来创建一个元组。元组中的每一个位置都有一个类型，而且这些不同值的类型也不必是相同的。
let tup: (i32, f64, u8) = (500, 6.4, 1);

解构（destructuring）：将元组拆分成多个不同的变量
也可以使用点号（.）后跟值的索引来直接访问元组元素

不带任何值的元组有个特殊的名称，叫做 单元（unit） 元组。这种值以及对应的类型都写作 ()，表示空值或空的返回类型。如果表达式不返回任何其他值，则会隐式返回单元值。

数组类型
数组中的每个元素的类型必须相同，Rust 中的数组长度是固定的，将数组的值写成在方括号内，用逗号分隔
let a = [1, 2, 3, 4, 5];

数组并不如 vector 类型灵活。vector 类型是标准库提供的一个 允许 增长和缩小长度的类似数组的集合类型。当不确定是应该使用数组还是 vector 的时候，那么很可能应该使用 vector，然而，当你确定元素个数不会改变时，数组会更有用

数组的赋值：
let a: [i32; 5] = [1, 2, 3, 4, 5];
let a = [3; 5];

访问数组元素：下标访问

程序在索引操作中使用一个无效的值时导致 运行时 错误


// 函数
Rust 代码中的函数和变量名使用下划线命名法（snake case，直译为蛇形命名法）规范风格。在下划线命名法中，所有字母都是小写并使用下划线分隔单词
函数可以定义在 main 函数之后；也可以定义在之前。Rust 不关心函数定义于何处，只要定义了就行

参数：
当一个函数有多个参数时，使用逗号分隔
函数也可以被定义为拥有参数（parameter），参数是特殊变量，是函数签名的一部分。当函数拥有参数（形参）时，可以为这些参数提供具体的值（实参）
在函数签名中，必须声明每个参数的类型。这是一个 Rust 设计中经过慎重考虑的决定：要求在函数定义中提供类型标注，意味着编译器几乎从不需要你在代码的其他地方注明类型来指出你的意图

函数体由一系列语句组成，也可选择以表达式结尾，Rust 是一门基于表达式（expression-based）的语言
语句（statement）是执行一些操作但不返回值的指令。表达式（expression）计算并产生一个值。函数定义也是语句
let语句不返回值
表达式的结尾没有分号。如果在表达式的末尾加上分号，那么它就转换为语句，而语句不会返回值。

带有返回值的函数：
函数可以向调用它的代码返回值。我们并不对返回值命名，但要在箭头（->）后声明它的类型。在 Rust 中，函数的返回值等同于函数体最后一个表达式的值。使用 return 关键字和指定值，可以从函数中提前返回；但大部分函数隐式返回最后一个表达式。

注释//

// 控制流
if number % 4 == 0 {
	println!("number is divisible by 4");
} else if number % 3 == 0 {
	println!("number is divisible by 3");
} else if number % 2 == 0 {
	println!("number is divisible by 2");
} else {
	println!("number is not divisible by 4, 3, or 2");
}

if 是一个表达式
let number = if condition { 5 } else { 6 };

循环：
loop 关键字告诉 Rust 一遍又一遍地执行一段代码直到你明确要求停止

如果存在嵌套循环，break 和 continue 应用于此时最内层的循环。你可以选择在一个循环上指定一个循环标签（loop label），然后将标签与 break 或 continue 一起使用，使这些关键字应用于已标记的循环而不是最内层的循环

从循环返回
loop 的一个用例是重试可能会失败的操作，用于停止循环的 break 表达式添加你想要返回的值

while 条件循环
while index < 5 {

}
使用 for 遍历集合
for element in a {
	println!("the value is: {}", element);
}

for number in (1..4).rev() {
	println!("{}!", number);
}

// 所有权
所有权是 Rust 最为与众不同的特性，它让 Rust 无需垃圾回收器（garbage collector）即可保证内存安全
栈中的所有数据都必须占用已知且固定的大小。在编译时大小未知或大小可能变化的数据，要改为存储在堆上。堆是缺乏组织的
所有权的存在就是为了管理堆数据，跟踪哪部分代码正在使用堆上的哪些数据，最大限度地减少堆上的重复数据量，以及清理堆上不再使用的数据确保不会耗尽空间

有权规则:
1.Rust 中的每一个值都有一个被称为其 所有者（owner）的变量。
2.值在任一时刻有且只有一个所有者。
3.当所有者（变量）离开作用域，这个值将被丢弃。

变量从声明的那一刻开始直到当前作用域结束时都是有效的:
1.当 s 进入作用域 时，它就是有效的。
2.这一直持续到它 离开作用域 为止。

示例：String类型管理被分配到堆上的数据，所以能够存储在编译时未知大小的文本
可以使用 from 函数基于字符串字面量来创建 String
let s = String::from("hello");
双冒号（::）运算符允许我们将特定的 from 函数置于 String 类型的命名空间（namespace）下，而不需要使用类似 string_from 这样的名字
可以 修改此类字符串 ：
let mut s = String::from("hello");
s.push_str(", world!"); // push_str() 在字符串后追加字面值

对于 String 类型，为了支持一个可变，可增长的文本片段，需要在堆上分配一块在编译时未知大小的内存来存放内容。这意味着：
1.必须在运行时向内存分配器请求内存。    // 当调用 String::from 时，它的实现（implementation）请求其所需的内存
2.需要一个当我们处理完 String 时将内存返回给分配器的方法    // Rust 采取的策略：内存在拥有它的变量离开作用域后就被自动释放
{                                   // s 在这里无效, 它尚未声明
	let s = String::from("hello");  // 从此处起，s 开始有效

	// 使用 s
}                                  // 此作用域已结束，s 不再有效
当变量离开作用域，Rust 为我们调用一个特殊的函数。这个函数叫做 drop，Rust 在结尾的 } 处自动调用 drop

变量与数据交互的方式:
1.移动
let s1 = String::from("hello");
let s2 = s1;
s1:
三个字段(ptr, len, capacity位于栈上)， 且ptr指向堆上的地址"hello"
s2:
三个字段(ptr, len, capacity位于栈上)， 且ptr指向和s1的ptr指向相同的堆上的地址"hello"

拷贝指针、长度和容量而不拷贝数据可能听起来像浅拷贝，Rust 同时使第一个变量无效了，这个操作被称为 移动（move），而不是浅拷贝

2.克隆（当出现 clone 调用时，你知道一些特定的代码被执行而且这些代码可能相当消耗资源）
let s1 = String::from("hello");
let s2 = s1.clone();

println!("s1 = {}, s2 = {}", s1, s2);   // 堆上的数据确实被复制了

只在栈上的数据：拷贝
Rust 有一个叫做 Copy trait 的特殊标注，可以用在类似整型这样的存储在栈上的类型上，如果一个类型实现了 Copy trait，那么一个旧的变量在将其赋值给其他变量后仍然可用。
Rust 不允许自身或其任何部分实现了 Drop trait 的类型使用 Copy trait

哪些类型实现了 Copy trait:
作为一个通用的规则，任何一组简单标量值的组合都可以实现 Copy，任何不需要分配内存或某种形式资源的类型都可以实现 Copy

一些 Copy 的类型：
1.所有整数类型，比如 u32。
2.布尔类型，bool，它的值是 true 和 false。
3.所有浮点数类型，比如 f64。
4.字符类型，char。
5.元组，当且仅当其包含的类型也都实现 Copy 的时候。比如，(i32, i32) 实现了 Copy，但 (i32, String) 就没有

变量的所有权总是遵循相同的模式：将值赋给另一个变量时移动它。当持有堆中数据值的变量离开作用域时，其值将通过 drop 被清理掉，除非数据被移动为另一个变量所有。

所有权与函数
将值传递给函数在语义上与给变量赋值相似。向函数传递值可能会移动或者复制，就像赋值语句一样
fn main() {
	let s = String::from("hello");  // s 进入作用域

	takes_ownership(s);             // s 的值移动到函数里 ...
									// ... 所以到这里不再有效

	let x = 5;                      // x 进入作用域

	makes_copy(x);                  // x 应该移动函数里，
									// 但 i32 是 Copy 的，所以在后面可继续使用 x

} // 这里, x 先移出了作用域，然后是 s。但因为 s 的值已被移走，
	// 所以不会有特殊操作

fn takes_ownership(some_string: String) { // some_string 进入作用域
	println!("{}", some_string);
} // 这里，some_string 移出作用域并调用 `drop` 方法。占用的内存被释放

fn makes_copy(some_integer: i32) { // some_integer 进入作用域
	println!("{}", some_integer);
} // 这里，some_integer 移出作用域。不会有特殊操作

返回值与作用域
返回值也可以转移所有权
fn main() {
	let s1 = gives_ownership();         // gives_ownership 将返回值
										// 移给 s1

	let s2 = String::from("hello");     // s2 进入作用域

	let s3 = takes_and_gives_back(s2);  // s2 被移动到
										// takes_and_gives_back 中,
										// 它也将返回值移给 s3
} // 这里, s3 移出作用域并被丢弃。s2 也移出作用域，但已被移走，
	// 所以什么也不会发生。s1 移出作用域并被丢弃

fn gives_ownership() -> String {           // gives_ownership 将返回值移动给
											// 调用它的函数

	let some_string = String::from("yours"); // some_string 进入作用域

	some_string                              // 返回 some_string 并移出给调用的函数
}

// takes_and_gives_back 将传入字符串并返回该值
fn takes_and_gives_back(a_string: String) -> String { // a_string 进入作用域

	a_string  // 返回 a_string 并移出给调用的函数
}

转移返回值的所有权：使用元组来返回多个值
fn main() {
	let s1 = String::from("hello");

	let (s2, len) = calculate_length(s1);

	println!("The length of '{}' is {}.", s2, len);
}

fn calculate_length(s: String) -> (String, usize) {
	let length = s.len(); // len() 返回字符串的长度

	(s, length)
}

// 引用与借用
引用允许你使用值但不获取其所有权
引用（reference）像一个指针，因为它是一个地址，我们可以由此访问储存于该地址的属于其他变量的数据。 与指针不同，引用确保指向某个特定类型的有效值

引用语法让我们创建一个指向值的引用，但是并不拥有它。因为并不拥有这个值，所以当引用停止使用时，它所指向的值也不会被丢弃。
同理，函数签名使用 & 来表明参数的类型是一个引用
创建一个引用的行为称为 借用（borrowing）
如果尝试修改借用的变量，正如变量默认是不可变的，引用也一样。（默认）不允许修改引用的值
fn main() {
	let s1 = String::from("hello");

	let len = calculate_length(&s1);

	println!("The length of '{}' is {}.", s1, len);
}

fn calculate_length(s: &String) -> usize {
	s.len()
}

可变引用：
可变引用有一个很大的限制：如果你有一个对该变量的可变引用，你就不能再创建对该变量的引用，这个限制的好处是 Rust 可以在编译时就避免数据竞争
注意一个引用的作用域从声明的地方开始一直持续到最后一次使用为止。
然而，多个不可变引用是可以的
fn main() {
	let mut s = String::from("hello");

	change(&mut s);
}

fn change(some_string: &mut String) {
	some_string.push_str(", world");
}

数据竞争（data race）类似于竞态条件，它可由这三个行为造成：
1.两个或更多指针同时访问同一数据。
2.至少有一个指针被用来写入数据。
3.没有同步数据访问的机制。

我们也不能在拥有不可变引用的同时拥有可变引用


悬垂引用
悬垂指针: 其指向的内存可能已经被分配给其它持有者
在 Rust 中编译器确保引用永远也不会变成悬垂状态：当你拥有一些数据的引用，编译器确保数据不会在其引用之前离开作用域

让我们尝试创建一个悬垂引用，Rust 会通过一个编译时错误来避免：
fn main() {
	let reference_to_nothing = dangle();
}

fn dangle() -> &String {
	let s = String::from("hello");

	&s
}
解决方法：
fn no_dangle() -> String {
	let s = String::from("hello");

	s
}

引用的规则:
1.在任意给定时间，要么 只能有一个可变引用，要么 只能有多个不可变引用。
2.引用必须总是有效的


Slice 类型
slice 允许你引用集合中一段连续的元素序列，而不用引用整个集合。slice 是一类引用，所以它没有所有权

字符串 slice
字符串 slice（string slice）是 String 中一部分值的引用
let s = String::from("hello world");

let hello = &s[0..5];
let world = &s[6..11];
使用一个由中括号中的 [starting_index..ending_index] 指定的 range 创建一个 slice，其中 starting_index 是 slice 的第一个位置，ending_index 则是 slice 最后一个位置的后一个值，在其内部，slice 的数据结构存储了 slice 的开始位置和长度，长度对应于 ending_index 减去 starting_index 的值
对于 Rust 的 .. range 语法，如果想要从索引 0 开始，可以不写两个点号之前的值，也可以同时舍弃这两个值来获取整个字符串的 slice
字符串 slice range 的索引必须位于有效的 UTF-8 字符边界内
示例：
fn first_word(s: &String) -> &str {
	let bytes = s.as_bytes();

	for (i, &item) in bytes.iter().enumerate() {
		if item == b' ' {
			return &s[0..i];
		}
	}

	&s[..]
}

字符串字面值就是 slice
let s = "Hello, world!";
这里 s 的类型是 &str：它是一个指向二进制程序特定位置的 slice。这也就是为什么字符串字面值是不可变的；&str 是一个不可变引用

字符串 slice 作为参数
fn first_word(s: &String) -> &str {
或者
fn first_word(s: &str) -> &str {

其他类型的 slice
let a = [1, 2, 3, 4, 5];
let slice = &a[1..3];
assert_eq!(slice, &[2, 3]);


// 结构体
结构体比元组更灵活：不需要依赖顺序来指定或访问实例中的值
struct User {
	active: bool,
	username: String,
	email: String,
	sign_in_count: u64,
}

let user1 = User {
	active: true,
	username: String::from("someusername123"),
	email: String::from("someone@example.com"),
	sign_in_count: 1,
};

注意整个实例必须是可变的；Rust 并不允许只将某个字段标记为可变

字段初始化简写语法(参数名与字段名完全相同的情况下)：
fn build_user(email: String, username: String) -> User {
	User {
		active: true,
		username,
		email,
		sign_in_count: 1,
	}
}

使用结构体更新语法从其他实例创建实例
let user2 = User {
	email: String::from("another@example.com"),
	..user1
};
.. 语法指定了剩余未显式设置值的字段应有与给定实例对应字段相同的值，.. 语法必须放在最后以指定其余的字段应从实例的相应字段中获取其值

使用没有命名字段的元组结构体来创建不同的类型：
元组结构体有着结构体名称提供的含义，但没有具体的字段名，只有字段的类型。当你想给整个元组取一个名字，并使元组成为与其他元组不同的类型时，元组结构体是很有用的
struct Color(i32, i32, i32);
struct Point(i32, i32, i32);

fn main() {
	let black = Color(0, 0, 0);
	let origin = Point(0, 0, 0);
}
注意 black 和 origin 值的类型不同，因为它们是不同的元组结构体的实例

元组结构体实例类似于元组，你可以将它们解构为单独的部分，也可以使用 . 后跟索引来访问单独的值，等等

没有任何字段的类单元结构体，
一个没有任何字段的结构体称为类单元结构体，类单元结构体常常在你想要在某个类型上实现 trait 但不需要在类型中存储数据的时候发挥作用
struct AlwaysEqual;

fn main() {
	let subject = AlwaysEqual;
}

结构体示例程序
struct Rectangle {
	width: u32,
	height: u32,
}

fn main() {
	let rect1 = Rectangle {
		width: 30,
		height: 50,
	};

	println!(
		"The area of the rectangle is {} square pixels.",
		area(&rect1)
	);
}

fn area(rectangle: &Rectangle) -> u32 {
	rectangle.width * rectangle.height
}

注意，访问对结构体的引用的字段不会移动字段的所有权，这就是为什么你经常看到对结构体的引用

println! 宏能处理很多类型的格式，不过，{} 默认告诉 println! 使用被称为 Display 的格式：意在提供给直接终端用户查看的输出。目前为止见过的基本类型都默认实现了 Display，因为它就是向用户展示 1 或其他任何基本类型的唯一方式
println!("rect1 is {}", rect1); 编译时会有错误，因为Rectangle没有实现Display

在 {} 中加入 :? 指示符告诉 println! 我们想要使用叫做 Debug 的输出格式。Debug 是一个 trait，它允许我们以一种对开发者有帮助的方式打印结构体，以便当我们调试代码时能看到它的值
println!("rect1 is {:?}", rect1); 编译时会有错误，因为我们没有添加外部属性#[derive(Debug)]

#[derive(Debug)]
println!("rect1 is {:?}", rect1);
output: rect1 is Rectangle { width: 30, height: 50 }

更易读一点的输出，为此可以使用 {:// ?} 替换 println! 字符串中的 {:?}
rect1 is Rectangle {
	width: 30,
	height: 50,
}

另一种使用 Debug 格式打印数值的方法是使用 dbg! 宏。dbg! 宏接收一个表达式的所有权（与 println! 宏相反，后
者接收的是引用），打印出代码中调用 dbg! 宏时所在的文件和行号，以及该表达式的结果值，并返回该值的所有权
#[derive(Debug)]
struct Rectangle {
	width: u32,
	height: u32,
}

fn main() {
	let scale = 2;
	let rect1 = Rectangle {
		width: dbg!(30 * scale),
		height: 50,
	};

	dbg!(&rect1);
}
可以把 dbg! 放在表达式 30 * scale 周围，因为 dbg! 返回表达式的值的所有权，所以 width 字段将获得相同的值
，就像我们在那里没有 dbg! 调用一样。我们不希望 dbg! 拥有 rect1 的所有权，所以我们在下一次调用 dbg! 时
传递一个引用
output:
[src/main.rs:10] 30 * scale = 60
[src/main.rs:14] &rect1 = Rectangle {
	width: 60,
	height: 50,
}

除了 Debug trait，Rust 还为我们提供了很多可以通过 derive 属性来使用的 trait，它们可以为我们的自定义类型
增加实用的行为

// 方法语法
方法与函数是不同的，因为它们在结构体的上下文中被定义（或者是枚举或 trait 对象的上下文），并且它们第一个参数总是 self，它代表调用该方法的结构体实例

定义方法:
#[derive(Debug)]
struct Rectangle {
	width: u32,
	height: u32,
}

impl Rectangle {
	fn area(&self) -> u32 {
		self.width * self.height
	}
}

fn main() {
	let rect1 = Rectangle {
		width: 30,
		height: 50,
	};

	println!(
		"The area of the rectangle is {} square pixels.",
		rect1.area()
	);
}

impl块 使函数定义于 Rectangle 的上下文中
在 area 的签名中，使用 &self 来替代 rectangle: &Rectangle，&self 实际上是 self: &Self 的缩写。
在一个 impl 块中，Self 类型是 impl 块的类型的别名。方法的第一个参数必须有一个名为 self 的Self 类型的参数，所以 Rust 让你在第一个参数位置上只用 self 这个名字来缩写

方法的名称与结构中的字段相同
impl Rectangle {
	fn width(&self) -> bool {
		self.width > 0
	}
}


Rust 有一个叫 自动引用和解引用（automatic referencing and dereferencing）的功能。方法调用是 Rust 中少数几个拥有这种行为的地方。
当使用 object.something() 调用方法时，Rust 会自动为 object 添加 &、&mut 或 * 以便使 object 与方法签名匹配
等价的代码：
p1.distance(&p2);
(&p1).distance(&p2);

带有更多参数的方法
impl Rectangle {
	fn area(&self) -> u32 {
		self.width * self.height
	}

	fn can_hold(&self, other: &Rectangle) -> bool {
		self.width > other.width && self.height > other.height
	}
}

关联函数
所有在 impl 块中定义的函数被称为关联函数（associated function），因为它们与 impl 后面命名的类型相关
我们可以定义不以 self 为第一参数的关联函数（因此不是方法），因为它们并不作用于一个结构体的实例。例如
String::from 函数，它是在 String 类型上定义的
关联函数经常被用作返回一个结构体新实例的构造函数。
例如：
impl Rectangle {
	fn square(size: u32) -> Rectangle {
		Rectangle {
			width: size,
			height: size,
		}
	}
}
使用结构体名和 :: 语法来调用这个关联函数：比如 let sq = Rectangle::square(3);。这个方法位于结构体的命名
空间中：:: 语法用于关联函数和模块创建的命名空间

多个 impl 块
每个结构体都允许拥有多个 impl 块

结构体让你可以创建出在你的领域中有意义的自定义类型。通过结构体，我们可以将相关联的数据片段联系起来并命名
它们，这样可以使得代码更加清晰
在 impl 块中，你可以定义与你的类型相关联的函数，而方法是一种相关联的函数，让你指定结构体的实例所具有的行为

枚举和模式匹配
枚举允许你通过列举可能的 成员（variants） 来定义一个类型

定义枚举
enum IpAddrKind {
	V4,
	V6,
}

枚举值：
let four = IpAddrKind::V4;
let six = IpAddrKind::V6;


可以直接将数据附加到枚举的每个成员上，这样就不需要一个额外的结构体
enum IpAddr {
	V4(String),
	V6(String),
}

let home = IpAddr::V4(String::from("127.0.0.1"));
let loopback = IpAddr::V6(String::from("::1"));

或者
enum IpAddr {
	V4(u8, u8, u8, u8),
	V6(String),
}

let home = IpAddr::V4(127, 0, 0, 1);
let loopback = IpAddr::V6(String::from("::1"));

可以将任意类型的数据放入枚举成员中：例如字符串、数字类型或者结构体。甚至可以包含另一个枚举！另外，标准库中的类型通常并不比你设想出来的要复杂多少
enum Message {
	Quit,                       // 没有关联任何数据
	Move { x: i32, y: i32 },    // 包含一个匿名结构体
	Write(String),              // 包含单独一个 String
	ChangeColor(i32, i32, i32), // 包含三个 i32
}

因为枚举是单独一个类型，因此可以轻易的定义一个能够处理这些不同类型的结构体的函数

枚举和结构体还有另一个相似点：就像可以使用 impl 来为结构体定义方法那样，也可以在枚举上定义方法
示例：
impl Message {
	fn call(&self) {
		// 在这里定义方法体
	}
}

let m = Message::Write(String::from("hello"));
m.call();


标准库中的实用枚举：Option
Option 类型应用广泛是因为它编码了一个非常普遍的场景，即一个值要么有值要么没值
空值尝试表达的概念是有意义的：空值是一个因为某种原因目前无效或缺失的值
Rust 并没有空值，不过它确实拥有一个可以编码存在或不存在概念的枚举。这个枚举是 Option<T>，而且它定义于标准库中:
enum Option<T> {
	Some(T),
	None,
}

Option<T> 枚举是如此有用以至于不需要将其显式引入作用域，它的成员也是如此，可以不需要 Option:: 前缀来直接使用 Some 和 None
<T> 语法是一个泛型类型参数，意味着 Option 枚举的 Some 成员可以包含任意类型的数据
例如：
let some_number = Some(5);
let some_string = Some("a string");
let absent_number: Option<i32> = None;
在对 Option<T> 进行 T 的运算之前必须将其转换为 T。通常这能帮助我们捕获到空值最常见的问题之一：假设某值不为空但实际上为空的情况
为了使用 Option<T> 值，需要编写处理每个成员的代码。你想要一些代码只当拥有 Some(T) 值时运行，允许这些代码使用其中的 T。也希望一些代码在值为 None 时运行，这些代码并没有一个可用的 T 值。
match 表达式就是这么一个处理枚举的控制流结构：它会根据枚举的成员运行不同的代码，这些代码可以使用匹配到的值中的数据。

match 控制流运算符
match是一种控制流运算符，它允许我们将一个值与一系列的模式相比较，并根据相匹配的模式执行相应代码。模式可由字面量、变量、通配符和许多其他内容构成；

enum Coin {
	Penny,
	Nickel,
	Dime,
	Quarter,
}

fn value_in_cents(coin: Coin) -> u8 {
	match coin {
		Coin::Penny => {
			println!("Lucky penny!");
			1
		}
		Coin::Nickel => 5,
		Coin::Dime => 10,
		Coin::Quarter => 25,
	}
}

match 表达式 {          // 表达式返回值可以是任意类型
	模式1   => 代码1,   // 如果模式匹配则对应的代码会被执行，否则继续执行下一个分支
	模式2   => 代码2,
	...
	模式n   => 代码n,
}

绑定值的模式：
匹配分支的另一个有用的功能是可以绑定匹配的模式的部分值。这也就是如何从枚举成员中提取值的
示例：
#[derive(Debug)] // 这样可以立刻看到州的名称
enum UsState {
	Alabama,
	Alaska,
	// --snip--
}

enum Coin {
	Penny,
	Nickel,
	Dime,
	Quarter(UsState),
}

fn value_in_cents(coin: Coin) -> u8 {
	match coin {
		Coin::Penny => 1,
		Coin::Nickel => 5,
		Coin::Dime => 10,
		Coin::Quarter(state) => {
			println!("State quarter from {:?}!", state);
			25
		}
	}
}
对于value_in_cents(Coin::Quarter(UsState::Alaska))，当将值与每个分支相比较时，没有分支会匹配，直到遇到 Coin::Quarter(state)，这时，state 绑定的将会是值 UsState::Alaska。接着就可以在 println! 表达式中使用这个绑定了，像这样就可以获取 Coin 枚举的 Quarter 成员中内部的州的值


匹配 Option<T>
fn main() {
	fn plus_one(x: Option<i32>) -> Option<i32> {
		match x {
			None => None,
			Some(i) => Some(i + 1),
		}
	}

	let five = Some(5);
	let six = plus_one(five);
	let none = plus_one(None);
}

匹配 Some(T)

Rust 代码中看到很多这样的模式：match 一个枚举，绑定其中的值到一个变量，接着根据其值执行代码

匹配是穷尽的
fn plus_one(x: Option<i32>) -> Option<i32> {
	match x {
		Some(i) => Some(i + 1),
	}
}
会出错
Rust 知道我们没有覆盖所有可能的情况甚至知道哪些模式被忘记了！Rust 中的匹配是穷举式的（exhaustive）：必须穷举到最后的可能性来使代码有效。


通配模式和 _ 占位符
希望对一些特定的值采取特殊操作，而对其他的值采取默认操作
let dice_roll = 9;
match dice_roll {
	3 => add_fancy_hat(),
	7 => remove_fancy_hat(),
	other => move_player(other),
}

fn add_fancy_hat() {}
fn remove_fancy_hat() {}
fn move_player(num_spaces: u8) {}
必须将通配分支放在最后，因为模式是按顺序匹配的

Rust 还提供了一个模式，当我们不想使用通配模式获取的值时，请使用 _ ，这是一个特殊的模式，可以匹配任意值而不绑定到该值。
例如：
让我们改变游戏规则，当你掷出的值不是 3 或 7 的时候，你必须再次掷出
let dice_roll = 9;
match dice_roll {
	3 => add_fancy_hat(),
	7 => remove_fancy_hat(),
	_ => reroll(),
}

fn add_fancy_hat() {}
fn remove_fancy_hat() {}
fn reroll() {}

再次改变游戏规则，如果你掷出 3 或 7 以外的值，你的回合将无事发生
let dice_roll = 9;
match dice_roll {
	3 => add_fancy_hat(),
	7 => remove_fancy_hat(),
	_ => (),
}

fn add_fancy_hat() {}
fn remove_fancy_hat() {}

//  if let 简单控制流
if let 获取通过等号分隔的一个模式和一个表达式。它的工作方式与 match 相同，这里的表达式对应 match 而模式则对应第一个分支

if let Some(3) = some_u8_value {
	println!("three");
}
等同于
let some_u8_value = Some(0u8);
match some_u8_value {
	Some(3) => println!("three"),
	_ => (),
}

可以认为 if let 是 match 的一个语法糖，它当值匹配某一模式时执行代码而忽略所有其他值

let mut count = 0;
match coin {
	Coin::Quarter(state) => println!("State quarter from {:?}!", state),
	_ => count += 1,
}
等同于
let mut count = 0;
if let Coin::Quarter(state) = coin {
	println!("State quarter from {:?}!", state);
} else {
	count += 1;
}


// 模块系统（the module system）
包（Packages）： Cargo 的一个功能，它允许你构建、测试和分享 crate。
Crates ：一个模块的树形结构，它形成了库或二进制项目。
模块（Modules）和 use： 允许你控制作用域和路径的私有性。
路径（path）：一个命名例如结构体、函数或模块等项的方式

包和 crate
crate 是一个二进制项或者库
crate root 是一个源文件，Rust 编译器以它为起始点，并构成你的 crate 的根模块
包（package）是提供一系列功能的一个或者多个 crate。一个包会包含有一个 Cargo.toml 文件，阐述如何去构建这些 crate

包中所包含的内容的规则：
一个包中至多 只能 包含一个库 crate（library crate）
包中可以包含任意多个二进制 crate（binary crate）
包中至少包含一个 crate，无论是库的还是二进制的

Cargo 遵循的一个约定：
src/main.rs 就是一个与包同名的二进制 crate 的 crate 根
Cargo 知道如果包目录中包含 src/lib.rs，则包带有与其同名的库 crate，且 src/lib.rs 是 crate 根
crate 根文件将由 Cargo 传递给 rustc 来实际构建库或者二进制项目

// 模块
模块 让我们可以将一个 crate 中的代码进行分组，以提高可读性与重用性。模块还可以控制项的 私有性，即项是可以
被外部代码使用的（public），还是作为一个内部实现的内容，不能被外部代码使用（private）

用关键字 mod 定义一个模块，指定模块的名字，并用大括号包围模块的主体。我们可以在模块中包含其他模块
模块中也可以包含其他项，比如结构体、枚举、常量、trait，或者包含函数
mod front_of_house {
	mod hosting {
		fn add_to_waitlist() {}

		fn seat_at_table() {}
	}

	mod serving {
		fn take_order() {}

		fn serve_order() {}

		fn take_payment() {}
	}
}

通过使用模块，我们可以把相关的定义组织起来，并通过模块命名来解释为什么它们之间有相关性。使用这部分代码的
开发者可以更方便的循着这种分组找到自己需要的定义，而不需要通览所有。编写这部分代码的开发者通过分组知道该
把新功能放在哪里以便继续让程序保持组织性

上述示例所对应的模块树
crate
└── front_of_house
	├── hosting
	│   ├── add_to_waitlist
	│   └── seat_at_table
	└── serving
		├── take_order
		├── serve_order
		└── take_payment

路径：用来在模块树中找到一个项的位置
mod front_of_house {
	mod hosting {
		fn add_to_waitlist() {}
	}
}

pub fn eat_at_restaurant() {
	// 绝对路径
	crate::front_of_house::hosting::add_to_waitlist();

	// 相对路径
	front_of_house::hosting::add_to_waitlist();
}

倾向于使用绝对路径，因为把代码定义和项调用各自独立地移动是更常见的

上述代码会有编译错误，因为在 Rust 中，默认所有项（函数、方法、结构体、枚举、模块和常量）对父模块都是私有的

父模块中的项不能使用子模块中的私有项，但是子模块中的项可以使用它们父模块中的项。这是因为子模块封装并隐藏了它们的实现详情，但是子模块可以看到它们定义的上下文
Rust提供了通过使用 pub 关键字来创建公共项，使子模块的内部部分暴露给上级模块

使用 pub 关键字暴露路径:
mod front_of_house {
	pub mod hosting {
		pub fn add_to_waitlist() {}
	}
}

pub fn eat_at_restaurant() {
	// 绝对路径
	crate::front_of_house::hosting::add_to_waitlist();

	// 相对路径
	front_of_house::hosting::add_to_waitlist();
}
虽然 front_of_house 模块不是公有的，不过因为 eat_at_restaurant 函数与 front_of_house 定义于同一模块中（即，eat_at_restaurant 和 front_of_house 是兄弟），我们可以从 eat_at_restaurant 中引用 front_of_house


super 开始的相对路径:
通过在路径的开头使用 super ，从父模块开始构建相对路径，而不是从当前模块或者 crate 根开始，类似以 .. 语法开始一个文件系统路径
使用 super 允许我们引用父模块中的已知项，这使得重新组织模块树变得更容易 —— 当模块与父模块关联的很紧密，但某天父模块可能要移动到模块树的其它位置
fn deliver_order() {}

mod back_of_house {
	fn fix_incorrect_order() {
		cook_order();
		super::deliver_order();
	}

	fn cook_order() {}
}

创建公有的结构体和枚举
关于在结构体和枚举上使用 pub，如果我们在一个结构体定义的前面使用了 pub ，这个结构体会变成公有的，但是这个结构体的字段仍然是私有的。我们可以根据情况决定每个字段是否公有


使用 use 关键字将名称引入作用域
使用 use 关键字将路径一次性引入作用域，然后调用该路径中的项，就如同它们是本地项一样
mod front_of_house {
	pub mod hosting {
		pub fn add_to_waitlist() {}
	}
}

use crate::front_of_house::hosting;
// use front_of_house::hosting;     // 相对路径

pub fn eat_at_restaurant() {
	hosting::add_to_waitlist();
}


创建惯用的 use 路径
将函数引入作用域的习惯用法：
use crate::front_of_house::hosting;

pub fn eat_at_restaurant() {
	hosting::add_to_waitlist();
}
而不是
use crate::front_of_house::hosting::add_to_waitlist;

pub fn eat_at_restaurant() {
	add_to_waitlist();
}

使用 use 引入结构体、枚举和其他项时，习惯是指定它们的完整路径
例如：
use std::collections::HashMap;

fn main() {
	let mut map = HashMap::new();
	map.insert(1, 2);
}


使用 as 关键字提供新的名称
use std::fmt::Result;
use std::io::Result as IoResult;

使用 pub use 重导出名称
当使用 use 关键字将名称导入作用域时，在新作用域中可用的名称是私有的
如果为了让调用你编写的代码的代码能够像在自己的作用域内引用这些类型，可以结合 pub 和 use。这个技术被称为 “重导出（re-exporting）”，因为这样做将项引入作用域并同时使其可供其他代码引入自己的作用域
mod front_of_house {
	pub mod hosting {
		pub fn add_to_waitlist() {}
	}
}

pub use crate::front_of_house::hosting;

pub fn eat_at_restaurant() {
	hosting::add_to_waitlist();
}
通过 pub use，现在可以通过新路径 hosting::add_to_waitlist 来调用 add_to_waitlist 函数。
如果没有指定 pub use，eat_at_restaurant 函数可以在其作用域中调用 hosting::add_to_waitlist，但外部代码则不允许使用这个新路径
当你的代码的内部结构与调用你的代码的开发者的思考领域不同时，重导出会很有用


使用外部包
[dependencies]
rand = "0.8.3"
在 Cargo.toml 中加入 rand 依赖告诉了 Cargo 要从 crates.io 下载 rand 和其依赖，并使其可在项目代码中使用。
use rand::Rng;

fn main() {
	let secret_number = rand::thread_rng().gen_range(1..101);
}
为了将 rand 定义引入项目包的作用域，我们加入一行 use 起始的包名，它以 rand 包名开头并列出了需要引入作用域的项


嵌套路径来消除大量的 use 行
当需要引入很多定义于相同包或相同模块的项时，为每一项单独列出一行会占用源码很大的空间。
可以使用嵌套路径将相同的项在一行中引入作用域。这么做需要指定路径的相同部分，接着是两个冒号，接着是大括号中的各自不同的路径部分
use std::cmp::Ordering;
use std::io;
// ---snip---
重写为
use std::{cmp::Ordering, io};
// ---snip---


use std::io;
use std::io::Write;
重写为
use std::io::{self, Write};

通过 glob 运算符将所有的公有定义引入作用域
如果希望将一个路径下 所有 公有项引入作用域，可以指定路径后跟 glob 运算符 *：
use std::collections::*;
这个 use 语句将 std::collections 中定义的所有公有项引入当前作用域
glob 运算符经常用于测试模块 tests 中，这时会将所有内容引入作用域；

将模块分割进不同文件
将一个文件中定义多个模块移动到单独的文件中，使代码更容易维护和阅读
例如：
mod front_of_house {
	pub mod hosting {
		pub fn add_to_waitlist() {}
	}
}

pub use crate::front_of_house::hosting;

pub fn eat_at_restaurant() {
	hosting::add_to_waitlist();
}
重构为:
src/lib.rs:
mod front_of_house;

pub use crate::front_of_house::hosting;

pub fn eat_at_restaurant() {
	hosting::add_to_waitlist();
}
声明 front_of_house 模块，其内容将位于 src/front_of_house.rs

src/front_of_house.rs:
pub mod hosting {
	pub fn add_to_waitlist() {}
}
在 src/front_of_house.rs 中定义 front_of_house 模块

src/front_of_house.rs:
pub mod hosting;

src/front_of_house/hosting.rs:
pub fn add_to_waitlist() {}

模块树依然保持相同，eat_at_restaurant 中的函数调用也无需修改继续保持有效，即便其定义存在于不同的文件中。这个技巧让你可以在模块代码增长时，将它们移动到新文件中
src/lib.rs 中的 pub use crate::front_of_house::hosting 语句是没有改变的，在文件作为 crate 的一部分而编译时，use 不会有任何影响
mod 关键字声明了模块，Rust 会在与模块同名的文件中查找模块的代码

常见集合
Rust 标准库中包含一系列被称为 集合（collections）的非常有用的数据结构
集合指向的数据是储存在堆上的，这意味着数据的数量不必在编译时就已知，并且还可以随着程序的运行增长或缩小
常用集合：
vector 允许我们一个挨着一个地储存一系列数量可变的值
字符串（string）是字符的集合。
哈希 map（hash map）允许我们将值与一个特定的键（key）相关联。这是一个叫做 map 的更通用的数据结构的特定实现。

vector
vector 允许我们在一个单独的数据结构中储存多个值，所有值在内存中彼此相邻排列。vector 只能储存相同类型的值
vector 是用泛型实现的

新建 vector
let v: Vec<i32> = Vec::new();
let v = vec![1, 2, 3];

更新 vector
let mut v = Vec::new();

v.push(5);
v.push(6);
v.push(7);
v.push(8);

丢弃 vector 时也会丢弃其所有元素

读取 vector 的元素(索引与罚或者get方法)
let v = vec![1, 2, 3, 4, 5];

let does_not_exist = &v[100];       // 当引用一个不存在的元素时 Rust 会造成 panic
let does_not_exist = v.get(100);    // 不会 panic 而是返回 None


遍历 vector 中的元素
let v = vec![100, 32, 57];
for i in &v {
	println!("{}", i);
}

let mut v = vec![100, 32, 57];
for i in &mut v {
	*i += 50;
}


使用枚举来储存多种类型
当需要在 vector 中储存不同类型值时，我们可以定义并使用一个枚举
enum SpreadsheetCell {
	Int(i32),
	Float(f64),
	Text(String),
}

let row = vec![
	SpreadsheetCell::Int(3),
	SpreadsheetCell::Text(String::from("blue")),
	SpreadsheetCell::Float(10.12),
];

Rust 的核心语言中只有一种字符串类型：str
字符串 slice 是一些储存在别处的 UTF-8 编码字符串数据的引用
String 的类型是由标准库提供的，而没有写进核心语言部分，它是可增长的、可变的、有所有权的、UTF-8 编码的字符串类型

新建字符串
let mut s = String::new();
let data = "initial contents";
let s = data.to_string();
// 该方法也可直接用于字符串字面量：
let s = "initial contents".to_string();
let s = String::from("initial contents");


更新字符串
let mut s = String::from("foo");
s.push_str("bar");

let mut s1 = String::from("foo");
let s2 = "bar";
s1.push_str(s2);
println!("s2 is {}", s2);

let s1 = String::from("Hello, ");
let s2 = String::from("world!");
let s3 = s1 + &s2; // 注意 s1 被移动了，不能继续使用
s1 在相加后不再有效的原因，和使用 s2 的引用的原因，与使用 + 运算符时调用的函数签名有关
+ 运算符使用了 add 函数，这个函数签名看起来像这样
fn add(self, s: &str) -> String {
之所以能够在 add 调用中使用 &s2 是因为 &String 可以被 强转（coerced）成 &str
签名中 add 获取了 self 的所有权，因为 self 没有 使用 &，意味着示 s1 的所有权将被移动到 add 调用中，之后就不再有效

let s1 = String::from("tic");
let s2 = String::from("tac");
let s3 = String::from("toe");

let s = format!("{}-{}-{}", s1, s2, s3);
format! 与 println! 的工作原理相同，不过不同于将输出打印到屏幕上，它返回一个带有结果内容的 String

索引字符串
Rust 的字符串不支持索引
String 是一个 Vec<u8> 的封装

字符串 slice
索引字符串通常是一个坏点子，因为字符串索引应该返回的类型是不明确的：字节值、字符、字形簇或者字符串 slice
如果你真的希望使用索引创建字符串 slice 时，可以使用 [] 和一个 range 来创建含特定字节的字符串 slice
let hello = "Здравствуйте";
let s = &hello[0..4];

遍历字符串的方法
for c in "नमस्ते".chars() {
	println!("{}", c);
}

哈希 map
HashMap<K, V> 类型储存了一个键类型 K 对应一个值类型 V 的映射。它通过一个 哈希函数（hashing function）来实现映射，决定如何将键和值放入内存中
哈希 map 可以用于需要任何类型作为键来寻找数据的情况，而不是像 vector 那样通过索引

新建一个哈希 map
// 1
use std::collections::HashMap;

let mut scores = HashMap::new();

scores.insert(String::from("Blue"), 10);
scores.insert(String::from("Yellow"), 50);

// 2
use std::collections::HashMap;

let teams  = vec![String::from("Blue"), String::from("Yellow")];
let initial_scores = vec![10, 50];

let scores: HashMap<_, _> = teams.iter().zip(initial_scores.iter()).collect();

哈希 map 和所有权
对于像 i32 这样的实现了 Copy trait 的类型，其值可以拷贝进哈希 map。对于像 String 这样拥有所有权的值，其值将被移动而哈希 map 会成为这些值的所有者
use std::collections::HashMap;

let field_name = String::from("Favorite color");
let field_value = String::from("Blue");

let mut map = HashMap::new();
map.insert(field_name, field_value);
// 这里 field_name 和 field_value 不再有效，
// 尝试使用它们看看会出现什么编译错误！


访问哈希 map 中的值
use std::collections::HashMap;

let mut scores = HashMap::new();

scores.insert(String::from("Blue"), 10);
scores.insert(String::from("Yellow"), 50);

let team_name = String::from("Blue");
let score = scores.get(&team_name)

遍历：
for (key, value) in &scores {
	println!("{}: {}", key, value);
}

更新哈希 map
任何时候，每个键只能关联一个值，要改变哈希 map 中的数据时，必须决定如何处理一个键已经有值了的情况

覆盖一个值
use std::collections::HashMap;

let mut scores = HashMap::new();

scores.insert(String::from("Blue"), 10);
scores.insert(String::from("Blue"), 25);

println!("{:?}", scores);

只在键没有对应值时插入
use std::collections::HashMap;

let mut scores = HashMap::new();
scores.insert(String::from("Blue"), 10);

scores.entry(String::from("Yellow")).or_insert(50);
scores.entry(String::from("Blue")).or_insert(50);

println!("{:?}", scores);
Entry 的 or_insert 方法在键对应的值存在时就返回这个值的可变引用，如果不存在则将参数作为新值插入并返回新值的可变引用。这比编写自己的逻辑要简明的多，另外也与借用检查器结合得更好。

根据旧值更新一个值
use std::collections::HashMap;

let text = "hello world wonderful world";

let mut map = HashMap::new();

for word in text.split_whitespace() {
	let count = map.entry(word).or_insert(0);
	*count += 1;
}

println!("{:?}", map);
or_insert 方法事实上会返回这个键的值的一个可变引用（&mut V）

哈希函数
hasher 是一个实现了 BuildHasher trait 的类型

错误处理
Rust 将错误组合成两个主要类别：可恢复错误（recoverable）和 不可恢复错误（unrecoverable）
	可恢复错误通常代表向用户报告错误和重试操作是合理的情况
	不可恢复错误通常是 bug 的同义词，比如尝试访问超过数组结尾的位置

panic! 与不可恢复的错误
当出现 panic 时，程序默认会开始 展开（unwinding），这意味着 Rust 会回溯栈并清理它遇到的每一个函数的数据，不过这个回溯并清理的过程有很多工作。另一种选择是直接 终止（abort），这会不清理数据就退出程序。

Result 与可恢复的错误

Result 枚举
enum Result<T, E> {
	Ok(T),
	Err(E),
}
T 和 E 是泛型类型参数 (T 代表成功时返回的 Ok 成员中的数据的类型，而 E 代表失败时返回的 Err 成员中的错误的类型)


示例：
use std::fs::File;

fn main() {
	let f = File::open("hello.txt");

	let f = match f {
		Ok(file) => file,
		Err(error) => {
			panic!("Problem opening the file: {:?}", error)
		},
	};
}

匹配不同的错误
示例：
use std::fs::File;
use std::io::ErrorKind;

fn main() {
	let f = File::open("hello.txt");

	let f = match f {
		Ok(file) => file,
		Err(error) => match error.kind() {
			ErrorKind::NotFound => match File::create("hello.txt") {
				Ok(fc) => fc,
				Err(e) => panic!("Problem creating the file: {:?}", e),
			},
			other_error => panic!("Problem opening the file: {:?}", other_error),
		},
	};
}

也可以简化为
use std::fs::File;
use std::io::ErrorKind;

fn main() {
	let f = File::open("hello.txt").unwrap_or_else(|error| {
		if error.kind() == ErrorKind::NotFound {
			File::create("hello.txt").unwrap_or_else(|error| {
				panic!("Problem creating the file: {:?}", error);
			})
		} else {
			panic!("Problem opening the file: {:?}", error);
		}
	});
}

失败时 panic 的简写：unwrap 和 expect
match 能够胜任它的工作，不过它可能有点冗长并且不总是能很好地表明其意图。Result<T, E> 类型定义了很多辅助方法来处理各种情况

use std::fs::File;

fn main() {
	let f = File::open("hello.txt").unwrap();
}
unwrap 的实现就类似于 match 语句。如果 Result 值是成员 Ok，unwrap 会返回 Ok 中的值。如果 Result 是成员 Err，unwrap 会为我们调用 panic!

use std::fs::File;

fn main() {
	let f = File::open("hello.txt").expect("Failed to open hello.txt");
}
expect 与 unwrap 的使用方式一样：返回文件句柄或调用 panic! 宏。expect 在调用 panic! 时使用的错误信息将是我们传递给 expect 的参数，而不像 unwrap 那样使用默认的 panic! 信息

传播错误
除了在这个函数中处理错误外，还可以选择让调用者知道这个错误并决定该如何处理。这被称为 传播（propagating）错误

use std::io;
use std::io::Read;
use std::fs::File;

fn read_username_from_file() -> Result<String, io::Error> {
	let f = File::open("hello.txt");

	let mut f = match f {
		Ok(file) => file,
		Err(e) => return Err(e),
	};

	let mut s = String::new();

	match f.read_to_string(&mut s) {
		Ok(_) => Ok(s),
		Err(e) => Err(e),
	}
}

传播错误的简写：? 运算符
上述代码可以简写为：
use std::io;
use std::io::Read;
use std::fs::File;

fn read_username_from_file() -> Result<String, io::Error> {
	let mut f = File::open("hello.txt")?;
	let mut s = String::new();
	f.read_to_string(&mut s)?;
	Ok(s)
}

进一步缩短代码：
use std::io;
use std::io::Read;
use std::fs::File;

fn read_username_from_file() -> Result<String, io::Error> {
	let mut s = String::new();

	File::open("hello.txt")?.read_to_string(&mut s)?;

	Ok(s)
}

更短的写法：
use std::io;
use std::fs;

fn read_username_from_file() -> Result<String, io::Error> {
	fs::read_to_string("hello.txt")
}

? 运算符可被用于返回 Result 的函数


使用 panic! 还是不用 panic!
如果代码 panic，就没有恢复的可能
返回 Result 是定义可能会失败的函数的一个好的默认选择

错误处理指导原则
当代码有可能以有害状态结束时，建议让代码 panic。
有害状态是指一些假设、保证、协议或不可变性被打破的状态，比如无效的值、矛盾的值或缺失的值被传递给代码
有害状态是指一些意外的事情，而不是预期可能偶尔发生的事情，比如用户输入错误格式的数据。
在此之后的代码需要摆脱这种有害状态，而不是在每一步都检查这个问题。
在使用的类型中，没有一个好的方式来编码这些信息

当错误预期会出现时，返回 Result 仍要比调用 panic! 更为合适，例如：解析器接收到格式错误的数据，或者 HTTP 请求返回了一个表明触发了限流的状态，应该通过返回 Result 来表明失败预期是可能的，这样将有害状态向上传播，调用者就可以决定该如何处理这个问题


当代码对值进行操作时，应该首先验证值是有效的，并在其无效时 panic!
函数通常都遵循 契约（contracts）：他们的行为只有在输入满足特定条件时才能得到保证。当违反契约时 panic 是
有道理的，因为这通常代表调用方的 bug，而且这也不是那种你希望所调用的代码必须处理的错误


创建自定义类型进行有效性验证
示例：
pub struct Guess {
	value: i32,
}

impl Guess {
	pub fn new(value: i32) -> Guess {
		if value < 1 || value > 100 {
			panic!("Guess value must be between 1 and 100, got {}.", value);
		}

		Guess {
			value
		}
	}

	pub fn value(&self) -> i32 {
		self.value
	}
}
私有的字段 value 是很重要的，这样使用 Guess 结构体的代码将不允许直接设置 value 的值：调用者 必须 使用 
Guess::new 方法来创建一个 Guess 的实例，这就确保了不会存在一个 value 没有通过 Guess::new 函数的条件
检查的 Guess

泛型、trait 和生命周期
泛型是具体类型或其他属性的抽象替代
trait是一个定义泛型行为的方法，trait 可以与泛型结合来将泛型限制为拥有特定行为的类型，而不是任意类型
生命周期（lifetimes）是一类允许我们向编译器提供引用如何相互关联的泛型。Rust 的生命周期功能允许在很多场景下借用值的同时仍然使编译器能够检查这些引用的有效性

在函数定义中使用泛型
fn largest<T>(list: &[T]) -> T {
	let mut largest = list[0];

	for &item in list.iter() {
		if item > largest {
			largest = item;
		}
	}

	largest
}

fn main() {
	let number_list = vec![34, 50, 25, 100, 65];

	let result = largest(&number_list);
	println!("The largest number is {}", result);

	let char_list = vec!['y', 'm', 'a', 'q'];

	let result = largest(&char_list);
	println!("The largest char is {}", result);
}
有编译错误，表明 largest 的函数体不能适用于 T 的所有可能的类型

枚举定义中的泛型
enum Option<T> {
	Some(T),
	None,
}

enum Result<T, E> {
	Ok(T),
	Err(E),
}

方法定义中的泛型
struct Point<T, U> {
	x: T,
	y: U,
}

impl<T, U> Point<T, U> {
	fn mixup<V, W>(self, other: Point<V, W>) -> Point<T, W> {
		Point {
			x: self.x,
			y: other.y,
		}
	}
}

fn main() {
	let p1 = Point { x: 5, y: 10.4 };
	let p2 = Point { x: "Hello", y: 'c'};

	let p3 = p1.mixup(p2);

	println!("p3.x = {}, p3.y = {}", p3.x, p3.y);
}

泛型代码的性能
Rust 通过在编译时进行泛型代码的 单态化（monomorphization）来保证效率。单态化是一个通过填充编译时使用的具体类型，将通用代码转换为特定代码的过程

程序员使用泛型来编写不重复的代码，而 Rust 将会为每一个实例编译其特定类型的代码。这意味着在使用泛型时没有运行时开销；当代码运行，它的执行效率就跟好像手写每个具体定义的重复代码一样。
这个单态化过程正是 Rust 泛型在运行时极其高效的原因。

let integer = Some(5);
let float = Some(5.0);
当 Rust 编译这些代码的时候，它会进行单态化
enum Option_i32 {
	Some(i32),
	None,
}

enum Option_f64 {
	Some(f64),
	None,
}

fn main() {
	let integer = Option_i32::Some(5);
	let float = Option_f64::Some(5.0);
}

trait：定义共享的行为
trait 告诉 Rust 编译器某个特定类型拥有可能与其他类型共享的功能，可以通过 trait 以一种抽象的方式定义共享的行为。可以使用 trait bounds 指定泛型是任何拥有特定行为的类型。
注意：trait 类似于其他语言中常被称为 接口（interfaces）的功能，虽然有一些不同。

一个类型的行为由其可供调用的方法构成。如果可以对不同类型调用相同的方法的话，这些类型就可以共享相同的行为了。trait 定义是一种将方法签名组合起来的方法，目的是定义一个实现某些目的所必需的行为的集合
例如：
pub trait Summary {
	fn summarize(&self) -> String;
}
trait 关键字来声明一个 trait，后面是 trait 的名字，大括号中声明描述实现这个 trait 的类型所需要的行为的方法签名，在方法签名后跟分号，而不是在大括号中提供其实现
每一个实现 trait 的类型都需要提供其自定义行为的方法体，编译器也会确保任何实现该 trait 的类型都拥有与这个签名的定义完全一致的 方法
trait 体中可以有多个方法：一行一个方法签名且都以分号结尾。


为类型实现 trait
示例：
pub struct NewsArticle {
	pub headline: String,
	pub location: String,
	pub author: String,
	pub content: String,
}

impl Summary for NewsArticle {
	fn summarize(&self) -> String {
		format!("{}, by {} ({})", self.headline, self.author, self.location)
	}
}

pub struct Tweet {
	pub username: String,
	pub content: String,
	pub reply: bool,
	pub retweet: bool,
}

impl Summary for Tweet {
	fn summarize(&self) -> String {
		format!("{}: {}", self.username, self.content)
	}
}

调用 trait 方法：
let tweet = Tweet {
	username: String::from("horse_ebooks"),
	content: String::from("of course, as you probably already know, people"),
	reply: false,
	retweet: false,
};

println!("1 new tweet: {}", tweet.summarize());

实现 trait 时需要注意的一个限制是，只有当 trait 或者要实现 trait 的类型位于 crate 的本地作用域时，才能为该类型实现 trait，不能为外部类型实现外部 trait

默认实现
有时为 trait 中的某些或全部方法提供默认的行为，而不是在每个类型的每个实现中都定义自己的行为是很有用的
pub trait Summary {
	fn summarize(&self) -> String {
		String::from("(Read more...)")
	}
}
如果想要对 NewsArticle 实例使用这个默认实现，而不是定义一个自己的实现，则可以通过 impl Summary for NewsArticle {} 指定一个空的 impl 块
重载一个默认实现的语法与实现没有默认实现的 trait 方法的语法一样

默认实现允许调用相同 trait 中的其他方法，哪怕这些方法没有默认实现
pub trait Summary {
	fn summarize_author(&self) -> String;

	fn summarize(&self) -> String {
		format!("(Read more from {}...)", self.summarize_author())
	}
}
请注意，无法从相同方法的重载实现中调用默认方法




```