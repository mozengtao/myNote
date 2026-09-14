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
