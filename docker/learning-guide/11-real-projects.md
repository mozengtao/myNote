# Section 11: Using Docker in Real Projects

## 11.1 Development Workflow

```
TYPICAL DOCKER DEVELOPMENT WORKFLOW:
+==================================================================+
||                                                                ||
||  1. Write code locally                                         ||
||       |                                                        ||
||       v                                                        ||
||  2. docker-compose up (with bind mount)                        ||
||     +----------------------------------------------------------+
||     | services:                                                |
||     |   app:                                                   |
||     |     build: .                                             |
||     |     volumes:                                             |
||     |       - .:/app    # Code synced into container           |
||     +----------------------------------------------------------+
||       |                                                        ||
||       v                                                        ||
||  3. Edit code -> Changes reflected immediately                 ||
||     (with hot-reload frameworks)                               ||
||       |                                                        ||
||       v                                                        ||
||  4. Test in containerized environment                          ||
||     (same dependencies as production)                          ||
||       |                                                        ||
||       v                                                        ||
||  5. Commit code + Dockerfile                                   ||
||       |                                                        ||
||       v                                                        ||
||  6. CI builds and tests image                                  ||
||       |                                                        ||
||       v                                                        ||
||  7. Push image to registry                                     ||
||       |                                                        ||
||       v                                                        ||
||  8. Deploy same image to production                            ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

典型Docker开发工作流：

1. 本地编写代码
2. docker-compose up（带bind mount）代码同步到容器
3. 编辑代码 -> 变更立即反映（用热重载框架）
4. 在容器化环境测试（与生产相同依赖）
5. 提交代码+Dockerfile
6. CI构建和测试镜像
7. 推送镜像到仓库
8. 将相同镜像部署到生产

### Development docker-compose.yml

```yaml
# docker-compose.dev.yml
version: "3.8"

services:
  app:
    build:
      context: .
      target: development  # Multi-stage build target
    volumes:
      - .:/app             # Sync code
      - /app/node_modules  # Exclude node_modules (use container's)
    ports:
      - "3000:3000"
    environment:
      - NODE_ENV=development
      - DEBUG=true
    command: npm run dev   # Hot-reload command
```

---

## 11.2 Production Considerations

### Image Size

```
IMAGE SIZE MATTERS:
+------------------------------------------------------------------+
|                                                                  |
|  WHY SIZE MATTERS:                                               |
|  - Faster deployments (less to download)                         |
|  - Less storage costs                                            |
|  - Smaller attack surface                                        |
|  - Faster container start                                        |
|                                                                  |
|  SIZE REDUCTION STRATEGIES:                                      |
|  +------------------------------------------------------------+  |
|  |  1. Use slim/alpine base images                           |  |
|  |     FROM python:3.9-slim  (150MB vs 900MB full)           |  |
|  |                                                           |  |
|  |  2. Multi-stage builds                                    |  |
|  |     Build in one stage, copy artifacts to minimal runtime |  |
|  |                                                           |  |
|  |  3. Combine RUN commands                                  |  |
|  |     Clean up in same layer                                |  |
|  |                                                           |  |
|  |  4. Use .dockerignore                                     |  |
|  |     Exclude .git, node_modules, __pycache__, etc.         |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

镜像大小很重要：更快部署（更少下载）、更少存储成本、更小攻击面、更快容器启动。

大小减少策略：1）使用slim/alpine基础镜像、2）多阶段构建、3）合并RUN命令在同层清理、4）使用.dockerignore排除.git、node_modules等。

### Multi-Stage Build Example

```dockerfile
# Multi-stage build for Go application

# Stage 1: Build
FROM golang:1.17 AS builder
WORKDIR /build
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=0 GOOS=linux go build -o /app

# Stage 2: Runtime (minimal)
FROM alpine:3.14
RUN apk --no-cache add ca-certificates
COPY --from=builder /app /app
USER nobody
ENTRYPOINT ["/app"]

# Result:
# - Build stage: ~800MB (Go toolchain + source)
# - Final image: ~15MB (just binary + alpine)
```

**Chinese Explanation (中文说明):**

