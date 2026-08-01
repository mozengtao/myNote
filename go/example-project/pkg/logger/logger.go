// Package logger 是一个可以被外部模块 import 的极简分级日志库，
// 放在 pkg/ 下（而不是 internal/），因为它不包含任何本项目私有的业务知识，
// 任何人都可以直接拿去用——这正是 pkg/ 与 internal/ 的分界线。
package logger

import (
	"log"
	"os"
)

// Level 表示日志级别。
type Level int

const (
	LevelDebug Level = iota
	LevelInfo
	LevelError
)

// Logger 是对标准库 log.Logger 的一层极薄封装，只是为了演示
// "分级日志"这个概念，真实项目通常会换成 zap/zerolog 等结构化日志库。
type Logger struct {
	level Level
	std   *log.Logger
}

// New 创建一个输出到 stdout、带级别前缀的 Logger。
func New(level Level) *Logger {
	return &Logger{
		level: level,
		std:   log.New(os.Stdout, "", log.LstdFlags),
	}
}

func (l *Logger) Debug(format string, args ...any) {
	l.log(LevelDebug, "DEBUG", format, args...)
}

func (l *Logger) Info(format string, args ...any) {
	l.log(LevelInfo, "INFO", format, args...)
}

func (l *Logger) Error(format string, args ...any) {
	l.log(LevelError, "ERROR", format, args...)
}

func (l *Logger) log(level Level, tag, format string, args ...any) {
	if level < l.level {
		return
	}
	l.std.Printf("["+tag+"] "+format, args...)
}
