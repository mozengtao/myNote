# Docker Usage Scenarios for Developers

A practical reference of the most common ways developers use Docker, organized by intent. Each case shows the real command, what it does, and when you'd reach for it.

---

## Case 1: Interactive Environment

Spin up a language runtime to experiment, debug, or explore.

```bash
docker run -it python:3.12
```

| Flag | Purpose |
|------|---------|
| `-i` | Interactive mode — keeps STDIN open so you can type commands |
| `-t` | Allocates a pseudo-TTY for a proper terminal experience |

**When to use:** Quick REPL session, testing a snippet, checking what version of a package is available, or exploring an unfamiliar runtime without polluting your host.

**Variations:**

```bash
# Drop into a bash shell inside an Ubuntu container
docker run -it ubuntu:24.04 bash

# Interactive Node.js REPL
docker run -it node:22 node

# Interactive shell with your project mounted
docker run -it -v $(pwd):/work -w /work golang:1.22 bash
```

---

## Case 2: One-Shot Script Execution

Run a script once and throw the container away.

```bash
cat > app.py << 'EOF'
print("hello from python script running in docker")
EOF

docker run --rm -v $(pwd):/app -w /app python:3.12 python app.py
```

| Flag | Purpose |
|------|---------|
| `--rm` | Automatically remove the container after it exits — no leftover stopped containers |
| `-v $(pwd):/app` | Bind-mount the current directory into `/app` inside the container |
| `-w /app` | Set the working directory inside the container to `/app` |

**When to use:** Running a script in a specific runtime version, CI pipelines, one-off data processing, or testing your code in a clean environment.

**Variations:**

```bash
# Run a Go program without installing Go
docker run --rm -v $(pwd):/app -w /app golang:1.22 go run main.go

# Format code with a specific tool version
docker run --rm -v $(pwd):/src -w /src mvdan/shfmt:latest -w .

# Run a linter
docker run --rm -v $(pwd):/app -w /app golangci/golangci-lint:latest golangci-lint run
```

---

## Case 3: Running a Background Service

Start a long-running process (web server, API, worker) in the background.

```bash
docker run -d -p 8080:80 --name my-nginx nginx:latest
```

| Flag | Purpose |
|------|---------|
| `-d` | Detached mode — runs the container in the background |
| `-p 8080:80` | Maps host port 8080 to container port 80 |
| `--name my-nginx` | Assigns a human-readable name for easy reference |

**When to use:** Hosting a service locally for development, running a reverse proxy, serving static files, or running a background worker.

**Lifecycle commands:**

```bash
docker logs my-nginx          # view output
docker logs -f my-nginx       # follow/tail logs in real time
docker stop my-nginx           # graceful shutdown
docker start my-nginx          # restart a stopped container
docker rm my-nginx             # remove the container
```

---

## Case 4: Running a Database Locally

Spin up a database with persistent storage — no install, no config files on the host.

```bash
docker run -d \
  --name postgres-dev \
  -e POSTGRES_USER=dev \
  -e POSTGRES_PASSWORD=secret \
  -e POSTGRES_DB=myapp \
  -p 5432:5432 \
  -v pgdata:/var/lib/postgresql/data \
  postgres:16
```

| Flag | Purpose |
|------|---------|
| `-e` | Sets environment variables inside the container (used for DB config) |
| `-v pgdata:/var/lib/...` | Named volume — persists data across container restarts and removals |
| `-p 5432:5432` | Exposes the database on localhost for your app to connect |

**When to use:** Local development databases, testing migrations, reproducing production data issues.

**Other databases:**

```bash
# Redis
docker run -d --name redis-dev -p 6379:6379 redis:7

# MySQL
docker run -d --name mysql-dev \
  -e MYSQL_ROOT_PASSWORD=secret \
  -e MYSQL_DATABASE=myapp \
  -p 3306:3306 \
  mysql:8

# MongoDB
docker run -d --name mongo-dev -p 27017:27017 mongo:7
```

