[**Lua Documentation**](https://www.lua.org/docs.html)  

[The Standard Libraries](https://www.lua.org/manual/5.4/manual.html#6)  

[**Programming in Lua**](https://www.lua.org/pil/contents.html)  

[**Lua Directory**](http://lua-users.org/wiki/LuaDirectory)  

[**LuaRocks**](https://luarocks.org/) # package manager  

[The Lua Tutorial](https://dev.to/jd2r/the-lua-tutorial-544b)  

[Lua online](https://www.lua.org/demo.html)  
[Lua for Programmers](https://ebens.me/posts/lua-for-programmers-part-1/)  
[lua-style-guide](https://github.com/luarocks/lua-style-guide)  
[]()  
[Lua Programming Gems](https://www.lua.org/gems/)  
[Lua Style Guide](https://roblox.github.io/lua-style-guide/)  

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

-- curl -R -O https://www.lua.org/ftp/lua-5.3.5.tar.gz
--[[
folder structure:
lua-c
	Makefile
	lib
		lua         -- lua source code
			src
	scripts
		myscript.lua
	src
		main.c
]]

```

## integrate lua with c/c++
- folder structure:
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
- file content:
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