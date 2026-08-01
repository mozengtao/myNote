// Package memory 是 repository.UserRepository 的一个真实实现：
// 用内存中的 map 存储数据，配合 sync.RWMutex 保证并发安全。
// 它足够简单，可以让整个示例项目在没有任何外部数据库的情况下真正跑起来，
// 也非常适合在单元测试里当作测试替身使用。
package memory

import (
	"context"
	"sync"

	"github.com/morrism/go-example-project/internal/domain"
	"github.com/morrism/go-example-project/internal/repository"
)

type userRepository struct {
	mu    sync.RWMutex
	byID  map[string]*domain.User
	byUsr map[string]string // username -> id，用于按用户名快速查找
}

// New 返回一个基于内存的 UserRepository 实现。
func New() repository.UserRepository {
	return &userRepository{
		byID:  make(map[string]*domain.User),
		byUsr: make(map[string]string),
	}
}

func (r *userRepository) Create(_ context.Context, u *domain.User) error {
	r.mu.Lock()
	defer r.mu.Unlock()

	if _, exists := r.byUsr[u.Username]; exists {
		return domain.ErrUserAlreadyExists
	}

	// 拷贝一份存储，避免调用方后续修改指针指向的内容影响到"数据库"里的数据。
	stored := *u
	r.byID[u.ID] = &stored
	r.byUsr[u.Username] = u.ID
	return nil
}

func (r *userRepository) FindByID(_ context.Context, id string) (*domain.User, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()

	u, ok := r.byID[id]
	if !ok {
		return nil, domain.ErrUserNotFound
	}
	found := *u
	return &found, nil
}

func (r *userRepository) FindByUsername(_ context.Context, username string) (*domain.User, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()

	id, ok := r.byUsr[username]
	if !ok {
		return nil, domain.ErrUserNotFound
	}
	found := *r.byID[id]
	return &found, nil
}
