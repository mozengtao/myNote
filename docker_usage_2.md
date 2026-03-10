# Docker Usage Scenarios for Developers

A practical reference of how Docker is used in real development workflows.

---

## Case 1: Interactive Environment (REPL / Shell)

Spin up a language runtime or OS shell to experiment interactively.

```bash
docker run -it python:3.12
```

```bash
docker run -it node:22
```

```bash
docker run -it ubuntu:24.04 bash
```

| Flag | Purpose |
|------|---------|
| `-i` | Interactive mode — keeps STDIN open so you can type input |
| `-t` | Allocates a pseudo-TTY — gives you a proper terminal prompt |

Use case: quick experimentation, testing a snippet, exploring an unfamiliar OS or runtime without installing anything locally.

---

## Case 2: One-Shot Script Execution

Run a script once and throw away the container.

```bash
cat > app.py << 'EOF'
print("hello from python script running in docker")
EOF

docker run --rm -v $(pwd):/app -w /app python:3.12 python app.py
```

```bash
cat > index.js << 'EOF'
console.log("hello from node in docker");
EOF

docker run --rm -v $(pwd):/app -w /app node:22 node index.js
```

| Flag | Purpose |
|------|---------|
| `--rm` | Automatically remove the container after it exits |
| `-v $(pwd):/app` | Bind-mount the current host directory into `/app` inside the container |
| `-w /app` | Set the working directory inside the container |

Use case: running a script in a specific runtime version without installing it, CI pipeline steps, one-off data processing.

---

## Case 3: Running a Service (Web Server, Database)

Start a long-running service in the background.

**Run Nginx:**

```bash
docker run -d --name my-nginx -p 8080:80 nginx:latest
```

**Run PostgreSQL:**

```bash
docker run -d --name my-postgres \
  -e POSTGRES_USER=dev \
  -e POSTGRES_PASSWORD=secret \
  -e POSTGRES_DB=myapp \
  -p 5432:5432 \
  postgres:16
```

**Run Redis:**

```bash
docker run -d --name my-redis -p 6379:6379 redis:7
```

| Flag | Purpose |
|------|---------|
| `-d` | Detached mode — runs the container in the background |
| `--name` | Assign a human-readable name to the container |
| `-p 8080:80` | Map host port 8080 to container port 80 |
| `-e` | Set environment variables inside the container |

Use case: local development dependencies (databases, caches, message brokers) without polluting the host.

---

## Case 4: Building and Running Your Own Application

Package your app into a Docker image using a `Dockerfile`.

**Dockerfile:**

```dockerfile
FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY . .
EXPOSE 8000
CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
```

**Build and run:**

```bash
docker build -t my-api:latest .
docker run -d -p 8000:8000 my-api:latest
```

| Command / Flag | Purpose |
|----------------|---------|
| `docker build -t my-api:latest .` | Build an image from the Dockerfile in `.`, tag it as `my-api:latest` |
| `-t` | Tag the image with a name and optional version |
| `EXPOSE 8000` | Documents the port the app listens on (informational, does not publish) |
| `CMD` | Default command when the container starts |

Use case: packaging an application for deployment, sharing a reproducible build with teammates.

---

## Case 5: Development with Live Code Reload (Bind Mount)

Mount source code into the container so edits on the host are reflected instantly.

```bash
docker run --rm -it \
  -v $(pwd):/app \
  -w /app \
  -p 3000:3000 \
  node:22 \
  sh -c "npm install && npm run dev"
```

```bash
docker run --rm -it \
  -v $(pwd):/app \
  -w /app \
  -p 8000:8000 \
  python:3.12 \
  sh -c "pip install -r requirements.txt && uvicorn main:app --reload --host 0.0.0.0"
```

| Flag | Purpose |
|------|---------|
| `-v $(pwd):/app` | Bind-mount — any file change on host immediately visible inside the container |
| `--reload` | Framework-level flag (uvicorn, nodemon, etc.) to restart on file changes |

Use case: develop locally inside a container with hot-reload, ensuring the dev environment matches production.

---

## Case 6: Multi-Container Application with Docker Compose

Orchestrate multiple services that work together.

**docker-compose.yml:**

```yaml
services:
  web:
    build: .
    ports:
      - "8000:8000"
    volumes:
      - .:/app
    depends_on:
      - db
      - redis
    environment:
      DATABASE_URL: postgresql://dev:secret@db:5432/myapp
      REDIS_URL: redis://redis:6379

  db:
    image: postgres:16
    environment:
      POSTGRES_USER: dev
      POSTGRES_PASSWORD: secret
      POSTGRES_DB: myapp
    volumes:
      - pgdata:/var/lib/postgresql/data
    ports:
      - "5432:5432"

  redis:
    image: redis:7
    ports:
      - "6379:6379"

volumes:
  pgdata:
```