**Connecting to the running database:**

```bash
# Psql into the running Postgres container
docker exec -it postgres-dev psql -U dev -d myapp
```

---

## Case 5: Building a Custom Image

Package your application into a reproducible, distributable image.

```Dockerfile
# Dockerfile
FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY . .
EXPOSE 8000
CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
```

```bash
docker build -t myapp:latest .
docker run -d -p 8000:8000 myapp:latest
```

| Flag | Purpose |
|------|---------|
| `-t myapp:latest` | Tags the image with a name and version |
| `.` | Build context — the directory Docker sends to the daemon |

**When to use:** Preparing for deployment, sharing reproducible environments with teammates, CI/CD pipelines.

**Multi-stage build (smaller production images):**

```Dockerfile
# Build stage
FROM golang:1.22 AS builder
WORKDIR /app
COPY . .
RUN CGO_ENABLED=0 go build -o server .

# Runtime stage — only the binary, no compiler
FROM alpine:3.19
COPY --from=builder /app/server /server
CMD ["/server"]
```

```bash
docker build -t myapp:prod .
```

---

## Case 6: Development with Hot-Reload

Mount your source code into a container so changes on the host are reflected immediately.

```bash
docker run --rm \
  -v $(pwd):/app \
  -w /app \
  -p 3000:3000 \
  node:22 \
  npx nodemon server.js
```

| Flag | Purpose |
|------|---------|
| `-v $(pwd):/app` | Bind-mount so edits on host appear instantly in the container |
| `-p 3000:3000` | Expose the dev server to your browser |

**When to use:** Frontend/backend development where you want a consistent runtime but fast feedback. Your editor stays on the host; the code runs in the container.

**Python example with Flask:**

```bash
docker run --rm \
  -v $(pwd):/app \
  -w /app \
  -p 5000:5000 \
  -e FLASK_DEBUG=1 \
  python:3.12 \
  bash -c "pip install flask && flask run --host=0.0.0.0"
```

---

## Case 7: Multi-Container with Docker Compose

Orchestrate multiple services (app + database + cache) together.

```yaml
# compose.yaml
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

  redis:
    image: redis:7

volumes:
  pgdata:
```

```bash
docker compose up -d          # start all services in background
docker compose logs -f web    # follow logs for one service
docker compose down            # stop and remove all containers
docker compose down -v         # also remove named volumes (wipes data)
```

**When to use:** Any project with more than one service. Replaces a dozen `docker run` commands with a single config file.

---

## Case 8: Running Tests in Isolation

Execute your test suite in a clean environment identical to CI.

```bash
docker run --rm \
  -v $(pwd):/app \
  -w /app \
  python:3.12 \
  bash -c "pip install -r requirements.txt && pytest -v"
```

**When to use:** Reproducing CI failures locally, ensuring tests pass in a clean environment, testing against a specific runtime version.

**Testing against multiple versions:**

```bash
for version in 3.10 3.11 3.12; do
  echo "=== Python $version ==="
  docker run --rm -v $(pwd):/app -w /app python:$version \
    bash -c "pip install -r requirements.txt && pytest -v"
done
```

---

## Case 9: Debugging Inside a Container

Attach to or exec into a running container to inspect its state.

```bash
# Get a shell inside a running container
docker exec -it my-nginx bash

# If the image has no bash, try sh
docker exec -it my-nginx sh

# Run a specific diagnostic command
docker exec my-nginx cat /etc/nginx/nginx.conf

# Inspect a stopped/crashed container's filesystem
docker run -it --entrypoint sh myapp:latest
```

| Flag | Purpose |
|------|---------|
| `exec` | Run a command in an already-running container |
| `--entrypoint sh` | Override the default startup command to get a shell instead |

**When to use:** Investigating why a container is misbehaving, checking config files, verifying environment variables, testing network connectivity from inside.

**Inspecting container metadata:**

