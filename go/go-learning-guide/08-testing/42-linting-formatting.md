# Topic 42: Linting and Formatting (gofmt, go vet)

## 1. gofmt - Code Formatting

```bash
# Check if formatted
gofmt -d file.go        # Show diff
gofmt -l .              # List unformatted files

# Format in place
gofmt -w file.go        # Format file
gofmt -w .              # Format all

# Or use go fmt (wrapper)
go fmt ./...
```

**gofmt is not optional** - all Go code should be gofmt'd.

## 2. go vet - Static Analysis

```bash
go vet ./...
```

Catches:
- Printf format errors
- Unreachable code
- Incorrect struct tags
- Copying locks
- Nil checks after use

```go
// go vet catches this:
fmt.Printf("%d", "string")  // Wrong format verb

var mu sync.Mutex
mu2 := mu  // Copying mutex!
```

## 3. golint / staticcheck

```bash
# Install staticcheck (recommended)
go install honnef.co/go/tools/cmd/staticcheck@latest

# Run
staticcheck ./...
```

## 4. golangci-lint (All-in-one)

```bash
# Install
go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest

# Run
golangci-lint run

# With config (.golangci.yml)
linters:
  enable:
    - gofmt
    - govet
    - staticcheck
    - errcheck
    - gosimple
```

## 5. Editor Integration

Most editors run formatters on save:
- VS Code: Go extension
- GoLand: Built-in
- Vim: vim-go

## 6. CI/CD Checks

```yaml
# GitHub Actions example
- name: Format check
  run: |
    if [ -n "$(gofmt -l .)" ]; then
      echo "Code not formatted"
      exit 1
    fi

- name: Vet
  run: go vet ./...

- name: Lint
  run: golangci-lint run
```

---

**Summary**: gofmt is mandatory, go vet catches bugs, golangci-lint combines many linters. Run all in CI.