**Commands:**

```bash
docker compose up -d          # start all services in background
docker compose logs -f web    # tail logs of the web service
docker compose down           # stop and remove all containers
docker compose down -v        # also remove named volumes (wipes DB data)
```

| Concept | Purpose |
|---------|---------|
| `depends_on` | Controls startup order (web waits for db and redis) |
| `volumes: pgdata:` | Named volume — data persists across container restarts |
| Service names as hostnames | Containers reference each other by service name (e.g., `db`, `redis`) |

Use case: full-stack local development (app + database + cache + worker), onboarding new developers with a single command.

---

## Case 7: Running Tests in an Isolated Environment

Ensure tests run in a clean, reproducible environment.

```bash
docker run --rm \
  -v $(pwd):/app \
  -w /app \
  python:3.12 \
  sh -c "pip install -r requirements.txt && pytest -v"
```

```bash
docker run --rm \
  -v $(pwd):/app \
  -w /app \
  node:22 \
  sh -c "npm ci && npm test"
```

**With a test database (compose):**

```yaml
services:
  test:
    build: .
    command: pytest -v
    environment:
      DATABASE_URL: postgresql://test:test@testdb:5432/testdb
    depends_on:
      - testdb

  testdb:
    image: postgres:16
    environment:
      POSTGRES_USER: test
      POSTGRES_PASSWORD: test
      POSTGRES_DB: testdb
```

```bash
docker compose -f docker-compose.test.yml up --abort-on-container-exit --exit-code-from test
```

| Flag | Purpose |
|------|---------|
| `--abort-on-container-exit` | Stop all services when any container exits |
| `--exit-code-from test` | Use the exit code of the `test` service as the command's exit code |

Use case: CI pipelines, pre-commit checks, ensuring tests pass regardless of developer machine setup.

---

## Case 8: Debugging Inside a Running Container

Attach a shell to a container that's already running to inspect or debug.

```bash
docker exec -it my-postgres bash
```

```bash
docker exec -it my-postgres psql -U dev -d myapp
```

```bash
docker exec -it my-nginx cat /etc/nginx/nginx.conf
```

**Inspect logs:**

```bash
docker logs my-nginx              # all logs
docker logs -f my-nginx            # follow (tail -f)
docker logs --since 5m my-nginx    # last 5 minutes
```

| Command | Purpose |
|---------|---------|
| `docker exec -it <container> bash` | Open an interactive shell in a running container |
| `docker logs` | View stdout/stderr output of a container |
| `docker inspect <container>` | Dump full container metadata (network, mounts, env, etc.) |

Use case: troubleshooting a misbehaving service, checking config files, running DB migrations manually.

---

## Case 9: Multi-Stage Build for Production Images

Use multi-stage builds to create small, secure production images.

**Go example:**

```dockerfile
# Stage 1: build
FROM golang:1.23 AS builder
WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=0 go build -o /app/server .

# Stage 2: minimal runtime
FROM gcr.io/distroless/static-debian12
COPY --from=builder /app/server /server
EXPOSE 8080
ENTRYPOINT ["/server"]
```

**Node.js example:**

```dockerfile
# Stage 1: build
FROM node:22 AS builder
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY . .
RUN npm run build

# Stage 2: production
FROM node:22-slim
WORKDIR /app
COPY --from=builder /app/dist ./dist
COPY --from=builder /app/node_modules ./node_modules
EXPOSE 3000
CMD ["node", "dist/index.js"]
```

```bash
docker build -t my-app:prod .
docker images my-app:prod    # check the final image size
```

| Concept | Purpose |
|---------|---------|
| `FROM ... AS builder` | Name a build stage so later stages can copy from it |
| `COPY --from=builder` | Copy artifacts from a previous stage into the current one |
| Distroless / slim base | Final image has no compiler, no package manager — smaller and more secure |

Use case: production deployments where image size and attack surface matter.

---

## Case 10: Using Docker as a Tool (No Installation Required)

Run CLI tools without installing them on the host.

**Format code with Black (Python):**

```bash
docker run --rm -v $(pwd):/app -w /app pyfound/black:latest black .
```

**Lint Dockerfiles with Hadolint:**

```bash
docker run --rm -i hadolint/hadolint < Dockerfile
```

**Run AWS CLI:**

```bash
docker run --rm \
  -e AWS_ACCESS_KEY_ID \
  -e AWS_SECRET_ACCESS_KEY \
  -e AWS_DEFAULT_REGION \
  amazon/aws-cli s3 ls
```

**Run Terraform:**

