// Package service 承载业务逻辑（"公司里的业务部门"）。它只依赖
// repository 接口和 domain 模型，完全不知道 HTTP、JSON 或数据库驱动
// 的存在——这正是分层架构里 handler -> service -> repository 单向依赖
// 的核心：service 可以在完全不同的传输层（HTTP、gRPC、CLI）下被复用。
package service

import (
	"context"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"time"

	"github.com/morrism/go-example-project/internal/domain"
	"github.com/morrism/go-example-project/internal/repository"
	"github.com/morrism/go-example-project/pkg/logger"
)

// UserService 封装用户相关的业务用例。
type UserService struct {
	repo repository.UserRepository
	log  *logger.Logger
}

// New 通过依赖注入接收一个 repository.UserRepository 实现——
// 调用方决定用 memory 还是 mysql，UserService 本身完全不关心。
func New(repo repository.UserRepository, log *logger.Logger) *UserService {
	return &UserService{repo: repo, log: log}
}

// Register 创建一个新用户，密码以 sha256 哈希后存储。
//
// 生产环境应使用 bcrypt/argon2 等专为密码设计的慢哈希算法；这里为了保持
// "零第三方依赖"用标准库 crypto/sha256 做简化演示，README 中会说明这一点。
func (s *UserService) Register(ctx context.Context, username, password string) (*domain.User, error) {
	if username == "" || password == "" {
		return nil, fmt.Errorf("username and password are required")
	}

	id, err := randomHex(16)
	if err != nil {
		return nil, fmt.Errorf("generate id: %w", err)
	}

	u := &domain.User{
		ID:           id,
		Username:     username,
		PasswordHash: hashPassword(password),
		CreatedAt:    time.Now().UTC(),
	}

	if err := s.repo.Create(ctx, u); err != nil {
		return nil, err // domain.ErrUserAlreadyExists 会原样透传给 handler
	}

	s.log.Info("user registered: id=%s username=%s", u.ID, u.Username)
	return u, nil
}

// Login 校验用户名密码，成功后签发一个"伪 token"。
//
// 生产环境这里应该签发并返回一个真正的 JWT（包含过期时间、签名），
// 这属于"具体复杂实现"，用打印说明代替真正的签发逻辑。
func (s *UserService) Login(ctx context.Context, username, password string) (string, error) {
	u, err := s.repo.FindByUsername(ctx, username)
	if err != nil {
		if err == domain.ErrUserNotFound {
			// 不向调用方暴露"用户不存在"和"密码错误"的区别，避免用户名枚举攻击。
			return "", domain.ErrInvalidCredentials
		}
		return "", err
	}

	if hashPassword(password) != u.PasswordHash {
		return "", domain.ErrInvalidCredentials
	}

	token, err := randomHex(24)
	if err != nil {
		return "", fmt.Errorf("generate token: %w", err)
	}

	fmt.Printf("[service] would issue a signed JWT here; using opaque token instead: %s\n", token)
	s.log.Info("user logged in: username=%s", username)
	return token, nil
}

// GetUser 按 ID 查询用户。
func (s *UserService) GetUser(ctx context.Context, id string) (*domain.User, error) {
	return s.repo.FindByID(ctx, id)
}

func hashPassword(password string) string {
	sum := sha256.Sum256([]byte(password))
	return hex.EncodeToString(sum[:])
}

func randomHex(n int) (string, error) {
	b := make([]byte, n)
	if _, err := rand.Read(b); err != nil {
		return "", err
	}
	return hex.EncodeToString(b), nil
}
