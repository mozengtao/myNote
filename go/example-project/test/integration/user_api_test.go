// test/integration 是文档里"integration test"这个概念的落地：
// 不像 internal/service 里的单元测试那样只测一个包，而是把
// handler + service + persistence/memory + transport/http 全部真实
// 组装起来，通过一个真实的 net/http 端口发请求，验证整条链路。
package integration

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/morrism/go-example-project/internal/handler"
	"github.com/morrism/go-example-project/internal/persistence/memory"
	"github.com/morrism/go-example-project/internal/service"
	transporthttp "github.com/morrism/go-example-project/internal/transport/http"
	"github.com/morrism/go-example-project/pkg/logger"
)

// newTestServer 复现了 internal/app.New 的组装步骤（不依赖 app 包本身，
// 因为 app.App 没有暴露内部 http.Handler），从而拿到一个完全真实的、
// 端到端可用的 HTTP 服务用于集成测试。
func newTestServer(t *testing.T) *httptest.Server {
	t.Helper()

	log := logger.New(logger.LevelError)
	repo := memory.New()
	svc := service.New(repo, log)
	h := handler.New(svc, log)
	router := transporthttp.New(h, log)

	srv := httptest.NewServer(router)
	t.Cleanup(srv.Close)
	return srv
}

func postJSON(t *testing.T, url string, body any) *http.Response {
	t.Helper()

	payload, err := json.Marshal(body)
	if err != nil {
		t.Fatalf("marshal request body: %v", err)
	}

	resp, err := http.Post(url, "application/json", bytes.NewReader(payload))
	if err != nil {
		t.Fatalf("POST %s: %v", url, err)
	}
	return resp
}

func TestUserAPI_RegisterLoginGetUser(t *testing.T) {
	srv := newTestServer(t)

	// 1. 健康检查
	healthResp, err := http.Get(srv.URL + "/healthz")
	if err != nil {
		t.Fatalf("GET /healthz: %v", err)
	}
	defer healthResp.Body.Close()
	if healthResp.StatusCode != http.StatusOK {
		t.Fatalf("GET /healthz status = %d, want %d", healthResp.StatusCode, http.StatusOK)
	}

	// 2. 注册
	registerResp := postJSON(t, srv.URL+"/users/register", map[string]string{
		"username": "alice",
		"password": "s3cret",
	})
	defer registerResp.Body.Close()
	if registerResp.StatusCode != http.StatusCreated {
		t.Fatalf("POST /users/register status = %d, want %d", registerResp.StatusCode, http.StatusCreated)
	}

	var registered struct {
		ID       string `json:"id"`
		Username string `json:"username"`
	}
	if err := json.NewDecoder(registerResp.Body).Decode(&registered); err != nil {
		t.Fatalf("decode register response: %v", err)
	}
	if registered.ID == "" {
		t.Fatal("expected non-empty user id after register")
	}

	// 3. 重复注册应该 409
	dupResp := postJSON(t, srv.URL+"/users/register", map[string]string{
		"username": "alice",
		"password": "s3cret",
	})
	defer dupResp.Body.Close()
	if dupResp.StatusCode != http.StatusConflict {
		t.Fatalf("duplicate register status = %d, want %d", dupResp.StatusCode, http.StatusConflict)
	}

	// 4. 登录
	loginResp := postJSON(t, srv.URL+"/users/login", map[string]string{
		"username": "alice",
		"password": "s3cret",
	})
	defer loginResp.Body.Close()
	if loginResp.StatusCode != http.StatusOK {
		t.Fatalf("POST /users/login status = %d, want %d", loginResp.StatusCode, http.StatusOK)
	}

	var loginBody struct {
		Token string `json:"token"`
	}
	if err := json.NewDecoder(loginResp.Body).Decode(&loginBody); err != nil {
		t.Fatalf("decode login response: %v", err)
	}
	if loginBody.Token == "" {
		t.Fatal("expected non-empty token after login")
	}

	// 5. 密码错误应该 401
	badLoginResp := postJSON(t, srv.URL+"/users/login", map[string]string{
		"username": "alice",
		"password": "wrong",
	})
	defer badLoginResp.Body.Close()
	if badLoginResp.StatusCode != http.StatusUnauthorized {
		t.Fatalf("bad login status = %d, want %d", badLoginResp.StatusCode, http.StatusUnauthorized)
	}

	// 6. 按 ID 查询
	getResp, err := http.Get(srv.URL + "/users/" + registered.ID)
	if err != nil {
		t.Fatalf("GET /users/{id}: %v", err)
	}
	defer getResp.Body.Close()
	if getResp.StatusCode != http.StatusOK {
		t.Fatalf("GET /users/{id} status = %d, want %d", getResp.StatusCode, http.StatusOK)
	}

	// 7. 查询不存在的用户应该 404
	notFoundResp, err := http.Get(srv.URL + "/users/does-not-exist")
	if err != nil {
		t.Fatalf("GET /users/does-not-exist: %v", err)
	}
	defer notFoundResp.Body.Close()
	if notFoundResp.StatusCode != http.StatusNotFound {
		t.Fatalf("GET /users/does-not-exist status = %d, want %d", notFoundResp.StatusCode, http.StatusNotFound)
	}
}