多阶段构建示例（Go应用）：阶段1（Build）用golang:1.17构建，阶段2（Runtime）用alpine:3.14只复制二进制。结果：构建阶段约800MB，最终镜像约15MB。

### Security Best Practices

```
SECURITY CHECKLIST:
+------------------------------------------------------------------+
|                                                                  |
|  1. DON'T RUN AS ROOT                                            |
|  +------------------------------------------------------------+  |
|  |  RUN useradd -r -s /bin/false appuser                     |  |
|  |  USER appuser                                             |  |
|  |  # Container process runs as non-root                     |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  2. DON'T STORE SECRETS IN IMAGES                                |
|  +------------------------------------------------------------+  |
|  |  BAD:  ENV API_KEY=supersecret                            |  |
|  |  GOOD: Pass at runtime: docker run -e API_KEY=...         |  |
|  |  BETTER: Use Docker secrets or secret management          |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  3. USE SPECIFIC IMAGE TAGS                                      |
|  +------------------------------------------------------------+  |
|  |  BAD:  FROM python:latest                                 |  |
|  |  GOOD: FROM python:3.9.7-slim-buster                      |  |
|  |  # Reproducible builds                                    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  4. SCAN IMAGES FOR VULNERABILITIES                              |
|  +------------------------------------------------------------+  |
|  |  docker scan myimage:latest                               |  |
|  |  # Or use Trivy, Clair, Snyk                              |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  5. MINIMIZE INSTALLED PACKAGES                                  |
|  +------------------------------------------------------------+  |
|  |  Less software = fewer vulnerabilities                    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

安全检查清单：

1. 不以root运行：创建用户并切换到该用户。
2. 不在镜像中存储密钥：运行时传递或使用密钥管理。
3. 使用具体镜像标签：FROM python:3.9.7-slim-buster而非latest。
4. 扫描镜像漏洞：docker scan或Trivy/Clair/Snyk。
5. 最小化安装包：软件越少漏洞越少。

### Configuration via Environment Variables

```
12-FACTOR APP CONFIGURATION:
+------------------------------------------------------------------+
|                                                                  |
|  CONFIGURATION SHOULD COME FROM ENVIRONMENT, NOT FILES           |
|                                                                  |
|  BAD (config baked into image):                                  |
|  +------------------------------------------------------------+  |
|  |  COPY config.prod.json /app/config.json                   |  |
|  |  # Different image for each environment                   |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  GOOD (config from environment):                                 |
|  +------------------------------------------------------------+  |
|  |  # Application reads from environment                     |  |
|  |  db_host = os.environ.get('DATABASE_HOST', 'localhost')   |  |
|  |                                                           |  |
|  |  # Different config per deployment:                       |  |
|  |  docker run -e DATABASE_HOST=prod.db.example.com myapp    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  BENEFITS:                                                       |
|  - Same image for dev, staging, production                       |
|  - No secrets in image                                           |
|  - Easy to change without rebuild                                |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

12-Factor应用配置：配置应来自环境而非文件。

差的做法：COPY config.prod.json（配置烘焙到镜像，每个环境不同镜像）。

好的做法：应用从环境读取，docker run -e传递配置。好处：dev/staging/production同一镜像、镜像无密钥、无需重建即可更改。

---

## Summary

```
REAL PROJECTS KEY INSIGHTS:
+==================================================================+
||                                                                ||
||  DEVELOPMENT:                                                  ||
||  - Use bind mounts for code sync                               ||
||  - Hot-reload for fast iteration                               ||
||  - docker-compose for local stack                              ||
||                                                                ||
||  PRODUCTION:                                                   ||
||  - Optimize image size (multi-stage, slim bases)               ||
||  - Run as non-root user                                        ||
||  - Don't store secrets in images                               ||
||  - Use specific image tags                                     ||
||  - Configure via environment variables                         ||
||  - Implement health checks                                     ||
||                                                                ||
||  CI/CD:                                                        ||
||  - Build image in CI                                           ||
||  - Run tests in container                                      ||
||  - Push to registry                                            ||
||  - Deploy same image everywhere                                ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Common Beginner Mistakes](12-common-mistakes.md)
