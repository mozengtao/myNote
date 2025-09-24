[**Lua Documentation**](https://www.lua.org/docs.html)  

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

-- #7
-- #8
-- #9

```