--
-- app.lua
--
-- Framework/Caller role: only ever calls protocol.encode / protocol.decode.
-- encode_header / encode_payload / validate_header do not exist on the
-- returned table.
--

local protocol = require("protocol")

local msg = { type = 1, payload = "hello" }

print("== encode ==")
local bytes, encode_err = protocol.encode(msg)
if not bytes then
    print("encode failed: " .. tostring(encode_err))
    os.exit(1)
end

io.write("encoded " .. #bytes .. " bytes: ")
for i = 1, #bytes do
    io.write(string.format("%02X ", bytes:byte(i)))
end
print()

print()
print("== decode ==")
local decoded, decode_err = protocol.decode(bytes)
if not decoded then
    print("decode failed: " .. tostring(decode_err))
    os.exit(1)
end
print(string.format("decoded type=%d payload=%q", decoded.type, decoded.payload))

print()
print("== round-trip assertion ==")
assert(decoded.type == msg.type, "type mismatch after round-trip")
assert(decoded.payload == msg.payload, "payload mismatch after round-trip")
print("round-trip OK: decode(encode(msg)) == msg")

print()
print("== decode: failure path (truncated buffer) ==")
local truncated = bytes:sub(1, 3)
local bad, bad_err = protocol.decode(truncated)
print("bad, err ->", bad, bad_err)

print()
print("== decode: failure path (declared length larger than buffer) ==")
local corrupt = string.pack(">BI4", 1, 999) .. "hi"
local bad2, bad2_err = protocol.decode(corrupt)
print("bad2, err ->", bad2, bad2_err)

-- Only M.encode / M.decode exist on the returned table. Uncommenting the
-- line below would fail with "attempt to call a nil value (field
-- 'encode_header')" -- the wire-format internals are not reachable:
--
-- protocol.encode_header(msg)
