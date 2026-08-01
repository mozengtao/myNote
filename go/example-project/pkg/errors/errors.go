// Package errors 提供统一的"错误边界"类型 AppError，用来在系统边界
// （这里是 HTTP handler）把内部错误（领域错误、基础设施错误）翻译成
// 面向调用方的、带 HTTP 状态码的错误。
//
// 这是 04-errors/04-error-boundaries.md 里"错误边界"概念的落地：
// 内层（domain/service/repository）只使用 Go 原生 error 和哨兵错误，
// 完全不知道 HTTP 的存在；只有 handler 这一层才把它们收敛成 AppError。
package errors

import (
	"fmt"
	"net/http"
)

// AppError 是一个携带 HTTP 状态码的应用级错误。
type AppError struct {
	Status  int
	Message string
	Err     error
}

func (e *AppError) Error() string {
	if e.Err != nil {
		return fmt.Sprintf("%s: %v", e.Message, e.Err)
	}
	return e.Message
}

// Unwrap 让 errors.Is / errors.As 可以穿透 AppError 找到原始错误。
func (e *AppError) Unwrap() error {
	return e.Err
}

func New(status int, message string, err error) *AppError {
	return &AppError{Status: status, Message: message, Err: err}
}

func NotFound(message string, err error) *AppError {
	return New(http.StatusNotFound, message, err)
}

func Conflict(message string, err error) *AppError {
	return New(http.StatusConflict, message, err)
}

func Unauthorized(message string, err error) *AppError {
	return New(http.StatusUnauthorized, message, err)
}

func BadRequest(message string, err error) *AppError {
	return New(http.StatusBadRequest, message, err)
}

func Internal(message string, err error) *AppError {
	return New(http.StatusInternalServerError, message, err)
}