```bash
docker run --rm -it \
  -v $(pwd):/workspace \
  -w /workspace \
  hashicorp/terraform:latest plan
```

| Pattern | Purpose |
|---------|---------|
| Passing env vars with `-e VAR` (no value) | Forwards the host's env var into the container |
| Piping via `-i` | Read from STDIN without a TTY (useful for tools that read files from stdin) |

Use case: using a specific tool version, avoiding global installs, keeping the host machine clean.

---

## Case 11: Persisting Data with Named Volumes

Keep data alive across container restarts and removals.

```bash
docker volume create mydata

docker run -d --name my-postgres \
  -v mydata:/var/lib/postgresql/data \
  -e POSTGRES_PASSWORD=secret \
  postgres:16
```

```bash
docker stop my-postgres && docker rm my-postgres   # container gone
docker run -d --name my-postgres \                  # new container, same data
  -v mydata:/var/lib/postgresql/data \
  -e POSTGRES_PASSWORD=secret \
  postgres:16
```

**Volume management:**

```bash
docker volume ls                 # list all volumes
docker volume inspect mydata     # show mount point and metadata
docker volume rm mydata          # delete (data is gone forever)
docker volume prune              # remove all unused volumes
```

| Concept | Purpose |
|---------|---------|
| Named volume (`mydata:/path`) | Docker manages the storage; data survives container lifecycle |
| Bind mount (`$(pwd):/path`) | Maps a host directory; you manage the storage |
| `docker volume prune` | Clean up orphaned volumes to reclaim disk space |

Use case: databases, file uploads, any stateful service where data must survive container restarts.

---

## Case 12: Networking — Connecting Containers Manually

Create a custom network so containers can communicate by name.

```bash
docker network create my-net

docker run -d --name my-postgres --network my-net \
  -e POSTGRES_PASSWORD=secret \
  postgres:16

docker run -d --name my-app --network my-net \
  -e DATABASE_URL=postgresql://postgres:secret@my-postgres:5432/postgres \
  -p 8000:8000 \
  my-api:latest
```

| Concept | Purpose |
|---------|---------|
| `docker network create` | Create an isolated network for a group of containers |
| `--network my-net` | Attach a container to a specific network |
| Container name as hostname | Containers on the same network resolve each other by name |

Use case: when you need multi-container setups without Docker Compose, or for fine-grained network control.

---

## Case 13: Pushing Images to a Registry

Share your images with a team or deploy to production.

**Docker Hub:**

```bash
docker build -t myuser/my-api:1.0 .
docker login
docker push myuser/my-api:1.0
```

**Private registry (e.g., AWS ECR):**

```bash
aws ecr get-login-password --region us-east-1 | \
  docker login --username AWS --password-stdin 123456789.dkr.ecr.us-east-1.amazonaws.com

docker build -t 123456789.dkr.ecr.us-east-1.amazonaws.com/my-api:1.0 .
docker push 123456789.dkr.ecr.us-east-1.amazonaws.com/my-api:1.0
```

| Command | Purpose |
|---------|---------|
| `docker login` | Authenticate to a registry |
| `docker push` | Upload a local image to the registry |
| `docker pull` | Download an image from a registry |

Use case: CI/CD pipelines, deploying to Kubernetes or ECS, sharing images across teams.

---

## Case 14: Resource Limits and Health Checks

Control how much CPU and memory a container can use, and monitor its health.

```bash
docker run -d --name my-app \
  --memory=512m \
  --cpus=1.5 \
  -p 8000:8000 \
  my-api:latest
```

**Health check in Dockerfile:**

```dockerfile
HEALTHCHECK --interval=30s --timeout=5s --retries=3 \
  CMD curl -f http://localhost:8000/health || exit 1
```

**Check health status:**

```bash
docker inspect --format='{{.State.Health.Status}}' my-app
```

| Flag / Directive | Purpose |
|------------------|---------|
| `--memory=512m` | Limit container to 512 MB of RAM |
| `--cpus=1.5` | Limit container to 1.5 CPU cores |
| `HEALTHCHECK` | Periodically run a command to check if the service is healthy |

Use case: preventing a runaway process from eating all host resources, container orchestration readiness.

---

## Quick Reference: Common Docker Commands

| Command | What It Does |
|---------|--------------|
| `docker ps` | List running containers |
| `docker ps -a` | List all containers (including stopped) |
| `docker images` | List local images |
| `docker stop <name>` | Gracefully stop a container |
| `docker rm <name>` | Remove a stopped container |
| `docker rmi <image>` | Remove a local image |
| `docker system prune -a` | Remove all stopped containers, unused images, and build cache |
| `docker stats` | Live resource usage of running containers |
| `docker cp <container>:/path ./local` | Copy files from a container to the host |
