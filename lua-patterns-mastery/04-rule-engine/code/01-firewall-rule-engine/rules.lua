-- rules.lua
-- Firewall Rule Engine：一组结构统一的规则，每条规则是 { name, match, action }
-- match(packet)  -> boolean          判断这个 packet 是否命中本条规则
-- action(packet) -> "ALLOW"/"DROP"/"LOG"  命中后应该执行的动作（只是描述，不产生真实副作用）
--
-- Host 端采用 First Match 语义：从上到下遍历，第一条 match 返回 true 的规则
-- 就决定了这个 packet 的最终动作，后面的规则不会再被判断。
-- 因此规则的先后顺序本身就是一种优先级，书写顺序很重要。

rules = {
    -- 规则 1：放行发往 22 端口（SSH）的流量
    {
        name = "allow-ssh",
        match = function(packet)
            return packet.port == 22
        end,
        action = function(packet)
            return "ALLOW"
        end
    },

    -- 规则 2：来自黑名单地址的流量直接丢弃（覆盖 src 字段的用法）
    {
        name = "drop-blacklisted-src",
        match = function(packet)
            return packet.src == "10.0.0.66"
        end,
        action = function(packet)
            return "DROP"
        end
    },

    -- 规则 3：内网（10.0.0.0/8）访问 DMZ（192.168.0.0/16）只记录日志，不拦截
    -- 覆盖 src + dst 两个字段联合判断的用法
    {
        name = "log-internal-to-dmz",
        match = function(packet)
            return packet.src:match("^10%.") ~= nil and packet.dst:match("^192%.168%.") ~= nil
        end,
        action = function(packet)
            return "LOG"
        end
    },

    -- 规则 4：放行 HTTP/HTTPS
    {
        name = "allow-http",
        match = function(packet)
            return packet.port == 80 or packet.port == 443
        end,
        action = function(packet)
            return "ALLOW"
        end
    },

    -- 规则 5：默认规则，兜底丢弃所有未被前面规则命中的流量
    -- 永远放在最后一条，match 恒为 true
    {
        name = "default-drop",
        match = function(packet)
            return true
        end,
        action = function(packet)
            return "DROP"
        end
    }
}
