# 03 · Example 3：Protocol Encoder

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)。代码位于 [`code/03-protocol-encoder/`](code/03-protocol-encoder/)。

## 1. Problem / Scenario

需要把一个 Lua 表 `{type=1, payload="hello"}` 编码成一段二进制字节串（走网络、写文件都可能需要这种格式),再能从字节串解码回原来的表,并且解码结果要和原始输入完全一致（round-trip）。

这是 Pattern 2 最能体现"隐藏内部步骤"价值的场景：字节串里"第几个字节是什么字段"这件事——**wire format（线上格式）**——几乎注定会随着协议演进而改变（今天用 1 字节存类型、4 字节存长度,明天可能需要把长度字段从 32 位扩到 64 位,或者多加一个版本号字节)。只要 `encode()`/`decode()` 的函数签名保持不变,内部 wire format 怎么变都不应该影响任何调用者。

## 2. Architecture

```text
                     app.lua (Caller)
                            |
                            | require("protocol")
                            v
                    +----------------------+
                    |   protocol.lua       |
                    |                      |
                    |  Public API          |
                    |  M.encode()          |
                    |  M.decode()          |
                    |       |              |
                    |       v              |
                    |  Private Impl        |
                    |  encode_header()     |
                    |  encode_payload()    |
                    |  validate_header()   |
                    +----------------------+
```

当前 wire format（这条信息只存在于 `protocol.lua` 内部,`app.lua` 从未依赖它）：

```text
byte 1     byte 2-5              byte 6..
+------+  +------------------+  +---------------------+
| type |  | payload_len (be) |  | payload (raw bytes) |
+------+  +------------------+  +---------------------+
```

## 3. Framework / Caller

`app.lua` 构造 `{type=1, payload="hello"}`,调用 `protocol.encode` 得到字节串并以十六进制打印;再调用 `protocol.decode` 把字节串还原成表,断言 `decoded.type`/`decoded.payload` 和原始输入完全一致;最后演示两种解码失败路径——缓冲区被截断、声明的 `payload_len` 超出实际可用字节数。`app.lua` 从头到尾不知道字节的具体布局,只知道"传一个表进去,能拿回一个表来"。

## 4. Module

```text
Public API（挂在 M 上）
    M.encode(msg)    -- 校验 msg 形状 -> 调用两个私有步骤 -> 拼接字节串
    M.decode(bytes)  -- 校验 bytes 形状 -> unpack 字段 -> 校验字段合法性 -> 切出 payload

Private Implementation（local function）
    encode_header(msg)                     -- 把 type/payload 长度打包成 header 字节
    encode_payload(msg)                     -- 目前只是原样返回 payload（预留未来扩展点）
    validate_header(msg_type, len, bytes)   -- 校验已解出的字段是否合理（不是重新校验类型）
```

## 5. Complete Lua Code

`code/03-protocol-encoder/protocol.lua`：

```lua
--
-- protocol.lua
--
-- Pattern 2: Private + Public Module (Encapsulation)
--
-- Public API : encode(msg), decode(bytes)
-- Private impl: encode_header(msg), encode_payload(msg), validate_header(header)
--
-- Wire format (current version):
--   byte 1     : type       (unsigned 8-bit)
--   bytes 2-5  : payload_len (unsigned 32-bit, big-endian)
--   bytes 6..  : payload     (raw bytes, `payload_len` long)
--
-- Callers only ever call M.encode(msg) / M.decode(bytes). They never see
-- "byte 1 is type, bytes 2-5 are a length" -- that layout is exactly the
-- kind of internal decision this pattern hides. If the header grows from
-- 4 bytes to 8 bytes tomorrow (say, to fit a 64-bit length or a version
-- field), only encode_header/validate_header/decode's internal offsets
-- change. encode()/decode() keep the exact same signature, so no caller
-- anywhere in the codebase needs to change a single line.
--

local M = {}

local HEADER_FORMAT = ">BI4" -- 1 byte type + 4 byte big-endian length = 5 bytes
local HEADER_LEN = 5

--------------------------------------------------------------------------
-- Private Implementation
--------------------------------------------------------------------------

-- encode_header: assumes msg.type/msg.payload already validated by encode().
-- Its only job is turning already-trusted fields into wire bytes.
local function encode_header(msg)
    return string.pack(HEADER_FORMAT, msg.type, #msg.payload)
end

-- encode_payload: trivial today (payload is already a byte string), but
-- kept as its own private step so the module has one obvious place to add
-- future payload-level transforms (compression, checksums) without
-- touching encode_header or the public encode() contract.
local function encode_payload(msg)
    return msg.payload
end

-- validate_header: called only from M.decode, only after decode has already
-- checked that `bytes` is a string of at least HEADER_LEN bytes. It assumes
-- that much has already been checked -- its only remaining job is deciding
-- whether the *parsed* header fields make sense (e.g. does the declared
-- payload_len actually fit inside the given buffer).
local function validate_header(msg_type, payload_len, bytes)
    if payload_len < 0 then
        return false, "decode: negative payload_len is not valid"
    end
    if HEADER_LEN + payload_len > #bytes then
        return false, string.format(
            "decode: header declares payload_len=%d but only %d bytes available",
            payload_len, #bytes - HEADER_LEN)
    end
    return true
end

--------------------------------------------------------------------------
-- Public API
--------------------------------------------------------------------------

-- M.encode: validates the input shape, then delegates the two wire-format
-- steps to the private helpers. If the header layout changes in the future,
-- this function's body does not need to change at all.
function M.encode(msg)
    if type(msg) ~= "table" then
        return nil, "encode: msg must be a table"
    end
    if type(msg.type) ~= "number" or msg.type < 0 or msg.type > 255 then
        return nil, "encode: msg.type must be a number in [0, 255]"
    end
    if type(msg.payload) ~= "string" then
        return nil, "encode: msg.payload must be a string"
    end

    local header = encode_header(msg)
    local payload = encode_payload(msg)
    return header .. payload
end

-- M.decode: validates the raw bytes shape first (is it even a string, is
-- it long enough to contain a header), unpacks the header fields, then
-- hands them to validate_header for the "do these fields make sense"
-- check before trusting them to slice out the payload.
function M.decode(bytes)
    if type(bytes) ~= "string" then
        return nil, "decode: bytes must be a string"
    end
    if #bytes < HEADER_LEN then
        return nil, "decode: input too short to contain a header"
    end

    local msg_type, payload_len = string.unpack(HEADER_FORMAT, bytes)

    local ok, err = validate_header(msg_type, payload_len, bytes)
    if not ok then
        return nil, err
    end

    local payload = bytes:sub(HEADER_LEN + 1, HEADER_LEN + payload_len)

    return {
        type = msg_type,
        payload = payload,
    }
end

return M
```

