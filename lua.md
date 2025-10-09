[**Lua Documentation**](https://www.lua.org/docs.html)  

[**Lua 5.4 Reference Manual**](https://www.lua.org/manual/5.4/)  

[The Standard Libraries](https://www.lua.org/manual/5.4/manual.html#6)  

[**Programming in Lua**](https://www.lua.org/pil/contents.html)  

[**Lua Directory**](http://lua-users.org/wiki/LuaDirectory)  

[**LuaRocks**](https://luarocks.org/) # package manager  

[Notes on the Implementation of Lua 5.3](https://poga.github.io/lua53-notes/introduction.html)  

[The Lua Tutorial](https://dev.to/jd2r/the-lua-tutorial-544b)  

[Lua online](https://www.lua.org/demo.html)  
[Lua for Programmers](https://ebens.me/posts/lua-for-programmers-part-1/)  
[lua-style-guide](https://github.com/luarocks/lua-style-guide)  
[]()  
[Lua Programming Gems](https://www.lua.org/gems/)  
[Lua Style Guide](https://roblox.github.io/lua-style-guide/)  
[]()  
[Lua examples](https://tutorial.realtimelogic.com/Lua-Types.lsp)  
[]()  

[middleclass(Object-orientation for Lua)](https://github.com/kikito/middleclass)  
[]()  
[]()  
[Binding Code To Lua](http://lua-users.org/wiki/BindingCodeToLua)  
[LuaBridge3](https://kunitoki.github.io/LuaBridge3/)  
[Fun with Lua bindings](https://realmensch.org/2012/02/04/fun-with-lua-bindings/)  
[Simple Cpp Binding](http://lua-users.org/wiki/SimpleCppBinding)  
[]()  
[Using Lua with C++ (and C)](https://edw.is/using-lua-with-cpp/)  
[Integrating Lua with C](https://marcbalmer.ch/)  
[[C/C++] A Lua C API Cheat Sheet](https://www.codingwiththomas.com/blog/a-lua-c-api-cheat-sheet)  
[]()  
[]()  

[Understanding globals _ENV and _G](https://www.whoop.ee/)  
[Design patterns in Lua](https://www.whoop.ee/)  
[]()  
[]()  

## lua stack
```lua
-- 栈索引
正索引(positive indexes): 从栈底开始，1为第一个元素
负索引(negavite indexes): 从栈顶开始，-1为第一个元素
+-----+-----+-----+-----+
|  1  |  2  |  3  |  4  |   <- 正索引(从底到顶)
| -1  | -2  | -3  | -4  |   <- 负索引(从顶到底)
+-----+-----+-----+-----+

-- c 调用 lua 函数
-- lua 函数
add(10, 20)   -->  30, "success"
-- c
lua_getblobal(L, "add")   -- push function on the stack
lua_pushnumber(L, 10)     -- push first argument
lua_pushnumber(L, 20)     -- push second argument

栈底 1/-3   栈顶 -1/3
   |           |
+-----+-----+-----+
| add | 10  | 20  |
+-----+-----+-----+

lua_pcall(L, 2, 2, 0)
-- 从栈顶弹出2个参数和1个函数共3个元素，此时栈被清空到调用前的状态
-- 在 Lua 环境中执行 add(10, 20)
-- 将函数的2个返回值压栈

栈底 1/-2   栈顶 -1/2
   |           |
+--------+-----------+
|   30   | "success" |
+--------+-----------+

double result = lua_tonumber(L, -1);  // Get return value

lua_pop(L, 2)   -- 栈被清空到调用前的状态


-- lua 调用 c 函数
-- c 函数
multiply(5, 8)    -->   40.0, "ok"

-- lua
multiply(5, 8)    -- lua虚拟机会自动为c函数准备好栈


栈底 1/-2   栈顶 -1/2
   |           |
+--------+-----------+
|    5   |     8     |
+--------+-----------+

lua_tonumber(L, 1)    // read arg 1
lua_tonumber(L, 2)    // read arg 2

-- caculate in c

lua_pushnumber(L, 40.0)    // push result 1
lua_pushliteral (L, "ok")  // push result 2
-- c 函数返回 2 （2个返回值）
-- c 代码不需要清理栈

-- Lua 清理栈并接收返回值
-- 自动弹出c函数本身和所有参数
-- 保留c函数压入的返回值
-- 将返回值移动至栈底(如果需要)

-- 最用的栈状态（对lua脚本可见）

栈底 1/-2   栈顶 -1/2
   |           |
+--------+-----------+
|  40.0  |   "ok"    |
+--------+-----------+

-- lua 脚本像使用普通函数的返回值一样使用它们


-- c call lua function
-- script.lua
functon add(a, b)
  return a + b
end

-- c code
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

int main() {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    luaL_dofile(L, "script.lua");  // Load and run script (defines 'add')

    lua_getglobal(L, "add");       // Push function onto the stack
    lua_pushnumber(L, 10);         // Push first argument
    lua_pushnumber(L, 20);         // Push second argument

    // Stack now: [add][10][20]
    lua_call(L, 2, 1);             // Call add(a,b), 2 args, 1 result

    double result = lua_tonumber(L, -1);  // Get return value
    printf("Result: %f\n", result);

    lua_pop(L, 1);  // Clean up stack
    lua_close(L);
    return 0;
}

-- lua state stack
+----------------------------------------------+
|                Lua Stack                     |
+----------------------------------------------+
| Step 0: load script -> []                    |
| Step 1: getglobal("add") -> [add]            |
| Step 2: pushnumber(10) -> [add][10]          |
| Step 3: pushnumber(20) -> [add][10][20]      |
| Step 4: lua_call(2,1) -> [30]                |
| Step 5: read & pop -> []                     |
+----------------------------------------------+

-- lua call c function
-- script.lua
-- Lua function called first from C
function funcA()
    print("[Lua] funcA called from C")
    local result = c_func(5)   -- Call into C again
    print("[Lua] funcA got result:", result)
    return result * 2
end

-- Another Lua function that will be called from C later
function funcB(x)
    print("[Lua] funcB called from C, x=", x)
    return x + 1
end

-- c code
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// Forward declaration
int c_func(lua_State *L);

int main() {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    // Load the Lua script
    luaL_dofile(L, "script.lua");

    // Register C function into Lua
    lua_pushcfunction(L, c_func);
    lua_setglobal(L, "c_func");

    // --- Call Lua function funcA() from C ---
    lua_getglobal(L, "funcA");
    lua_call(L, 0, 1);

    double finalResult = lua_tonumber(L, -1);
    printf("[C] Final result = %f\n", finalResult);

    lua_close(L);
    return 0;
}

// --- C function called by Lua ---
int c_func(lua_State *L) {
    double x = lua_tonumber(L, 1);
    printf("[C] c_func called from Lua, x=%f\n", x);

    // Call Lua function funcB(x * 10)
    lua_getglobal(L, "funcB");
    lua_pushnumber(L, x * 10);
    lua_call(L, 1, 1);

    double y = lua_tonumber(L, -1);
    lua_pop(L, 1);

    printf("[C] c_func got result from funcB: %f\n", y);
    lua_pushnumber(L, y + 100); // Return y+100 back to Lua
    return 1;                   // tells Lua: 1 return value
}

-- stack flow
C(main)
 └── call funcA (Lua)
      └── call c_func (C)
           └── call funcB (Lua)
           ◄── return 51
      ◄── return 151
 ◄── return 302

```

## variable scope
```lua
--[[
1. global variables
2. local variables
3. table fields
]]

-- global
-- lua keeps all its global variables in a regular table, called the environment
_G

x = 10  -- This is a global variable
print(_G.x)   -- 10
print(x)      -- 10
print(_G.print(x))  -- 10

-- local
local x = 10
function getX()
  local x = 20
  return x
end

print(x)        -- 10
print(getX())   -- 20

-- every chunk has its own local variables
local x = 10
if x == 10 then
  local x = 20
end

print(x)    -- 10

-- table fields
Any table key-value pair is the variable with its own value. Undefined variables in Lua are always nil
local t = {}
t.x = 10
print(t.x)    -- 10
print(t.y)    -- nil
print(t.z)    -- nil

-- _ENV
_ENV table it is a table that defines the environment in which code runs. By assigning a new table to _ENV, you can sandbox or restrict access to globals.

_ENV = { print = print }
print("hello")              -- works
math.sqrt(9)                -- Error: math is nil

-- best practice
1. use local variables whenever possible
2. explicitly declare globals if needed, e.g. global_x = 10 or gloalX = 10
3. use _ENV for namespacing or sandboxing only
local safeENV = {
  print = print,
  tostring = tostring
}

_ENV = safeENV

print(tonumber)   -- nil
print(type)       -- nil
print(print)      -- function: 0x00000001
print(tostring)   -- function: 0x00000001

```

## metatable
```lua
-- A metatable defines various extraneous behaviors for a table
-- Any table (and userdata) may be assigned a metatable

-- our sample table
local tab = {}
-- our metatable
local metatable = {
    -- This table is then what holds the metamethods or metafields
    -- that Lua will read to determine the tables's behavior, eg.
    __index = function() --[[ ... ]] end
}

setmetatable(tab, metatable)

-- metatable can be retrieved back
local metatable = getmetatable(tab)

-- remove a metatable and restore regular functionality
stmetatable(tab, nil)

-- for signature f(a, b), a and b don't necessarily have to be instances of your metatable. One of them will always be, but not necessarily the first

-- for f(a, b)
1. check the first operand (even if it is a number) a
2. if operand a does not define a metamethod for the operation then Lua will check the second operand b
3. if Lua can find a metamethod for operand b, it calls the metamethod with the two operands as arguments, and the result of the call (adjusted to one value) is the result of the operation
4. otherwise, if no metamethod is found, Lua raises an error

-- calculation operators
__add(a, b)
__sub(a, b)
__mul(a, b)
__div(a, b)
__unm(a)
__mod(a, b) (Lua 5.1)
__pow(a, b) (Lua 5.1)
__idiv(a, b) (Lua 5.3)

-- bitwise operators(Lua 5.3)
__band(a, b)
__bor(a, b)
__bxor(a, b)
__bnot(a)
__shl(a, b)
__shr(a)

-- equation operators
__eq(a, b)
__lt(a, b)
__le(a, b)

-- misc operators
__concat(a, b)
—__len(a) (Lua 5.1)

-- behavior methods
-- indexing
__index
__newindex

--calling
__call(args)

-- garbage collection & memory management
__mode
__close(value, err?) (Lua 5.4)
__gc()

-- misc
__tostring()
__metatable
__name
__pairs() (Lua 5.2)
__ipairs() (Lua 5.2 - Lua 5.3)

-- implementation-specific
__iterator()
__usedindex()
```

## syntactic sugar
```lua
-- 1
object:method(args...)      -- the : automatically passes the table/string as the first argument (self)
    -- equivalent to
object.method(object, args...)

--
local s = "Hello, World"
print(s:sub(1, 5))          -- prefered
print(string.sub(s, 1, 5))  -- equivalent

--
local s = "Lua"
print(s:upper())            -- perfered
print(string.upper(s))      -- equivalent

-- 2
t = {1, 2 ,3}               -- same as t = { [1]=1, [2]=2, [3]=3 }
t = {name="Tom", age=30}    -- same as t = { ["name"]="Tom", ["age"]=30 }
t = {"one", "two", foo=42}  -- same as t = { [1]="one", [2]="two", ["foo"]=42}

-- 3
print "hello"               -- same as print("hello")
f {x=10, y=20}              -- same as f({x=10, y=20})

-- 4
s1 = "hello"
s2 = "world"
print(s1 .. " " .. s2)      -- .. is sugar for string concatenation

-- 5
t = {1, 2, 3}
print(#t)                   -- number of elements (array-like part)
print(#"Lua")               -- string length

-- 6
square = function(x) return x*x end
print(square(5))

table.sort(nums, function(a, b) return a>b end)

-- 7
if not condition then return end
local x = condition and v1 or v2    -- lua doesn't have ?:

lcoal status = ok and "success" or "failure"

-- 8
a + b           __add
a - b           __sub
a .. b          __concat
a == b          __eq
a < b           __lt
a()             __call
a[k]            __index/__newindex

meta = { __add = function(a, b) return a.value + b.value end }
setmetatable(a, meta)
setmetatable(b, meta)
print(a+b)      -- calls meta.__add(a,b)

-- 9
function sum(...)
  local s = 0
  for _, v in ipairs({...}) do
    s = s + v
  end
  return s
end

-- examples
-- 1
local Person = {}
Person.__index = Person

-- Note: use : instead of . to allow self to be implicitly passed to the method
-- function Person.new(self, name, age)
function Person:new(name, age)
  return setmetatable({name=name, age=age}, self)           -- {} builds a dict-like table
end

function Person:speak()
  print(self.name .. " say hi, I am " .. self.age .. " years old")  -- .. concatenates strings
end

local p1 = Person:new("Tom", 30)
p1:speak()                          -- same as p1.speak(p1)

-- 2
-- server = configure({host="127.0.0.1", port=8080})
server = configure {        -- no parens needed because it’s a single table argument
  host = "127.0.0.1",
  port = 8080
}

-- 3
local n = 5
local parity = (n % 2 == 0) and "even" or "odd"
print("n is " .. parity)

-- 4
Vector = {}
Vector.__index = Vector

function Vector:new(x, y)
  return setmetatable({x=x, y=y}, self)
end

function Vector:__add(v)
  return Vector:new(self.x + v.x, self.y + v.y)
end

function Vector:__tostring()
  return "(" .. self.x ..", " .. self.y ..")"
end

local a = Vector:new(1, 2)
local b = Vector:new(3, 4)
print(a+b)

-- 5
local App = {}
App.__index = App

-- "call" metamethod makes App { ... } work like a constructor
setmetatable(App, {
  __call = function (_, t)
	return setmetatable(t, App)
  end
})

function App:run()
	print(self.name .. " version " .. self.version .. " is running!")
end

App {
	name = "MyApp",
	version = "1.0"
}:run()

-- DSL-like config system
-- Features in this enhanced DSL:
----Nested workflows — group related tasks together.
----Conditional execution — skip tasks dynamically.
----Automatic wrapping — tables are automatically treated as Task or Workflow.
----Declarative style — still feels like a config file, easy to read and extend.
----Composable — you can create complex hierarchies of tasks with minimal boilerplate.

-- Helper function to create callable classes
local function make_callable(class)
  class.__index = class
  setmetatable(class, {
    __call = function(_, t)
      return setmetatable(t, class)
    end
  })
  return class
end

-- Base DSL class
local DSL = make_callable({})

-- Task entity
local Task = make_callable(setmetatable({}, DSL))

function Task:run(completed)
  completed = completed or {}

  -- Skip if condition fails
  if self.condition and not self.condition() then
    print("Skipping task: " .. (self.name or "<unnamed>"))
    return
  end

  -- Check dependencies
  if self.depends_on then
    for _, dep in ipairs(self.depends_on) do
      if not completed[dep] then
        print("Skipping task: " .. self.name .. " (waiting for " .. dep .. ")")
        return
      end
    end
  end

  -- Run the action
  print("Running task: " .. (self.name or "<unnamed>"))
  if self.action then
    self.action()
  end

  -- Mark as completed
  if self.name then
    completed[self.name] = true
  end
end

-- Workflow entity
local Workflow = make_callable({})
-- Override the default __call to handle tasks initialization
setmetatable(Workflow, {
  __call = function(_, t)
    t.tasks = t.tasks or {}
    return setmetatable(t, Workflow)
  end
})

function Workflow:run(completed)
  completed = completed or {}
  print("Starting workflow: " .. (self.name or "<unnamed>"))
  for _, t in ipairs(self.tasks) do
    -- Handle Task or nested Workflow
    local mt = getmetatable(t)
    if mt == Task or mt == Workflow then
      t:run(completed)
    else
      -- Automatically wrap tables
      if t.tasks then
        t = Workflow(t)
      else
        t = Task(t)
      end
      t:run(completed)
    end
  end
end

-- Example workflow with dependencies
local myWorkflow = Workflow{
  name = "Daily Jobs",
  tasks = {
    Task{name="Backup", action=function()
      print(" -> Backing up files...")
    end},

    Task{name="Cleanup", action=function()
      print(" -> Cleaning temp files...")
    end, depends_on={"Backup"}},  -- runs only after Backup

    Workflow{
      name="Reports",
      tasks={
        Task{name="Sales Report", action=function()
          print(" -> Generating sales report...")
        end, depends_on={"Cleanup"}},  -- waits for Cleanup

        Task{name="Inventory Report", action=function()
          print(" -> Generating inventory report...")
        end, depends_on={"Cleanup"}},
      }
    },

    Task{name="Notify", action=function()
      print(" -> Sending notifications...")
    end, depends_on={"Sales Report", "Inventory Report"}},  -- waits for reports
  }
}

myWorkflow:run()
```

## execute command and read/write file
```lua
-- execute external command
-- run a command without output capture
os.execute("ls -l")

-- run a command with output captured
local handle = io.popen("ls -l", "r")
local result = handle:read("*a")
handle:close()

print(result)

-- ssh
local user = "morrism"
local host = "135.242.60.169"
local command = "ls /tmp"

local ssh_cmd = string.format('ssh %s@%s "%s"', user, host, command)

local handle = io.popen(ssh_cmd)
local result = handle:read("*a")
handle:close()

print("Output from SSH command:\n" .. result)

-- write to a command(send input)
local handle = io.popen("grep hello", "w")
handle:write("hello world\nthis is a test\n")
handle:close()

-- 
local success, exit_type, code = os.execute("ls -l")
print(success, exit_type, code)

-- io.read and file:read
-- io.read reads from io.stdin
-- file:read reads from file object opened with io.open
"*a"        (all)
"*l"        (line)
"*n"        (number)
number      (explicit characters)

--
print("Enter two numbers:")
local n1 = io.read("*n")
local n2 = io.read("*n")
print("Sum:", n1 + n2)

--
local f = io.open("data.txt", "r")
local all = f:read("*a")
f:close()
print(all)

--
local f = io.open("data.txt", "r")
for line in f:lines() do
    print(line)
end
f:close()

--
local lines = {}

for line in io.lines("data.txt") do
  table.insert(lines, line)
end

for _, line in ipairs(lines) do
  print(line)
end

--
print("Enter a filename:")
local filename = io.read("*l")

local f = io.open(filename, "r")
if not f then
    print("Cound not open file:", filename)
    return
end

local content = f:read("*a")
f:close()
print("File content:\n" .. content)
```

## wireshark plugin
[Lua API Reference](https://www.wireshark.org/docs/wsdg_html_chunked/lua_module_Proto.html)  
```lua
-- wireshark lua plugin framework
--[[
1. Captrue engine                   -- pcap input, live capture, file read
2. Dissector Pipeline               -- main packet decoding
    2.1 Built-in dissectors             -- e.g., Ethernet -> IP -> TCP
    2.2 Lua dissectors                  -- Proto objects you register
    2.3 Post-dissectors                 -- Lua/C hooks after dissection
    2.4 Diplay filters/tree             -- TreeItem API, columns
3. GUI/Output                       -- Packet list, packet details, info

In parrallel:
Tap listeners                       -- Lua listener objects
    passive hooks                   -- stats, exporters, counters
]]


-- API layers
--[[
1. Proto / ProtoField      →       define protocols & fields.
2. Dissector function      →       decode packet data.
3. DissectorTable          →       hook dissectors into Wireshark’s pipeline.
4. Post-dissector          →       annotate packets after main dissection.
5. Listener (tap)          →       passively monitor packets for stats/logging.
6. GUI hooks               →       add menus & tools.
7. Utilities               →       use built-in dissectors, byte arrays, prefs, etc.
]]

-- basic lua plugin skeleton

------------------------------------------------------
-- Foo Protocol Dissector + Postdissector + Listener
------------------------------------------------------

-- 1. Define a new protocol
local foo_proto = Proto("foo", "Foo Protocol")

-- Fields
local f_type   = ProtoField.uint8("foo.type", "Message Type", base.DEC)
local f_length = ProtoField.uint16("foo.length", "Length", base.DEC)
local f_data   = ProtoField.string("foo.data", "Payload")

foo_proto.fields = { f_type, f_length, f_data }

------------------------------------------------------
-- 2. Dissector
------------------------------------------------------
function foo_proto.dissector(buffer, pinfo, tree)
    pinfo.cols.protocol = "FOO"

    local subtree = tree:add(foo_proto, buffer(), "Foo Protocol Data")

    -- Parse fields: 1 byte type, 2 bytes length, rest payload
    subtree:add(f_type, buffer(0,1))
    subtree:add(f_length, buffer(1,2))
    subtree:add(f_data, buffer(3))
end

-- Register dissector on TCP port 7777
DissectorTable.get("tcp.port"):add(7777, foo_proto)

------------------------------------------------------
-- 3. Post-dissector
------------------------------------------------------
local foo_post = Proto("foo_post", "Foo Post-Dissector")

function foo_post.dissector(buffer, pinfo, tree)
    -- Mark large packets
    if buffer:len() > 100 then
        pinfo.cols.info:append(" [FOO-LARGE]")
    end
end

register_postdissector(foo_post)

------------------------------------------------------
-- 4. Listener (Tap)
------------------------------------------------------
local foo_tap = Listener.new("tcp")

function foo_tap.packet(pinfo, tvb)
    -- Passive logging of all FOO traffic
    if pinfo.dst_port == 7777 or pinfo.src_port == 7777 then
        print(string.format("FOO tap: packet #%d length=%d",
            pinfo.number, tvb:len()))
    end
end

------------------------------------------------------
-- 5. GUI Tool (optional)
------------------------------------------------------
local function foo_tool()
    new_dialog("Foo Tool", function() end)
    print("Foo Tool executed from menu")
end

register_menu("Tools/Foo Tool", foo_tool, MENU_TOOLS_UNSORTED)

------------------------------------------------------
-- End of plugin
------------------------------------------------------
```

## lua note
```lua
-- first program

local function fact(n)
	if n == 1 then
		return 1
	else
		return n * fact(n - 1)
	end
end

print("Enter a number:")
local n = io.read("*number")
print(fact(n))

-- chunk is simply a sequecne of statements

-- file 'lib1.lua'
function norm(x, y)
	local n2 = x^2 + y^2
	return math.sqrt(n2)
end

-- in interactive mode
dofile("lib1.lua")
n = norm(3, 4)

-- global variables
print(g)	-- nil
g = 10
print(g)	-- 10

-- comment
--[[
this is
a comment
]]

-- types and values
-- lua is a dynamically typed language, variables have no predefined types, any variable may contain values of any type

print(type("Hello world"))		-- string
print(type(10.4*3))				-- number
print(type(print))				-- function
print(type(type))				-- function
print(type(true))				-- boolean
print(type(nil))				-- nil
print(type(type(X)))			-- string

-- string
page = [[
<HTML>
<HEAD>
<TITLE>An HTML Page</TITLE>
</HEAD>
<BODY>
	<A HREF="http://www.lua.org">Lua</A>
</BODY>
</HTML>
]]

-- automatic conversions between numbers and strings
print("10" + 1)		-- 11
print("10 + 1")		-- 10 + 1
print("2" * "3")	-- 6
print("hello" + 1)	-- error (cannot convert "hello")
print(10 .. 20)		-- 1020

line = io.read()
n = tonumber(line)

print(tostring(10) == "10")		-- true
print(10 .. "" == "10")			-- true

-- table: associative array
-- lua uses table to represent a package, io.read means "the read entry from the io package", for lua it means "index the table io using the string "read" as the key"
t = {}			-- create a table and stroe its reference in a
print(t["x"])	-- nil

-- a table is always anonymous, there is no relationship between a varialbe that holds a table and the table itself

--
t = {}
t.x = 10		-- same as t["x"] = 10
print(t.x)		-- same as print(t["x"])

--
t = {}
x = "y"
t[x] = 10		-- put 10 in field "y"
print(t[x])		-- 10
print(t.x)		-- nil
print(t.y)		-- 10

--
a = {}
for i=1,10 do
	a[i] = io.read()
end

for i,line in ipairs(a) do
	print(line)
end

-- function
-- functions are first-class values in lua

-- userdata
-- userdata type allows arbitary C data to be stored in lua variables, userdata are used to represent new types created by an application program or a library written in C

-- arithmetic operators
--[[
+
-
*
/
-(negation)
]]

-- relational operators
--[[
<
>
<=
>=
==
~=
]]

-- logical operators
--[[
and
or
not
]]

-- string concatenation
print("hello " .. "world")		--> hello world
print(0 .. 1)					--> 01

-- table constructors
days = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"}
print(days[1])	--> "Sunday"

a = {x=0, y=0}
--> equivalent to
a = {}
a.x = 0
a.y = 0


--
w = {x=0, y=0, label="console"}
x = {sin(0), sin(1), sin(2)}
w[1] = "another field"
x.f = w
print(w["x"])		--> 0
print(w[1])			--> another field
print(x.f[1])		--> another field
w.x = nil			-- remove field "x"

--
polyline = {
	color="blue", thickness=2, npoints=4,
	{x=0,   y=0},
	{x=-10, y=0},
	{x=-10, y=1},
	{x=0,   y=1}
}
print(polyline[2].x)		--> -10

t = {x=0, y=0}
--> equals to
t = {["x"]=0, ["y"]=0}

t = {"red", "green", "blue"}
--> equals to
t = {[1]="red", [2]="green", [3]="blue"}

-- array start at 0 ("Monday" naturally goes to index 1)
days = {[0]="Sunday", "Monday", "Tuesday", "Wednesday",
		"Thursday", "Friday", "Saturday"}

a = {[1]="red", [2]="green", [3]="blue",}	-- trailing commas are optional but are always valid

-- you can use a semicolon instead of a comma in a constructor
-- semicolons are usually used to delimit different sections in a constructor
t = {x=10, y=45; "one", "two", "three"}

-- assignment
x, y = 0, 0
a[i], a[j] = a[j], a[i]		-- swap

-- automatic adjust the number of values to the number of variables
a, b, c = 0, 1
print(a, b, c)			-- 0  1  nil
a, b = a+1, b+1, b+2	-- value of b+2 is ignored
print(a, b)				-- 1  2
a, b, c = 0
print(a, b, c)			-- 0  nil  nil
a, b = f()

-- local variables and blocks
j = 10			-- global variable
local i = 1		-- local variable

--
x = 10
local i = 1			-- local to the chunk

while i<=x do
	local x = i*2	-- local to the while body
	print(x)
	i = i + 1
end

if i > 20 then
	local x			-- local to the "then" body
	x = 20
	print(x + 2)
else
	print(x)		--> 10 (the global one)
end

print(x)			--> 1- (the global one)

-- creates a local variable, foo, and initializes it with the value of the global variable foo
local foo = foo

-- explicitly delimit a block
do
	local a2 = 2*a
	local d = sqrt(b^2 - 4*a*c)
	x1 = (-b + d)/a2
	x2 = (-b - d)/a2
end							-- scope of a2 and d ends here
print(x1, x2)


-- control structures
-- if
if op == "+" then
	r = a + b
elseif op == "-" then
	r = a - b
elseif op == "*" then
	r = a * b
elseif op == "/" then
	r = a / b
else
	error("invalid operation")
end

-- while
local i = 1
while a[i] do
	print(a[i])
	i = i + 1
end

-- repeat
-- print the first non-empty line
repeat
	line = io.read()
until line ~= ""
print(line)

-- for
for i=10,1,-1 do
	print(i)
end

-- never change the value of the control variable

-- find a value in a list
local found = nil
for i=1,a.n do
	if a[i] == value then
		found = i
		break
	end
end
print(found)

--
days = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"}

for k in pairs(days) do
	print(k)
end

for i,v in ipairs(days) do
	print(i, v)
end

revDays = {}
for i,v in ipairs(days) do
	revDays[v] = i
end

-- break, return
a = {1, 2, 3}

local i = 1
local v = 2
while a[i] do
	if a[i] == v then
		break
	end
	i = i + 1
end
print(i)

--
function foo()
	do
		return
	end
end

-- functions
-- If the function has one single argument and this argument is either a literal string or a table constructor, then the parentheses are optional
print "hello world"		--> print("hello world")
dofile 'a.lua'			--> dofile('a.lua')
print [[a multi-line
 message
]]
f{x=10, y=20}			--> f({x=10, y=20})
type{}					--> type({})

-- syntax for object-oriented calls
o:foo(x)
--> equals to
o.foo(o, x)

-- function parameters work exactly as local varialbes
-- you can call a function with a number of arguments different from its number of parameters

function f(a, b)
	return a or b
end

f(3)			-- a = 3, b = nil
f(3, 4)			-- a = 3, b = 4
f(3, 4, 5)		-- a = 3, b = 4 (5 is discarded)

-- default argument
function incCount(n)
	n = n or 1
	count = count + n
end

-- functions with multiple results
function foo0()
end

function foo1()
	return 'a'
end

function foo2()
	return 'a'. 'b'
end

x,y = foo2()		-- x='a', y='b'
x= foo2()			-- x='a', 'b' is discarded
x,y,z = 10,foo2()	-- x=10, y='a', z='b'

x,y = foo0()		-- x=nil, y=nil
x,y= foo1()			-- x='a', y=nil
x,y,z = foo2()		-- x=10, y='a', z=nil

-- a function call that is not the last element in the list always produces one result
x,y = foo2(), 20			-- x='a', y=20
x,y = foo2(), 20, 30		-- x=nil, y=20, 30 is discarded

-- When a function call is the last (or the only) argument to another call, all results from the first call go as arguments
print(foo0())			-->
print(foo1())			-->  a
print(foo2())			-->  a   b
print(foo2(), 1)		-->  a   1
print(foo2() .. "x")	-->  ax (when the function call appears inside an expression, lua adjust the number of results to one)


-- a constructor also collects all results from a call, without any adjustments
a = {foo0()}			-- a = {}  (an empty table)
a = {foo1()}			-- a = {'a'}
a = {foo2()}			-- a = {'a', 'b'}

-- a statement like return f() returns all values returned by f
function foo (i)
  if i == 0 then return foo0()
  elseif i == 1 then return foo1()
  elseif i == 2 then return foo2()
  end
end

print(foo(1))		--> a
print(foo(2))		--> a  b
print(foo(0))		-- (no results)
print(foo(3))		-- (no results)

-- force a call to return exactly one result by enclosing it in an extra pair of parentheses
print((foo0()))		--> nil
print((foo1()))		--> a
print((foo2()))		--> a

-- unpack receives an array and returns as results all elements from the array, starting from index 1
print(unpack{10,20,30})		--> 10   20   30
a,b = unpack{10,20,30}		-- a=10, b=20, 30 is discarded

-- variable number of arguments
-- 
printResult = ""

function print(...)
	for i,v in ipairs(arg) do
		printResult = printResult .. tostring(v) .. "\t"
	end
	printResult = printResult .. "\n"
end

-- ... indicate that the function has a variable number of arguments
-- when function is called, all its arguments are collected in a single table, which the function accesses as a hidden parameter named arg
-- besides those arguments, the arg table has an extra field, n, with the actual number of arguments collected

-- select a specific return from a function
function select(n, ...)
	return arg[n]
end

--
function fwrite(fmt, ...)
	return io.write(string.format(fmt, unpack(arg)))
end

-- named arguments
-- lua has no direct support for named arguments
w = Window{
  x = 0,
  y = 0,
  width = 300,
  height = 200,
  title = "Lua",
  background = "blue",
  border = true,
}

function Window(options)
  -- check mandatory fields
  if type(options.title) ~= "string" then
    error("no title")
  elseif type(options.width) ~= "number" then
    error("no width")
  elseif type(options.height) ~= "number" then
    error("no height")
  end

  -- default values
  _Window(options.title,
          options.x or 0,
          options.y or 0,
          options.width,
          options.height,
          options.background or "white",
          options.border  -- default is nil if not given
  )
end

-- example
-- define a function with “named arguments” via table
function make_person(options)
  local name = options.name
  local age = options.age or 0        -- default age = 0
  local hobby = options.hobby or "none"
  if type(name) ~= "string" then
    error("must provide name")
  end
  return {
    name = name,
    age = age,
    hobby = hobby
  }
end

-- usage:
local p = make_person{ name = "Alice", hobby = "reading" }
-- p => { name = "Alice", age = 0, hobby = "reading" }

-- functions are anonymous
a = {p = print}
a.p("hello world")		--> hello world

function foo(x)
	return 2*x
end
--> is syntactic sugar for
foo = function (x)
	return 2*x
end

--
table.sort(network, function (a,b)
  return (a.name > b.name)
end)

-- closures

```

## Basics
```lua
-- 8 basic types in Lua: 
    nil
    boolean
    number
    string
    function
    userdata
    thread
    table

-- multiline string
users_csv = [[
  Tom,15
  Jack,16
  ...
]]

-- multiple assignment
x, y = 0, 0

-- #1 for loop
math.randomseed(os.time())

-- for c = 1, 10, 2 do
for c = 1, 10 do
  enemy_x = math.random(0, 800)
  enemy_y = math.random(0, 600)

  print("Enemy " .. c .. ": (" .. enemy_x .. ", " .. enemy_y .. ")")
end

-- #2 while loop
math.randomseed(os.time())

player_x, player_y = 400, 300

num_enemies = 0

while num_enemies < 10 do
  enemy_x = math.random(0, 800)
  enemy_y = math.random(0, 600)

  if (player_x == enemy_x) and (player_y == enemy_y) then
    print("Enemy and player crashed!")
  else
    print("Enemy " .. (num_enemies + 1) .. ": (" .. enemy_x .. ", " .. enemy_y .. ")")
    num_enemies = num_enemies + 1
  end
end

-- #3 variable scope
math.randomseed(os.time())

local player_x, player_y = 400, 300

local num_enemies = 0

while num_enemies < 10 do
  local enemy_x = math.random(0, 800)
  local enemy_y = math.random(0, 600)

  if (player_x == enemy_x) and (player_y == enemy_y) then
  print("Enemy and player crashed!")
  else
  print("Enemy " .. (num_enemies + 1) .. ": (" .. enemy_x .. ", " .. enemy_y .. ")")
  num_enemies = num_enemies + 1
  end
end

-- #4 input
math.randomseed(os.time())

local player_x, player_y = 400, 300

local enemy_x, enemy_y = 0, 0

local user_option = 0

while user_option ~= 4 do
-- display a menu on the console
  print("+---------------------------------+")
  print("| Welcome " .. os.date())
  print("+---------------------------------+")
  print("| 1.Generate random enemy position")
  print("| 2.Distance from enemy to player")
  print("| 3.Get angle from enemy to player")
  print("| 4.Exit")
  print("+---------------------------------+")

  print("Please select your option: ")
  user_option = io.read("*n")

  if user_option == 1 then
    enemy_x = math.random(0, 800)
    enemy_y = math.random(0, 600)
    print("Enemy position: (" .. enemy_x .. ", " .. enemy_y .. ")")
  end

  if user_option == 2 then
    local d = math.sqrt((enemy_x - player_x)^2 + (enemy_y - player_y)^2)
    print("Distance from enemy to player: " .. d)
  end

  if user_option == 3 then
    local a = math.atan2(player_y - enemy_y, player_x - enemy_x)
    local a_deg = math.deg(a)
    print("Angle from enemy to player: " .. a_deg .. " degrees")
  end
end

print("Exiting the program. Goodbye!")

-- #5 repeat until
local sum_scores = 0

local num_scores = 0

repeat
  print("Enter a score (negative to quit): ")
  local score = io.read("*n")

  if score >= 0 then
    sum_scores = sum_scores + score
    num_scores = num_scores + 1
  end
until score < 0

local avg_score = sum_scores / num_scores

print("Average of all "..num_scores.." values is "..avg_score)

-- #6 functions
function display_menu()
  print("+---------------------------------+")
  print("| Welcome " .. os.date())
  print("+---------------------------------+")
  print("| 1.Generate random enemy position")
  print("| 2.Distance from enemy to player")
  print("| 3.Get angle from enemy to player")
  print("| 4.Exit")
  print("+---------------------------------+")
end

function get_distance(x1, y1, x2, y2)
  return math.sqrt((x2 - x1)^2 + (y2 - y1)^2)
end

math.randomseed(os.time())

local player_x, player_y = 400, 300

local enemy_x, enemy_y = 0, 0

local user_option = 0

while user_option ~= 4 do
  display_menu()

  print("Please select your option: ")
  user_option = io.read("*n")

  if user_option == 1 then
    enemy_x = math.random(0, 800)
    enemy_y = math.random(0, 600)
    print("Enemy position: (" .. enemy_x .. ", " .. enemy_y .. ")")
  end

  if user_option == 2 then
    local d = get_distance(enemy_x, enemy_y, player_x, player_y)
    print("Distance from enemy to player: " .. d)
  end

  if user_option == 3 then
    local a = math.atan2(player_y - enemy_y, player_x - enemy_x)
    local a_deg = math.deg(a)
    print("Angle from enemy to player: " .. a_deg .. " degrees")
  end
end

print("Exiting the program. Goodbye!")

-- #7 table
-- table as array
local scores = {88.1, 92.3, 87.4}
print(scores[1])
scores[4] = 99.1

-- ipairs returns index-value pairs, in order
for k,v in ipairs(scores) do
  print("key: "..k..", value: "..v)
end

-- table as key-value
-- local scores = {["miles"] = 88.1, ["john"] = 92.3, ["nina"] = 87.4}
-- print(scores["miles"])
-- scores["oscar"] = 99.1

local scores = {miles = 88.1, john = 92.3, nina = 87.4}
print(scores.miles)
scores.oscar = 99.1

-- pairs returns key-value pairs, without order
for k,v in pairs(scores) do
  print("key: "..k..", value: "..v)
end

--
function display_menu()
  print("+---------------------------------+")
  print("| Welcome " .. os.date())
  print("+---------------------------------+")
  print("| 1.Generate random enemy position")
  print("| 2.Distance from enemy to player")
  print("| 3.Get angle from enemy to player")
  print("| 4.Exit")
  print("+---------------------------------+")
end

function get_distance(x1, y1, x2, y2)
  return math.sqrt((x2 - x1)^2 + (y2 - y1)^2)
end

math.randomseed(os.time())

-- local player_x, player_y = 400, 300
local player = {x = 400, y = 300}

-- local enemy_x, enemy_y = , 
local enemy= {x = 0, y = 0}

local user_option = 0

while user_option ~= 4 do
  display_menu()

  print("Please select your option: ")
  user_option = io.read("*n")

  if user_option == 1 then
--    enemy_x = math.random(0, 800)
--    enemy_y = math.random(0, 600)
--    print("Enemy position: (" .. enemy_x .. ", " .. enemy_y .. ")")
    enemy.x = math.random(0, 800)
    enemy.y = math.random(0, 600)
    print("Enemy position: (" .. enemy.x .. ", " .. enemy.y .. ")")
  end

  if user_option == 2 then
--     local d = get_distance(enemy_x, enemy_y, player_x, player_y)
    local d = get_distance(enemy.x, enemy.y, player.x, player.y)
    print("Distance from enemy to player: " .. d)
  end

  if user_option == 3 then
--    local a = math.atan2(player_y - enemy_y, player_x - enemy_x)
    local a = math.atan2(player.y - enemy.y, player.x - enemy.x)
--    local a_deg = math.deg(a)
--    print("Angle from enemy to player: " .. a_deg .. " degrees")
    print("Angle from enemy to player: " .. math.deg(a) .. " degrees")
  end
end

print("Exiting the program. Goodbye!")

-- 
local prince_codes = {
  { Page = 1, Line = 2, Word = 2, Code = "W" },
  { Page = 1, Line = 5, Word = 4, Code = "O" },
  { Page = 1, Line = 8, Word = 6, Code = "E" },
  { Page = 1, Line = 10, Word = 6, Code = "S" },
  { Page = 2, Line = 2, Word = 5, Code = "P" },
  { Page = 2, Line = 3, Word = 8, Code = "B" },
  { Page = 2, Line = 6, Word = 6, Code = "Y" },
  { Page = 2, Line = 1, Word = 2, Code = "S" },
  { Page = 3, Line = 4, Word = 9, Code = "K" },
  { Page = 3, Line = 5, Word = 1, Code = "J" },
  { Page = 3, Line = 6, Word = 3, Code = "T" },
  { Page = 3, Line = 1, Word = 7, Code = "B" }
}

print("What is the Page you are looking for?")
local page = io.read("*n")

print("What is the Line you are looking for?")
local line = io.read("*n")

print("What is the Word you are looking for?")
local word = io.read("*n")

local found = false

for _, entry in ipairs(prince_codes) do
  if entry.Page == page and entry.Line == line and entry.Word == word then
    print("The code for this combination is: " .. entry.Code)
    found = true
  end
end

if not found then
  print("Sorry, no code found for this combination.")
end

-- table as configuration file
Level1 = {
    -------------------------------------------
    -- table to define the map config variables
    -------------------------------------------
    map = {
        textureAssetId = mapTextureAssetId,
        file = "./assets/tilemaps/jungle.map",
        scale = 2,
        tileSize = 32,
        mapSizeX = 25,
        mapSizeY = 20
    }

    -------------------------------------------
    -- table to define the map config variables
    -------------------------------------------
    entities = {
        [0] = {
            name = "player",
            layer = 4,
            components = {
                transform = {
                    position = {
                        x = 240,
                        y = 106
                    },
                    velocity = {
                        x = 0,
                        y = 0
                    }
                    width = 32,
                    height = 32,
                    scale = 1,
                    rotation = 0
                },
                spirite = {
                    textureAssetId = "chopper-texture",
                    animated = true,
                    frameCount = 2,
                    animationSpeed = 90,
                    hasDirections = true,
                    fixed = false
                },
                collider = {
                    tag = "PLAYER"
                },
                input = {
                    keyboard = {
                        up = "w",
                        left = "a",
                        down = "s",
                        right = "d",
                        shoot = "space"
                    }
                }
            }
        },
        [1] = {
            name = "start",
            layer = 3,
            components = {
                transform = {
                    position = {
                        x = 240,
                        y = 115
                    },
                    velocity = {
                        x = 0,
                        y = 0
                    }
                    width = 32,
                    height = 32,
                    scale = 1,
                    rotation = 0
                },
                spirite = {
                    textureAssetId = "start-texture",
                    animated = false
                }
            }
        },
        [2] = {
            name = "heliport",
            layer = 3,
            components = {
                transform = {
                    position = {
                        x = 1395,
                        y = 495
                    },
                    velocity = {
                        x = 0,
                        y = 0
                    }
                    width = 32,
                    height = 32,
                    scale = 1,
                    rotation = 0
                },
                spirite = {
                    textureAssetId = "heliport-texture",
                    animated = false
                },
                collider = {
                    tag = "LEVEL_COMPLETE"
                }
            }
        },
        -- ......
    }
}

-- table as matrices
local M = {
  { 3.4, 2.0, 2.0 },
  { -3.5, 3.3, 0.5 },
  { 0.1, 0.1, 3.3 }
}

print(M[1][1])
print(M[2][2])
print(M[3][3])

--
local mat = {}

N = 3
M = 3

for i = 1, N do
  mat[i] = {}
  for j = 1, M do
    mat[i][j] = i * j
  end
end

print(mat[1][1])
print(mat[2][2])
print(mat[3][3])

-- Tic-Tac-Toe
---------------------------------------------
-- create new table to hold the board matrix
---------------------------------------------
local board = {}

---------------------------------------------
-- clear the board table
---------------------------------------------
local function clear_board()
  for i = 1, 3 do
    board[i] = {}
    for j = 1, 3 do
      board[i][j] = " "
    end
  end
end

---------------------------------------------
-- display the board table
---------------------------------------------
local function display_board()
  print("\n    1   2   3")
  for i = 1, 3 do
    io.write(i.." ")
    for j = 1, 3 do
      io.write("["..board[i][j].."] ")
    end
    print()
  end
  print()
end

---------------------------------------------
-- is board full
---------------------------------------------
local function board_full()
  for i = 1, 3 do
    for j = 1, 3 do
      if board[i][j] == " " then
        return false
      end
    end
  end
  return true
end

---------------------------------------------
-- check and return the winner
---------------------------------------------
local function check_winner()
  -- rows
  for i = 1, 3 do
    if board[i][1] ~= " " and board[i][1] == board[i][2] and board[i][2] == board[i][3] then
      return board[i][1]
    end
  end
  -- columns
  for j = 1, 3 do
    if board[1][j] ~= " " and board[1][j] == board[2][j] and board[2][j] == board[3][j] then
      return board[1][j]
    end
  end
  -- diagonals
  if board[1][1] ~= " " and board[1][1] == board[2][2] and board[2][2] == board[3][3] then
    return board[1][1]
  end
  if board[1][3] ~= " " and board[1][3] == board[2][2] and board[2][2] == board[3][1] then
    return board[1][3]
  end

  return nil
end

---------------------------------------------
-- switch current player
---------------------------------------------
local function switch_player(player)
  if player == "X" then
    return "O"
  else
    return "X"
  end
end

---------------------------------------------
-- single game session
---------------------------------------------
local function play_game()
  clear_board()
  local player = "X"
  local game_over = false

  while not game_over do
    display_board()

    print("Player "..player..", enter your move (row and column): ")

    io.write("Row (1-3): ")
    local row = io.read("*n")

    io.write("Column (1-3): ")
    local col = io.read("*n")

    -- flush newline left in input buffer
    io.read("*l")

    -- validate input
    if row and col and row >= 1 and row <= 3 and col >= 1 and col <= 3 and board[row][col] == " " then
      board[row][col] = player

      -- check winner
      local winner = check_winner()
      if winner then
        display_board()
        print("🎉 Player "..winner.." wins!")
        game_over = true
      elseif board_full() then
        display_board()
        print("🤝 It's a draw!")
        game_over = true
      else
        player = switch_player(player)
      end
    else
      print("❌ Invalid move, try again.")
    end
  end
end

---------------------------------------------
-- replay loop
---------------------------------------------
while true do
  play_game()
  io.write("Play again? (y/n): ")
  local answer = io.read("*l")
  if answer:lower() ~= "y" then
    print("👋 Thanks for playing Tic-Tac-Toe!")
    break
  end
end

-- #8 metatables
local meta = {}
local vector3d = {}

---------------------------------------------
-- Declare a new vector3d constructor
---------------------------------------------
function vector3d.new(x, y, z)
  local v = {x = x, y = y, z = z}
  setmetatable(v, meta)
  return v
end

function vector3d.add(v1, v2)
  return vector3d.new(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z)
end
meta.__add = vector3d.add

function vector3d.tostring(v)
  return "("..v.x..", "..v.y..", "..v.z..")"
end
meta.__tostring = vector3d.tostring

---------------------------------------------
-- Create 2 vector3d instances
---------------------------------------------
local position = vector3d.new(10, 20, 30)
local velocity = vector3d.new(1, 2, 3)

local result = velocity + position

print(position)   -- prints (10, 20, 30)
print(velocity)   -- prints (1, 2, 3)
print(result)     -- prints (11, 22, 33)
print("Result vector is "..tostring(result)) -- uses __tostring → (11, 22, 33)

-- #9 OOP
-- Define class table
local Fighter = {
  name = "",
  health = 0,
  speed = 0
}
Fighter.__index = Fighter  -- Set the metatable for the class (modern style)

---------------------------------------------
-- Declare the class methods
---------------------------------------------
function Fighter:light_punch()
  print("Fighter "..self.name.." performs a light punch!")
end

function Fighter:heavy_punch()
  print("Fighter "..self.name.." performs a heavy punch!")
end

function Fighter:light_kick()
  print("Fighter "..self.name.." performs a light kick!")
end

function Fighter:heavy_kick()
  print("Fighter "..self.name.." performs a heavy kick!")
end

function Fighter:special_attach()
  print("Fighter "..self.name.." performs a special attach!")
end

---------------------------------------------
-- Declare the class constrictor
---------------------------------------------
function Fighter:new(t)
  t = t or {}
  setmetatable(t, self)    -- instance uses Fighter as its metatable
  -- self.__index = self   -- old style, not safe
  return t
end

---------------------------------------------
-- Create 2 Fighter instances
---------------------------------------------
--[[
local blanka = Fighter:new({
  name = "Blanka",
  health = 100,
  speed = 80
})
print("Object "..blanka.name.." was created")

local chun_li = Fighter:new({
  name = "Chun Li",
  health = 100,
  speed = 90
})
print("Object "..chun_li.name.." was created")
]]

local blanka = Fighter:new{
  name = "Blanka",
  health = 100,
  speed = 80
}
print("Object "..blanka.name.." was created")

local chun_li = Fighter:new{
  name = "Chun Li",
  health = 100,
  speed = 90
}
print("Object "..chun_li.name.." was created")

---------------------------------------------
-- Call object methods
---------------------------------------------
blanka:light_punch()
blanka:heavy_kick()
blanka:special_attach()

chun_li:light_kick()
chun_li:heavy_punch()
chun_li:special_attach()

-- functions as first-class values (higher-order functions)
--
local students = {
  {name = "Alice", age = 20, grade = 90},
  {name = "Bob", age = 22, grade = 85},
  {name = "Charlie", age = 21, grade = 95}
}

local s = table.sort(students, function(a, b)
  return a.grade > b.grade
end)

for i, student in ipairs(students) do
  print(i, student.name, student.age, student.grade)
end

-- closure
local function add1(a)
	local c = 1
	local function add(b)
		return a + b + c
	end
	return add
end

local add6 = add1(5)
print(add6(3))  -- should print 9

-- variadic functions
local function find_max(...)
	local n = 0
	local args = {...}  -- pack all arguments into a table
	local max = args[1]
	for i, v in ipairs(args) do
		if v > max then
			max = v
		end
		n = n + 1
	end
	return n, max
end

local n, max = find_max(3, 5, 2, 8, 1)
print("The maximum value is "..max.." among "..n.." numbers")

```

## integrate lua with c/c++
**folder structure:**
```
lua-c
	Makefile
	lib
		lua         -- lua source code(curl -R -O https://www.lua.org/ftp/lua-5.3.5.tar.gz)
			src
	scripts
		dofile.lua
        factorial.lua
        getvar.lua
        native_factorial.lua
        stack.lua
        userdata.lua
        configtable.lua
	src
		main.c
```
**file content:**

```Makefile
# Makefile
LUA_SRC = $(filter-out ./lib/lua/src/lua.c ./lib/lua/src/luac.c, $(wildcard ./lib/lua/src/*.c))
APP_SRC = ./src/main.c

build:
	gcc -std=c99 -Wall $(LUA_SRC) $(APP_SRC) -lm -o main

clean:
	rm -f ./main

run:
	./main
```

**lua files**
```lua
-- dofile.lua
print("Hello from dofile.lua")

-- factorial.lua
function factorial(n)
  if n == 0 then
    return 1
  else
    return n * factorial(n - 1)
  end
end

-- getvar.lua
somevar = 42


-- native_factorial.lua
local num = 6
print("Factorial of " .. num .. " is: " .. native_factorial(num))

-- stack.lua
-- Not needed (stack is handled in C directly)

-- userdata.lua
square = create_rectangle(5, 5)

change_rectangle_dimensions(square, 10, 10)

-- configtable.lua
config_table = {
	window_width = 800,
	window_height = 600,
	num_enemies = 5,
	num_levels = 10
}
```

**c files**
```c
// main.c
#include <stdio.h>
#include "../lib/lua/src/lua.h"
#include "../lib/lua/src/lualib.h"
#include "../lib/lua/src/lauxlib.h"

void lua_example_dofile(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dofile(L, "./scripts/dofile.lua") != LUA_OK) {
        const char *error_msg = lua_tostring(L, -1);
        fprintf(stderr, "Error: %s\n", error_msg);
        lua_pop(L, 1);
    }

    lua_close(L);
}

void lua_example_getvar(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    // Run getvar.lua which sets somevar
    if (luaL_dofile(L, "./scripts/getvar.lua") != LUA_OK) {
        const char *error_msg = lua_tostring(L, -1);
        fprintf(stderr, "Error: %s\n", error_msg);
        lua_pop(L, 1);
    }

    lua_getglobal(L, "somevar");
    if (lua_isnumber(L, -1)) {
        int somevar = (int)lua_tointeger(L, -1);
        printf("somevar: %d\n", somevar);
    }
    lua_pop(L, 1);

    lua_close(L);
}

void lua_example_stack(void) {
    lua_State *L = luaL_newstate();

    lua_pushnumber(L, 10);
    lua_pushnumber(L, 20);
    lua_pushnumber(L, 30);

    int n = lua_gettop(L);
    printf("Number of elements in stack: %d\n", n);

    while (n > 0) {
        if (lua_isnumber(L, n)) {
            double value = lua_tonumber(L, n);
            printf("Value at index %d: %f\n", n, value);
        }
        n--;
    }

    lua_pop(L, lua_gettop(L));
    lua_close(L);
}

void lua_example_call_lua_function(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dofile(L, "./scripts/factorial.lua") != LUA_OK) {
        const char *error_msg = lua_tostring(L, -1);
        fprintf(stderr, "Error: %s\n", error_msg);
        lua_pop(L, 1);
        lua_close(L);
        return;
    }

    lua_getglobal(L, "factorial");
    if (!lua_isfunction(L, -1)) {
        fprintf(stderr, "'factorial' is not a function\n");
        lua_pop(L, 1);
        lua_close(L);
        return;
    }

    int num = 5;
    lua_pushinteger(L, num);

    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        const char *error_msg = lua_tostring(L, -1);
        fprintf(stderr, "Error calling 'factorial': %s\n", error_msg);
        lua_pop(L, 1);
        lua_close(L);
        return;
    }

    if (lua_isnumber(L, -1)) {
        int result = (int)lua_tointeger(L, -1);
        printf("Factorial of %d is: %d\n", num, result);
    }

    lua_pop(L, 1);
    lua_close(L);
}

int factorial(int n) {
    if (n == 0) return 1;
    return n * factorial(n - 1);
}

int native_factorial(lua_State *L) {
    int n = (int)lua_tointeger(L, -1);
    int result = factorial(n);
    lua_pushinteger(L, result);
    return 1;
}

void lua_example_lua_call_c_function(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "native_factorial", native_factorial);

    if (luaL_dofile(L, "./scripts/native_factorial.lua") != LUA_OK) {
        const char *error_msg = lua_tostring(L, -1);
        fprintf(stderr, "Error: %s\n", error_msg);
        lua_pop(L, 1);
    }

    lua_close(L);
}


struct rectangle {
    double length;
    double width;
};

int create_rectangle(lua_State *L) {
    double length = luaL_checknumber(L, 1);
    double width = luaL_checknumber(L, 2);

    struct rectangle *rect = (struct rectangle *)lua_newuserdata(L, sizeof(struct rectangle));
    rect->length = length;
    rect->width = width;

    printf("Created rectangle of length: %f and width: %f\n", length, width);

    luaL_getmetatable(L, "RectangleMeta");
    lua_setmetatable(L, -2);

    return 1;
}

int change_rectangle_dimensions(lua_State *L) {
    struct rectangle *rect = (struct rectangle *)luaL_checkudata(L, 1, "RectangleMeta");
    double new_length = luaL_checknumber(L, 2);
    double new_width = luaL_checknumber(L, 3);

    rect->length = new_length;
    rect->width = new_width;

    printf("Changed rectangle dimensions to length: %f and width: %f\n", new_length, new_width);

    return 0;
}

void lua_example_userdata(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    luaL_newmetatable(L, "RectangleMeta");

    lua_pushstring(L, "__index");
    lua_newtable(L);
    lua_settable(L, -3);

    lua_pop(L, 1);

    lua_register(L, "create_rectangle", create_rectangle);
    lua_register(L, "change_rectangle_dimensions", change_rectangle_dimensions);

    if (luaL_dofile(L, "./scripts/userdata.lua") != LUA_OK) {
        const char *error_msg = lua_tostring(L, -1);
        fprintf(stderr, "Error: %s\n", error_msg);
        lua_pop(L, 1);
    }

    lua_close(L);
}


void lua_example_load_configtable(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dofile(L, "./scripts/configtable.lua") != LUA_OK) {
        const char *error_msg = lua_tostring(L, -1);
        fprintf(stderr, "Error: %s\n", error_msg);
        lua_pop(L, 1);
        lua_close(L);
        return;
    }

    lua_getglobal(L, "config_table");
    if (!lua_istable(L, -1)) {
        fprintf(stderr, "'config_table' is not a table\n");
        lua_pop(L, 1);
        lua_close(L);
        return;
    }

    lua_getfield(L, -1, "window_width");
    int window_width = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "window_height");
    int window_height = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "num_enemies");
    int num_enemies = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "num_levels");
    int num_levels = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    printf("Config Table:\n");
    printf("Window Width: %d\n", window_width);
    printf("Window Height: %d\n", window_height);
    printf("Number of Enemies: %d\n", num_enemies);
    printf("Number of Levels: %d\n", num_levels);

    lua_pop(L, 1);
    lua_close(L);
}

int main() {
    lua_example_dofile();
    lua_example_getvar();
    lua_example_stack();
    lua_example_call_lua_function();
    lua_example_lua_call_c_function();
    lua_example_userdata();
    lua_example_load_configtable();

    return 0;
}
```

## lua examples
```lua
-- example 1
#!/usr/bin/env lua

-- Colors
local RED   = "\27[31m"
local GREEN = "\27[32m"
local BOLD  = "\27[1m"
local RESET = "\27[0m"

-- Config
local EVCNAME     = "evc-morris-dentist"
local SNMPNAME    = "snmp-evc-morris-dentist-1"
local DHCP_SERVER = "root@10.254.25.42"
local DHCP_SERVER_PASSWORD = "vecima@atc"
local MIB_DIR     = "/home/tcao/mibs"
local MIBTABLE_CMTSCMPTR   = "DOCS-IF-MIB:docsIfCmtsCmPtr"
local MIBTABLE_CMTSCMSTATUS = "DOCS-IF-MIB:docsIfCmtsCmStatusTable"

local NCS_MODEM_BRIEF_FILE = "/tmp/modem_brief.list"
local NCS_MODEM_IP_FILE    = "/tmp/modem_ip.list"
local SNMP_MIB_RESULT_FILE = "/tmp/modem_mib.result"

-- Run shell command and capture full output
local function run_cmd(cmd)
    local f = io.popen(cmd)
    local output = f:read("*a")
    f:close()
    return output
end

-- Save string to file
local function save_file(path, content)
    local f = io.open(path, "w")
    f:write(content)
    f:close()
end

-- Load lines from file
local function load_lines(path)
    local lines = {}
    for line in io.lines(path) do
        table.insert(lines, line)
    end
    return lines
end

-- Step 1: get modem brief info
local cli_cmd = string.format(
    "nomad alloc exec -task evc -job %s sh -c 'ncs_cli -u admin <<EOF\nshow cable modem brief | t\nEOF'",
    EVCNAME
)
local modem_brief = run_cmd(cli_cmd)
save_file(NCS_MODEM_BRIEF_FILE, modem_brief)

-- Step 2: get SNMP NSI port
local allocs = run_cmd("nomad job allocs " .. SNMPNAME)
local alloc_id = allocs:match("(%S+)%s+snmp")
local snmp_status = run_cmd("nomad alloc status " .. alloc_id .. " 2>/dev/null")
local SNMP_NSIPORT = snmp_status:match("snmp%-nsi%-port%s+(%S+)")

-- Step 3: run snmpwalks via ssh
local snmp_cmd = string.format(
    "sshpass -p %q ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null %s " ..
    "\"snmpwalk -On -v2c -c public %s %s -M %s; snmpwalk -On -v2c -c public %s %s -M %s\"",
    DHCP_SERVER_PASSWORD, DHCP_SERVER, SNMP_NSIPORT, MIBTABLE_CMTSCMPTR, MIB_DIR,
    SNMP_NSIPORT, MIBTABLE_CMTSCMSTATUS, MIB_DIR
)
local snmp_result = run_cmd(snmp_cmd)
save_file(SNMP_MIB_RESULT_FILE, snmp_result)

-- Step 4: emulate get_modem_ip.awk
local modem_ip_lines = {}
for _, line in ipairs(load_lines(NCS_MODEM_BRIEF_FILE)) do
    local mac, bracket = line:match("^(%x%x:%x%x:%x%x:%x%x:%x%x:%x%x).*(%b[])")
    if mac and bracket then
        table.insert(modem_ip_lines, mac .. " " .. bracket:sub(2, -2))
    else
        local mac2, ip = line:match("^(%x%x:%x%x:%x%x:%x%x:%x%x:%x%x)%s+%S+%s+%S+%s+%S+%s+(%S+)")
        if mac2 and ip then
            table.insert(modem_ip_lines, mac2 .. " " .. ip)
        end
    end
end
save_file(NCS_MODEM_IP_FILE, table.concat(modem_ip_lines, "\n"))

-- Step 5: load modem table
local modems = {}
for _, line in ipairs(load_lines(NCS_MODEM_IP_FILE)) do
    local mac, ip = line:match("^(%S+)%s+(%S+)")
    if mac then
        if ip:match("^%d+%.%d+%.%d+%.%d+$") then
            modems[mac] = ip
        else
            modems[mac] = "0.0.0.0"
        end
    end
end

-- Step 6: compare with SNMP results
local match, mismatch = 0, 0
for mac, ip in pairs(modems) do
    -- convert MAC -> decimal dotted
    local decmac = mac:gsub("(%x%x)", function(h) return tonumber(h, 16) end)
                       :gsub(":", ".")
    local snmp_result_lines = load_lines(SNMP_MIB_RESULT_FILE)
    local found = false
    for _, l in ipairs(snmp_result_lines) do
        if l:match("^" .. decmac .. " =") then
            found = true
            local snmp_key = l:match("= (%S+)")
            if snmp_key then
                for _, l2 in ipairs(snmp_result_lines) do
                    if l2:match("^" .. snmp_key .. " =") and l2:match("IpAddress") then
                        local snmp_ip = l2:match("(%d+%.%d+%.%d+%.%d+)$")
                        print(string.format("MAC: %s, IP: %s, SNMP IP: %s", mac, ip, snmp_ip))
                        if ip == snmp_ip then
                            match = match + 1
                        else
                            print(string.format("IP mismatch for MAC: %s, NCS IP: %s, SNMP IP: %s", mac, ip, snmp_ip))
                            mismatch = mismatch + 1
                        end
                    end
                end
            end
        end
    end
    if not found then
        print("No SNMP key for MAC: " .. mac)
    end
end

-- Step 7: summary
local COLOR = (mismatch ~= 0) and RED or GREEN
print(string.format("%s%sTotal modems: %d, IP matches: %d, IP mismatches: %d%s",
    BOLD, COLOR, #modem_ip_lines, match, mismatch, RESET))

```