```bash
docker inspect my-nginx              # full JSON metadata
docker inspect -f '{{.State.Status}}' my-nginx   # just the status
docker stats my-nginx                 # live CPU/memory/network usage
docker top my-nginx                   # processes inside the container
```

---

## Case 10: Networking Between Containers

Connect containers so they can talk to each other by name.

```bash
# Create a network
docker network create mynet

# Run services on that network
docker run -d --name api --network mynet myapp:latest
docker run -d --name db --network mynet postgres:16

# Inside the api container, 'db' resolves to the postgres container
# e.g., connection string: postgresql://dev:secret@db:5432/myapp
```

| Flag | Purpose |
|------|---------|
| `--network mynet` | Attach container to a user-defined network |

**When to use:** When you need containers to communicate without compose, or when adding a temporary debug container to an existing network.

**Debugging network issues:**

```bash
# Attach a throwaway container to test connectivity
docker run --rm -it --network mynet alpine sh
# then: ping db, wget api:8000/health, etc.
```

---

## Case 11: Copying Files In and Out

Transfer files between the host and a container without volumes.

```bash
# Copy a file from host into a running container
docker cp config.yaml my-nginx:/etc/nginx/conf.d/

# Copy a file out of a container to the host
docker cp my-nginx:/var/log/nginx/access.log ./access.log

# Copy from a stopped container (works the same way)
docker cp my-nginx:/app/data.db ./backup-data.db
```

**When to use:** Extracting logs or build artifacts, injecting config into a running container for a quick test, recovering data from a stopped container.

---

## Case 12: Environment Variable Injection

Pass configuration to containers at runtime.

```bash
# Inline variables
docker run --rm \
  -e API_KEY=abc123 \
  -e DEBUG=true \
  myapp:latest

# From a .env file
docker run --rm --env-file .env myapp:latest
```

| Flag | Purpose |
|------|---------|
| `-e KEY=VALUE` | Set a single environment variable |
| `--env-file .env` | Load variables from a file (one `KEY=VALUE` per line) |

**When to use:** Configuring apps for different environments (dev/staging/prod), passing secrets, toggling feature flags.

---

## Case 13: Resource Limits

Constrain CPU and memory to prevent a container from starving the host.

```bash
docker run -d \
  --name worker \
  --memory=512m \
  --cpus=1.5 \
  myapp:latest
```

| Flag | Purpose |
|------|---------|
| `--memory=512m` | Hard limit on RAM (container is killed if it exceeds this) |
| `--cpus=1.5` | Limit to 1.5 CPU cores worth of compute |

**When to use:** Running untrusted code, simulating production resource constraints, preventing runaway processes during development.

---

## Case 14: Cleaning Up

Reclaim disk space from unused images, containers, and volumes.

```bash
# Remove all stopped containers
docker container prune

# Remove unused images (dangling layers from builds)
docker image prune

# Nuclear option — remove everything unused
docker system prune -a --volumes
```

| Flag | Purpose |
|------|---------|
| `prune` | Remove resources that are not currently in use |
| `-a` | Also remove images not referenced by any container |
| `--volumes` | Also remove anonymous volumes |

**When to use:** When `docker system df` shows gigabytes of reclaimable space, or when builds start failing due to disk pressure.

---

## Quick Reference: Flag Cheat Sheet

| Flag | Short For | Example |
|------|-----------|---------|
| `-i` | `--interactive` | Keep STDIN open |
| `-t` | `--tty` | Allocate pseudo-TTY |
| `-d` | `--detach` | Run in background |
| `-p` | `--publish` | `host:container` port mapping |
| `-v` | `--volume` | `host:container` bind mount or named volume |
| `-e` | `--env` | Set environment variable |
| `-w` | `--workdir` | Set working directory inside container |
| `--rm` | | Auto-remove container on exit |
| `--name` | | Assign a name to the container |
| `--network` | | Connect to a Docker network |
| `--env-file` | | Load env vars from a file |
| `--cpus` | | CPU limit |
| `--memory` | | Memory limit |
| `--entrypoint` | | Override default entrypoint |
