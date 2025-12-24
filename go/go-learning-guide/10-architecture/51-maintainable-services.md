# Topic 51: Writing Maintainable Go Services

## 1. Key Principles

- **Explicit over implicit**: No magic
- **Simple over clever**: Readable by all
- **Small packages**: Single responsibility
- **Clear interfaces**: Easy to mock

## 2. Service Structure Template

```go
// service.go
type Config struct {
    DBUrl      string
    Port       int
    LogLevel   string
}

type Service struct {
    config Config
    db     *sql.DB
    logger *log.Logger
}

func New(cfg Config) (*Service, error) {
    db, err := sql.Open("postgres", cfg.DBUrl)
    if err != nil {
        return nil, fmt.Errorf("open database: %w", err)
    }
    
    return &Service{
        config: cfg,
        db:     db,
        logger: log.Default(),
    }, nil
}

func (s *Service) Run(ctx context.Context) error {
    // Setup
    srv := &http.Server{
        Addr:    fmt.Sprintf(":%d", s.config.Port),
        Handler: s.routes(),
    }
    
    // Graceful shutdown
    errCh := make(chan error, 1)
    go func() {
        if err := srv.ListenAndServe(); err != http.ErrServerClosed {
            errCh <- err
        }
    }()
    
    select {
    case <-ctx.Done():
        return srv.Shutdown(context.Background())
    case err := <-errCh:
        return err
    }
}

func (s *Service) Close() error {
    return s.db.Close()
}
```

## 3. Health Checks

```go
func (s *Service) routes() http.Handler {
    mux := http.NewServeMux()
    
    mux.HandleFunc("/health", s.handleHealth)
    mux.HandleFunc("/ready", s.handleReady)
    mux.HandleFunc("/api/", s.handleAPI)
    
    return mux
}

func (s *Service) handleHealth(w http.ResponseWriter, r *http.Request) {
    w.WriteHeader(http.StatusOK)
    w.Write([]byte("OK"))
}

func (s *Service) handleReady(w http.ResponseWriter, r *http.Request) {
    if err := s.db.PingContext(r.Context()); err != nil {
        http.Error(w, "database not ready", http.StatusServiceUnavailable)
        return
    }
    w.WriteHeader(http.StatusOK)
}
```

## 4. Configuration

```go
func LoadConfig() Config {
    return Config{
        DBUrl:    getEnv("DB_URL", "postgres://localhost/myapp"),
        Port:     getEnvInt("PORT", 8080),
        LogLevel: getEnv("LOG_LEVEL", "info"),
    }
}

func getEnv(key, fallback string) string {
    if value, ok := os.LookupEnv(key); ok {
        return value
    }
    return fallback
}
```

## 5. Logging

```go
// Structured logging
import "log/slog"

logger := slog.New(slog.NewJSONHandler(os.Stdout, nil))

logger.Info("request handled",
    slog.String("method", r.Method),
    slog.String("path", r.URL.Path),
    slog.Duration("duration", time.Since(start)),
)
```

## 6. Error Handling Pattern

```go
func (s *Service) handleCreateUser(w http.ResponseWriter, r *http.Request) {
    var req CreateUserRequest
    if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
        s.respondError(w, http.StatusBadRequest, "invalid JSON")
        return
    }
    
    user, err := s.createUser(r.Context(), req)
    if err != nil {
        s.logger.Error("create user", "error", err)
        s.respondError(w, http.StatusInternalServerError, "internal error")
        return
    }
    
    s.respondJSON(w, http.StatusCreated, user)
}
```

---

**Summary**: Use constructor injection, explicit configuration, graceful shutdown, health checks, and structured logging. Keep packages small and focused.

