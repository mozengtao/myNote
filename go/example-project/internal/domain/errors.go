package domain

import "errors"

// 哨兵错误（sentinel errors）：service 和 repository 之间通过这些错误值通信，
// handler 再用 errors.Is 把它们翻译成 HTTP 状态码。
// 这样 repository 的实现（memory / mysql）不需要知道 HTTP 的存在。
var (
	ErrUserNotFound       = errors.New("user not found")
	ErrUserAlreadyExists  = errors.New("user already exists")
	ErrInvalidCredentials = errors.New("invalid username or password")
)
