```go
/// Variable
var age int = 25
age2 := 26	// type inference

/// Arithmetic
a := 1
b := 2
sum := a + b
difference := a - b
product := a * b
quotient := a / b

/// Conditional Statement used for decision making
number := 10
if number > 20 {
	fmt.Println("Number is greater than 20")
} else if number >  5 {
	fmt.Println("Number is greater than 5")
} else {
	fmt.Println("Number is 5 or less")
}

/// Loops used to repeat a set of instructions
for i := 0; i < 5; i++ {
	fmt.Println("Iteration:", i)
}

// 
 i := 1
 for {
	if i > 5 {
		break
	}
	fmt.Println("Iteration:", i)
	i++
 }

 ///	Functions
 func addAndMultiply(a int, b int) (int, int) {
		sum := a + b
		product := a * b
		return sum, product
}
resultSum, resultProduct := addAndMultiply(3, 4)
fmt.Println("Sum:", resultSum)
fmt.Println("Product:", resultProduct)

/// Arrays and Sclices
// the size of Array is fixed
// Slices have a dynamic size
var arr [3]int = [3]int{1, 2, 3}	// array
var slice []int = []int{1, 2, 3}	// slice
slice := arr[1:3]	// slice of array, from index 1 to 2

/// Maps, used to store key-value pairs, allow quick lookups, insertions, and deletions, keys must be unique and of a comparable type
ages := map[string]int{
	"John": 30,
	"Jane": 25,
	"Bob": 35,
}

ages["Alice"] = 28	// add a new key-value pair
delete(ages, "Bob")	// delete a key-value pair
age, exists := ages["John"]	// check if a key exists
fmt.Println("John's age:", age, "Exists:", exists)

for name, age := range ages {
	fmt.Println("Name:", name, "Age:", age)
}

/// Error Handling
// Go uses multiple return values to handle errors

import "errors"

//
func divide(a, b int) (int, error) {
	if b == 0 {
		return 0, errors.New("division by zero")
	}
	return a / b, nil
}

result , err := divide(10, 0)
if err != nil {
	fmt.Println("Error:", err)
}

// Propagating errors up the call stack
func process(a, b int) (int, error) {
	result, err := divide(a, b)
	if err != nil {
		return 0, fmt.Errorf("process: %w", err)
	}
	return result, nil
}

func divide(a, b int) (int, error) {
	if b == 0 {
		return 0, errors.New("division by zero")
	}
	return a / b, nil
}

result, err := process(10, 0)
if err != nil {
	fmt.Println("Error:", err)
	return
}

fmt.Println("Result:", result)

/// Structs, used to group related data together, similar to classes in OOP
// A method is a function that is associated with a struct to perform operations on its data
type Person struct {
	Name string
	Age  int
}

func (p Person) DisplayInfo() {
	fmt.Println("Name:", p.Name, "Age:", p.Age)
}

p := Person{Name: "John", Age: 30}
p.DisplayInfo()

/// Pointers, used to store the memory address of a variable, allows for efficient memory management and manipulation of data
var x int = 10
var p *int = &x	// pointer to x
fmt.Println("Value of x:", *p)	// dereference the pointer to get the value

a := 10
b := 20
ptr1 := &a
ptr2 := &b
*ptr1, *ptr2 = *ptr2, *ptr1	// swap values using pointers
fmt.Println("a:", a, "b:", b)	// a: 20 b: 10

/// Strings, strings are immutable sequences of bytes, used to represent text
str := "Hello"
greeting := str + " World"	// concatenate strings
fmt.Println(greeting)	// Hello World
fmt.Println(len(greeting))	// length of string

//
str := "Hello, Go!"
slice := str[7:9]	// slice of string, from index 7 to 8
fmt.Println("Slice:", slice)	// Slice: Go
containsGo := strings.Contains(str, "Go")	// check if string contains "Go"
fmt.Println("Contains 'Go':", containsGo)	// Contains 'Go': true

/// Type Conversion, used to convert a value from one type to another
num := 42
fmt.Println("Integer:", num)
f := float64(num)	// convert int to float64
fmt.Println("Float:", f)
str := strconv.Itoa(num)	// convert int to string

// custom type conversion
type MyInt int
var x MyInt = 10
var y int = int(x)	// convert MyInt to int
fmt.Println("MyInt:", x, "Converted to int:", y)	// MyInt: 10 Converted to int: 10

/// Interfaces, used to define a contract that types must implement, allows for polymorphism
type Speaker interface {
	Speak() string
}

type Person struct {
	Name string
}
func (p Person) Speak() string {
	return "Hello, my name is " + p.Name
}

var speaker Speaker
speaker = Person{Name: "John"}
fmt.Println(speaker.Speak())	// Hello, my name is John

// interface{}: empty interface, can hold values of any type, it is useful when you don't know the type of value at compile time
type Printer interface {
	Print() string
}

type Document struct {
	Title string
}

func (d Document) Print() string {
	return "Document title: " + d.Title
}

func PrintDetails(p Printer) {
	fmt.Println(p.Print())
}

doc := Document{Title: "My Document"}
PrintDetails(doc)	// Document title: My Document

/// Custom types, used to create new types based on existing types
type Age int
type Person struct {
	Name string
	Age  Age
}

person := Person{Name: "John", Age: 30}
fmt.Println("Name:", person.Name, "Age:", person.Age)	// Name: John Age: 30

/// Slices vs Arrays
// Slices are more flexible and easier to work with than arrays
// Slices are dynamically sized, while arrays have a fixed size
// Slices are reference types, while arrays are value types
// Slices can be resized, while arrays cannot
// Slices can be passed to functions without copying the entire array, while arrays are copied when passed to functions
// Slices can be created using the built-in make function, while arrays are created using the array literal syntax
// Slices can be created using the built-in append,copy,len,cap,range,delete,sort,strings,bytes,time,reflect function, while arrays cannot
arr := [3]int{1, 2, 3}	// array
slice := arr[1:3]	// slice of array, from index 1 to 2
fmt.Println("Slice:", slice)	// Slice: [2 3]
slice[0] = 4	// modify slice (slice is a reference to the underlying array)
fmt.Println("Slice:", slice)	// Slice: [4 3]
fmt.Println("Array:", arr)	// Array: [1 4 3]

/// Range loops
// A range loop is used to iterate over elements in a collection, such as an array, slice, or map
// The range loop returns two values: the index and the value of the element at that index
fruits := []string{"apple", "banana", "cherry"}
for index, fruit := range fruits {
	fmt.Println("Index:", index, "Fruit:", fruit)
}

//
colors := map[string]string{
	"red":   "#FF0000",
	"green": "#00FF00",
	"blue":  "#0000FF",
}
for color, hex := range colors {
	fmt.Println("Color:", color, "Hex:", hex)
}

/// Goroutines, used to run functions concurrently, lightweight threads managed by the Go runtime
// Goroutines can share data, but synchronization is necessary
// Use channels to communicate between goroutines
func greet(ch chan string) {
	ch <- "Hello from goroutine!"	// send message to channel
}

ch := make(chan string)	// create a channel
go greet(ch)	// start goroutine
message := <-ch	// receive message from channel, blocking until message is received
fmt.Println(message)	// Hello from goroutine!


/// Channels, used to communicate between goroutines, allow for synchronization and data sharing
ch := make(chan string, 2)	// create a buffered channel
ch <- "Message 1"	// send message to channel
ch <- "Message 2"	// send message to channel
fmt.Println(<-ch)	// receive message from channel
fmt.Println(<-ch)	// receive message from channel
close(ch)	// close the channel

/// Goroutine and Concurrency
import "sync"

func printMessage(message string, wg *sync.WaitGroup) {
	defer wg.Done()	// signal that the goroutine is done
	fmt.Println(message)
}
func main() {
	var wg sync.WaitGroup	// create a WaitGroup
	wg.Add(2)	// add two goroutines to the WaitGroup
	go printMessage("Hello from goroutine 1!", &wg)	// start first goroutine
	go printMessage("Hello from goroutine 2!", &wg)	// start second goroutine
	wg.Wait()	// wait for all goroutines to finish
	fmt.Println("All goroutines finished!")
}

/// File I/O, used to read and write files
// read file
import (
	"os"
	"bufio"
)

file, err := os.Open("example.txt")	// open file
if err != nil {
	fmt.Println("Error:", err)
	return
}
defer file.Close()	// close file when done
scanner := bufio.NewScanner(file)	// create a scanner to read the file
for scanner.Scan() {
	fmt.Println(scanner.Text())	// print each line
}
if err := scanner.Err(); err != nil {
	fmt.Println("Error reading file:", err)	// handle error
	return
}
// write file
file, err := os.Create("output.txt")	// create file
if err != nil {
	fmt.Println("Error:", err)
	return
}
defer file.Close()	// close file when done

/*
writer := bufio.NewWriter(file)	// create a writer to write to the file
_, err = writer.WriteString("Hello, World!\n")	// write to file
if err != nil {
	fmt.Println("Error writing to file:", err)	// handle error
	return
}
writer.Flush()	// flush the writer to ensure all data is written
fmt.Println("File written successfully!")	// success message
*/

_, err := file.WriteString("Hello, World!")
if err != nil {
	fmt.Println("Error writing to file:", err)
} else {
	fmt.Println("Data written to file successfully!")
}

/// JSON
import (
    "encoding/json"
)

type User struct {
    Name  string `json:"name"`
    Email string `json:"email"`
}

data := `{"name": "John Doe", "email": "alice@example.com"}`
var user User
err := json.Unmarshal([]byte(data), &user)
if err != nil {
    fmt.Println("Error:", err)
    return
}
fmt.Println("Name:", user.Name)
fmt.Println("Email:", user.Email)

//
user := map[string]interface{}{
    "name":  "Bob",
    "age":   30,
    "contact": map[string]string{"email": "bob@example.com", "phone": "123-456-7890"},
}
jsonData, err := json.Marshal(user)
if err != nil {
    fmt.Println("Error:", err)
    return
}
fmt.Println("JSON Data:", string(jsonData))

/// Defer
// Defer is used to ensure that a function call is performed later in a program's execution
// Defer can be used to close files, unlock mutexes, or perform cleanup tasks
// Multiple deferred calls are executed in LIFO order
// You can pass arguments to deferred functions, but they are evaluated immediately
func example() {
    defer fmt.Println("First defer.")   // 3
    defer fmt.Println("Second defer.")  // 2

    fmt.Println("Function logic.")      // 1
}
example()

/// String Formatting
name := "John"
age := 30
fmt.Printf("My name is %s and I am %d years old.\n", name, age)

//
type Person struct {
    Name string
    Age  int
}
p := Person{Name: "Alice", Age: 25}
// formatting with width and precision, zero padding
// left and right alignment with - and +
// %v is used for default formatting
fmt.Printf("|%-20s|%05d|\n", p.Name, p.Age)

/// Built-in Libraries
import (
    "math"
    "net/http"
    "time"
)

fmt.Println("Square root of 16:", math.Sqrt(16))

reponse, err := http.Get("https://golang.org")
if err != nil {
    fmt.Println("Error:", err)
    return
}
fmt.Println("Response status:", response.Status)

/// Sorting
func bubbleSort(arr []int) {
    n := len(arr)
    for i := 0; i < n-1; i++ {
        for j := 0; j < n-i-1; j++ {
            if arr[j] > arr[j+1] {
                arr[j], arr[j+1] = arr[j+1], arr[j]
            }
        }
    }
}

func selectionSort(arr []int) {
    n := len(arr)
    for i := 0; i < n-1; i++ {
        minIndex := i
        for j := i + 1; j < n; j++ {
            if arr[j] < arr[minIndex] {
                minIndex = j
            }
        }
        arr[i], arr[minIndex] = arr[minIndex], arr[i]
    }
}

/// Concurrency and Parrallelism
// Concurrency
func main() {
    go sayHello() {
        fmt.Println("Hello from goroutine!")
    }
    time.Sleep(1 * time.Second) // Wait for goroutine to finish
    fmt.Println("Main function finished.")
}

// Parrallelism
func task(id int) {
    fmt.Printf("Task %d is running\n", id)
    time.Sleep(2 * time.Second)
    fmt.Printf("Task %d is done\n", id)
}

func main() {
    runtime.GOMAXPROCS(2) // Set the number of OS threads to use, allow up to 2 tasks to run in parallel
    for i := 1; i <= 5; i++ {
        go task(i)  // Start a goroutine for each task
    }
    time.Sleep(10 * time.Second) // Wait for all tasks to finish
}

/// Channels and Buffering
func worker(id int, ch chan int) {
    fmt.Printf("Worker %d: Received %d\n", id, <-ch)
}

func main() {
    ch := make(chan int, 3) // Create a buffered channel with a capacity of 3
    // Start multiple workers
    for i := 1; i <= 3; i++ {
        go worker(i, ch) // Start a goroutine for each worker
    }
    // Send data to the channel
    for i := 1; i <= 3; i++ {
            ch <- i // Send data to the channel
    }
    fmt.Println("All workers are done.")
}
```