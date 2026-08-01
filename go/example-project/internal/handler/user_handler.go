// Package handler 是 HTTP 传输层的适配器：只负责"HTTP 请求 <-> 领域调用"
// 的转换（解析 JSON、校验最基础的输入格式、编码响应），不包含任何业务规则。
// 它依赖 service（业务逻辑）和 pkg/errors（统一错误响应格式）。
package handler

import (
	"encoding/json"
	"errors"
	"net/http"

	"github.com/morrism/go-example-project/internal/domain"
	"github.com/morrism/go-example-project/internal/service"
	apperrors "github.com/morrism/go-example-project/pkg/errors"
	"github.com/morrism/go-example-project/pkg/logger"
)

// UserHandler 把 HTTP 请求翻译成对 UserService 的调用。
type UserHandler struct {
	svc *service.UserService
	log *logger.Logger
}

func New(svc *service.UserService, log *logger.Logger) *UserHandler {
	return &UserHandler{svc: svc, log: log}
}

type registerRequest struct {
	Username string `json:"username"`
	Password string `json:"password"`
}

type loginRequest struct {
	Username string `json:"username"`
	Password string `json:"password"`
}

type loginResponse struct {
	Token string `json:"token"`
}

type errorResponse struct {
	Error string `json:"error"`
}

// Register 处理 POST /users/register。
func (h *UserHandler) Register(w http.ResponseWriter, r *http.Request) {
	var req registerRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeError(w, apperrors.BadRequest("invalid request body", err))
		return
	}

	u, err := h.svc.Register(r.Context(), req.Username, req.Password)
	if err != nil {
		writeError(w, translateError(err))
		return
	}

	writeJSON(w, http.StatusCreated, u)
}

// Login 处理 POST /users/login。
func (h *UserHandler) Login(w http.ResponseWriter, r *http.Request) {
	var req loginRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeError(w, apperrors.BadRequest("invalid request body", err))
		return
	}

	token, err := h.svc.Login(r.Context(), req.Username, req.Password)
	if err != nil {
		writeError(w, translateError(err))
		return
	}

	writeJSON(w, http.StatusOK, loginResponse{Token: token})
}

// GetUser 处理 GET /users/{id}。
func (h *UserHandler) GetUser(w http.ResponseWriter, r *http.Request) {
	id := r.PathValue("id")

	u, err := h.svc.GetUser(r.Context(), id)
	if err != nil {
		writeError(w, translateError(err))
		return
	}

	writeJSON(w, http.StatusOK, u)
}

// translateError 是"错误边界"：把 domain 的哨兵错误映射成带 HTTP 状态码的 AppError。
// service/repository 层完全不需要知道 HTTP 状态码是什么。
func translateError(err error) *apperrors.AppError {
	var appErr *apperrors.AppError
	if errors.As(err, &appErr) {
		return appErr
	}

	switch {
	case errors.Is(err, domain.ErrUserNotFound):
		return apperrors.NotFound("user not found", err)
	case errors.Is(err, domain.ErrUserAlreadyExists):
		return apperrors.Conflict("username already taken", err)
	case errors.Is(err, domain.ErrInvalidCredentials):
		return apperrors.Unauthorized("invalid username or password", err)
	default:
		return apperrors.Internal("internal server error", err)
	}
}

func writeError(w http.ResponseWriter, err *apperrors.AppError) {
	writeJSON(w, err.Status, errorResponse{Error: err.Message})
}

func writeJSON(w http.ResponseWriter, status int, body any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(body)
}