`code/03-protocol-encoder/app.lua`：

```lua
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
```

## 6. Execution Flow

`lua5.4 app.lua` 的实际输出：

```text
== encode ==
encoded 10 bytes: 01 00 00 00 05 68 65 6C 6C 6F 

== decode ==
decoded type=1 payload="hello"

== round-trip assertion ==
round-trip OK: decode(encode(msg)) == msg

== decode: failure path (truncated buffer) ==
bad, err ->	nil	decode: input too short to contain a header

== decode: failure path (declared length larger than buffer) ==
bad2, err ->	nil	decode: header declares payload_len=999 but only 2 bytes available
```

`01 00 00 00 05` 正是 header 的 5 个字节（`type=1`,`payload_len=5` 大端表示),后面 `68 65 6C 6C 6F` 是 `"hello"` 的 ASCII 字节。

编码调用路径：

```text
app.lua
  |
  |  protocol.encode({type=1, payload="hello"})
  v
M.encode
  |
  |  校验 msg 形状
  |  encode_header(msg)        <- Private, string.pack 打包 type+长度
  v
encode_header ---> "\x01\x00\x00\x00\x05"
  |
  |  encode_payload(msg)       <- Private, 目前只是原样返回
  v
encode_payload ---> "hello"
  |
  |  拼接 header .. payload
  v
M.encode
  |
  |  返回完整字节串
  v
app.lua
```

## 7. State

这个 Module **没有 Module 级 State**——`HEADER_FORMAT`/`HEADER_LEN` 是不可变的常量,不是"随调用变化的数据"。`M.encode`/`M.decode` 都是纯函数式的：同样的输入永远产生同样的输出,两次调用之间不共享任何可变数据。这再一次说明 Pattern 2 的 State 是可选的——是否需要 State,取决于这个 Module 具体要解决的问题,不是 Pattern 本身强制要求的。

## 8. Why this Pattern fits

协议编解码的核心价值就在于"wire format 可以演进,调用者不用跟着变"。如果把 `encode_header`/`validate_header` 暴露成 `M.encode_header`/`M.validate_header`,任何调用者一旦直接用上它们的返回值形状,就等于把"header 目前是 5 个字节、第 1 个字节是 type"这个纯内部决策焊死进了自己的代码里——header 从 4 字节变成 8 字节这种后续演进(比如要支持超过 4GB 的 payload,把 `I4` 换成 `I8`)就会变成一次影响多处调用点的破坏性变更。Pattern 2 把这条演进路径完全封在 `protocol.lua` 内部：只要 `M.encode`/`M.decode` 的参数和返回值形状不变,`HEADER_FORMAT` 这个常量本身可以随时改。

## 9. What would go wrong with another Pattern

- **换成 Table Module（把 `encode_header`/`validate_header` 全部公开)**：等价于把 wire format 的字节布局写进了公开契约——任何人都可能开始自己拼 header 字节,一旦格式演进,没人能保证所有这些"自己拼字节"的代码都跟着更新。
- **换成 Class / Constructor Module（每次编解码创建一个 `Encoder` 对象)**：`encode`/`decode` 之间没有任何需要跨调用保留的状态——每次编码都是从一个新的 `msg` 表开始、每次解码都是从一段新的 `bytes` 开始,完全独立。引入对象只会多出一个毫无意义的 `Encoder.new()` 步骤,并且会让人误以为存在"这个 Encoder 记住了上一条消息"之类的隐藏状态,而实际上没有。

## 10. Mental Model

**一句话**：wire format 是协议的秘密,`encode()`/`decode()` 的签名是协议的承诺——秘密可以变,承诺不能变。

**核心问题**：如果 header 布局明天要变（4 字节变 8 字节),今天有多少行调用代码需要跟着改？答案应该永远是 0。

**核心关系**：

```text
M.encode / M.decode
        |
        v
encode_header / encode_payload / validate_header
        |
        v
当前 wire format（byte 1 = type, bytes 2-5 = length）
```
