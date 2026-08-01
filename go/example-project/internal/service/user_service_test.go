// 单元测试与业务代码放在同一个目录，这是 Go 的惯用法（idiomatic），
// 用 persistence/memory 作为测试替身（fake），不需要 mock 框架。
package service_test

import (
	"context"
	"errors"
	"testing"

	"github.com/morrism/go-example-project/internal/domain"
	"github.com/morrism/go-example-project/internal/persistence/memory"
	"github.com/morrism/go-example-project/internal/service"
	"github.com/morrism/go-example-project/pkg/logger"
)

func newTestService() *service.UserService {
	repo := memory.New()
	log := logger.New(logger.LevelError) // 测试时屏蔽 Info/Debug 日志噪音
	return service.New(repo, log)
}

func TestRegister_Success(t *testing.T) {
	svc := newTestService()

	u, err := svc.Register(context.Background(), "alice", "s3cret")
	if err != nil {
		t.Fatalf("Register() unexpected error: %v", err)
	}
	if u.ID == "" {
		t.Fatal("Register() expected non-empty ID")
	}
	if u.Username != "alice" {
		t.Fatalf("Register() username = %q, want %q", u.Username, "alice")
	}
	if u.PasswordHash == "s3cret" {
		t.Fatal("Register() must not store the raw password")
	}
}

func TestRegister_DuplicateUsername(t *testing.T) {
	svc := newTestService()
	ctx := context.Background()

	if _, err := svc.Register(ctx, "alice", "s3cret"); err != nil {
		t.Fatalf("first Register() unexpected error: %v", err)
	}

	_, err := svc.Register(ctx, "alice", "another-password")
	if !errors.Is(err, domain.ErrUserAlreadyExists) {
		t.Fatalf("Register() error = %v, want %v", err, domain.ErrUserAlreadyExists)
	}
}

func TestLogin_Success(t *testing.T) {
	svc := newTestService()
	ctx := context.Background()

	if _, err := svc.Register(ctx, "alice", "s3cret"); err != nil {
		t.Fatalf("Register() unexpected error: %v", err)
	}

	token, err := svc.Login(ctx, "alice", "s3cret")
	if err != nil {
		t.Fatalf("Login() unexpected error: %v", err)
	}
	if token == "" {
		t.Fatal("Login() expected non-empty token")
	}
}

func TestLogin_WrongPassword(t *testing.T) {
	svc := newTestService()
	ctx := context.Background()

	if _, err := svc.Register(ctx, "alice", "s3cret"); err != nil {
		t.Fatalf("Register() unexpected error: %v", err)
	}

	_, err := svc.Login(ctx, "alice", "wrong-password")
	if !errors.Is(err, domain.ErrInvalidCredentials) {
		t.Fatalf("Login() error = %v, want %v", err, domain.ErrInvalidCredentials)
	}
}

func TestLogin_UnknownUser(t *testing.T) {
	svc := newTestService()

	_, err := svc.Login(context.Background(), "ghost", "whatever")
	if !errors.Is(err, domain.ErrInvalidCredentials) {
		t.Fatalf("Login() error = %v, want %v", err, domain.ErrInvalidCredentials)
	}
}

func TestGetUser_NotFound(t *testing.T) {
	svc := newTestService()

	_, err := svc.GetUser(context.Background(), "does-not-exist")
	if !errors.Is(err, domain.ErrUserNotFound) {
		t.Fatalf("GetUser() error = %v, want %v", err, domain.ErrUserNotFound)
	}
}
