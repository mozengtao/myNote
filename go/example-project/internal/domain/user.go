// Package domain 定义业务领域模型。它不依赖任何其他内部包
// （不依赖 handler/service/repository），是整个项目依赖图的最底层。
package domain

import "time"

// User 是用户领域模型，只描述"用户是什么"，不关心如何存储、如何传输。
type User struct {
	ID           string    `json:"id"`
	Username     string    `json:"username"`
	PasswordHash string    `json:"-"` // 永远不通过 JSON 序列化返回给客户端
	CreatedAt    time.Time `json:"created_at"`
}
