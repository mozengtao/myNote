# Topic 54: Code Review Checklist for Go

## Error Handling
- [ ] All errors are checked
- [ ] Errors are wrapped with context (`%w`)
- [ ] No `panic` for expected errors
- [ ] Error messages are lowercase, no punctuation

## Concurrency
- [ ] Shared data protected by mutex OR channels
- [ ] Goroutines have exit conditions
- [ ] Context passed for cancellation
- [ ] `defer wg.Done()` pattern used
- [ ] Race detector run (`-race`)

## Resource Management
- [ ] `defer Close()` after opening resources
- [ ] Close called after error check
- [ ] Connections/files have timeouts

## Interfaces
- [ ] Interfaces are small (1-3 methods)
- [ ] Interfaces defined at point of use
- [ ] No `interface{}` where types could work

## Naming
- [ ] Exported names are clear without package prefix
- [ ] No stuttering (`http.HTTPClient` → `http.Client`)
- [ ] Receivers are consistent (all pointer or all value)

## Testing
- [ ] Table-driven tests for multiple cases
- [ ] Test covers error paths
- [ ] Mocks/fakes use interfaces

## Performance
- [ ] No obvious allocations in hot paths
- [ ] Slices pre-allocated when size known
- [ ] No `fmt.Sprintf` in hot paths

## Documentation
- [ ] Exported functions have doc comments
- [ ] Package has doc comment
- [ ] Non-obvious code has comments

## Formatting
- [ ] `gofmt` applied
- [ ] `go vet` passes
- [ ] No linter errors

## Security
- [ ] No secrets in code
- [ ] Input validated before use
- [ ] SQL uses parameterized queries

## Quick Commands

```bash
# Before submitting
gofmt -w .
go vet ./...
go test -race ./...
golangci-lint run
```

---

**Summary**: Use this checklist for self-review before requesting code review. Automate checks in CI.

