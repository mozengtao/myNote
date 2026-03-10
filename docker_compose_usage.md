# Docker Compose Usage Scenarios for Developers

A practical reference of how Docker Compose is used in real development workflows.

---

## Compose File Basics

A `docker-compose.yml` (or `compose.yml`) defines services, networks, and volumes as a single unit. Compose reads this file and manages the full lifecycle.

**Minimal example:**

```yaml
services:
  web:
    image: nginx:latest
    ports:
      - "8080:80"
```

**Core commands:**

```bash
docker compose up -d            # start everything in background
docker compose down              # stop and remove containers + default network
docker compose ps                # list running services
docker compose logs -f           # follow all service logs
docker compose restart web       # restart a single service
```

---

## Scenario 1: Web App + Database (Classic Two-Tier)

The most common developer setup — an application that talks to a database.

```yaml
services:
  app:
    build: .
    ports:
      - "8000:8000"
    environment:
      DATABASE_URL: postgresql://dev:secret@db:5432/myapp
    depends_on:
      db:
        condition: service_healthy
    restart: unless-stopped

  db:
    image: postgres:16
    environment:
      POSTGRES_USER: dev
      POSTGRES_PASSWORD: secret
      POSTGRES_DB: myapp
    ports:
      - "5432:5432"
    volumes:
      - pgdata:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U dev -d myapp"]
      interval: 5s
      timeout: 3s
      retries: 5

volumes:
  pgdata:
```

| Concept | Purpose |
|---------|---------|
| `depends_on` with `condition` | App waits until DB is actually ready, not just started |
| `healthcheck` on db | Compose uses this to determine when `service_healthy` is satisfied |
| Named volume `pgdata` | Database data survives `docker compose down` (but not `down -v`) |
| Service name as hostname | App connects to `db:5432` — Compose creates DNS automatically |

---

## Scenario 2: Full-Stack App (Frontend + Backend + DB + Cache)

A realistic multi-service stack for a web application.

```yaml
services:
  frontend:
    build:
      context: ./frontend
      dockerfile: Dockerfile
    ports:
      - "3000:3000"
    volumes:
      - ./frontend/src:/app/src
    environment:
      REACT_APP_API_URL: http://localhost:8000
    depends_on:
      - backend

  backend:
    build:
      context: ./backend
      dockerfile: Dockerfile
    ports:
      - "8000:8000"
    volumes:
      - ./backend:/app
    environment:
      DATABASE_URL: postgresql://dev:secret@db:5432/myapp
      REDIS_URL: redis://redis:6379/0
      SECRET_KEY: dev-only-secret
    depends_on:
      db:
        condition: service_healthy
      redis:
        condition: service_started

  db:
    image: postgres:16
    environment:
      POSTGRES_USER: dev
      POSTGRES_PASSWORD: secret
      POSTGRES_DB: myapp
    volumes:
      - pgdata:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U dev"]
      interval: 5s
      retries: 5

  redis:
    image: redis:7-alpine
    ports:
      - "6379:6379"

volumes:
  pgdata:
```

| Concept | Purpose |
|---------|---------|
| Separate `build.context` | Each service builds from its own directory |
| Bind mount for source (`./backend:/app`) | Live code reload during development |
| Mixed dependency conditions | `service_healthy` for DB, `service_started` for Redis |

---

## Scenario 3: Development with Live Reload

Mount source code into containers so every file save is reflected immediately.

```yaml
services:
  api:
    build: .
    ports:
      - "8000:8000"
    volumes:
      - .:/app
      - /app/__pycache__
      - /app/.venv
    command: uvicorn main:app --reload --host 0.0.0.0 --port 8000
    environment:
      DEBUG: "true"
      DATABASE_URL: postgresql://dev:secret@db:5432/myapp
    depends_on:
      - db

  db:
    image: postgres:16
    environment:
      POSTGRES_USER: dev
      POSTGRES_PASSWORD: secret
      POSTGRES_DB: myapp
    volumes:
      - pgdata:/var/lib/postgresql/data

volumes:
  pgdata:
```

| Concept | Purpose |
|---------|---------|
| `.:/app` bind mount | Host file changes are instantly visible in the container |
| `/app/__pycache__` anonymous volume | Exclude generated files — container's copy is used, not the host's |
| `command:` override | Replace the Dockerfile CMD with a dev-specific command (`--reload`) |

---

## Scenario 4: Running Tests with a Disposable Database

Spin up a clean test environment, run tests, tear everything down.

```yaml
# docker-compose.test.yml
services:
  test:
    build: .
    command: pytest -v --tb=short
    environment:
      DATABASE_URL: postgresql://test:test@testdb:5432/testdb
      TESTING: "true"
    depends_on:
      testdb:
        condition: service_healthy

  testdb:
    image: postgres:16
    environment:
      POSTGRES_USER: test
      POSTGRES_PASSWORD: test
      POSTGRES_DB: testdb
    tmpfs:
      - /var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U test"]
      interval: 3s
      retries: 5
```

**Run:**

```bash
docker compose -f docker-compose.test.yml up \
  --build \
  --abort-on-container-exit \
  --exit-code-from test
```

| Concept | Purpose |
|---------|---------|
| `-f docker-compose.test.yml` | Use a separate compose file for testing |
| `--abort-on-container-exit` | Stop all services when the test container exits |
| `--exit-code-from test` | Propagate the test container's exit code (useful for CI) |
| `tmpfs` on testdb | Database runs entirely in memory — fast and disposable |

---

## Scenario 5: Multiple Compose Files (Dev / Prod Override)

Layer compose files to share a common base while customizing per environment.

**docker-compose.yml** (base):

```yaml
services:
  app:
    build: .
    environment:
      DATABASE_URL: postgresql://user:pass@db:5432/myapp
    depends_on:
      - db

  db:
    image: postgres:16
    environment:
      POSTGRES_USER: user
      POSTGRES_PASSWORD: pass
      POSTGRES_DB: myapp
    volumes:
      - pgdata:/var/lib/postgresql/data

volumes:
  pgdata:
```

**docker-compose.override.yml** (auto-loaded, dev settings):

```yaml
services:
  app:
    ports:
      - "8000:8000"
    volumes:
      - .:/app
    command: uvicorn main:app --reload --host 0.0.0.0
    environment:
      DEBUG: "true"

  db:
    ports:
      - "5432:5432"
```

**docker-compose.prod.yml** (production override):

```yaml
services:
  app:
    image: registry.example.com/my-app:latest
    restart: always
    environment:
      DEBUG: "false"
    deploy:
      replicas: 2
      resources:
        limits:
          memory: 512M
          cpus: "1.0"
```

**Commands:**

```bash
# Dev: automatically merges docker-compose.yml + docker-compose.override.yml
docker compose up -d

# Prod: explicitly select base + prod (skips override)
docker compose -f docker-compose.yml -f docker-compose.prod.yml up -d

# Verify the merged config
docker compose -f docker-compose.yml -f docker-compose.prod.yml config
```

| Concept | Purpose |
|---------|---------|
| `docker-compose.override.yml` | Auto-merged with the base file — no `-f` flag needed |
| Multiple `-f` flags | Files are merged left to right; later files override earlier ones |
| `docker compose config` | Print the final merged YAML — useful for debugging |

---

## Scenario 6: Background Workers and Job Queues

Run async workers alongside your API using the same codebase.

```yaml
services:
  api:
    build: .
    command: uvicorn main:app --host 0.0.0.0 --port 8000
    ports:
      - "8000:8000"
    environment: &common-env
      DATABASE_URL: postgresql://dev:secret@db:5432/myapp
      REDIS_URL: redis://redis:6379/0
    depends_on:
      - db
      - redis

  worker:
    build: .
    command: celery -A tasks worker --loglevel=info --concurrency=4
    environment: *common-env
    depends_on:
      - db
      - redis

  beat:
    build: .
    command: celery -A tasks beat --loglevel=info
    environment: *common-env
    depends_on:
      - redis

  redis:
    image: redis:7-alpine

  db:
    image: postgres:16
    environment:
      POSTGRES_USER: dev
      POSTGRES_PASSWORD: secret
      POSTGRES_DB: myapp
    volumes:
      - pgdata:/var/lib/postgresql/data

volumes:
  pgdata:
```

| Concept | Purpose |
|---------|---------|
| Same `build: .` for api/worker/beat | All use the same image, different `command` |
| YAML anchors (`&common-env` / `*common-env`) | Reuse identical environment blocks without duplication |
| `beat` service | Celery scheduler for periodic tasks, separate from workers |

---

## Scenario 7: Using Environment Files

Keep secrets and config out of the compose file.

**.env:**

```
POSTGRES_USER=dev
POSTGRES_PASSWORD=secret
POSTGRES_DB=myapp
APP_SECRET_KEY=super-secret-key-123
```

**docker-compose.yml:**

```yaml
services:
  app:
    build: .
    ports:
      - "8000:8000"
    env_file:
      - .env
    environment:
      DATABASE_URL: postgresql://${POSTGRES_USER}:${POSTGRES_PASSWORD}@db:5432/${POSTGRES_DB}

  db:
    image: postgres:16
    env_file:
      - .env
    volumes:
      - pgdata:/var/lib/postgresql/data

volumes:
  pgdata:
```

| Concept | Purpose |
|---------|---------|
| `.env` file in project root | Compose automatically loads it for variable interpolation (`${VAR}`) |
| `env_file:` directive | Passes all variables from the file as container environment variables |
| `environment:` with `${VAR}` | Interpolate host/`.env` variables into compose values |
| `.env` in `.gitignore` | Never commit secrets — provide a `.env.example` template instead |

---

## Scenario 8: Database Initialization with SQL Scripts

Seed a database automatically on first run.

```yaml
services:
  db:
    image: postgres:16
    environment:
      POSTGRES_USER: dev
      POSTGRES_PASSWORD: secret
      POSTGRES_DB: myapp
    volumes:
      - pgdata:/var/lib/postgresql/data
      - ./init-scripts:/docker-entrypoint-initdb.d
    ports:
      - "5432:5432"

volumes:
  pgdata:
```

**init-scripts/01-schema.sql:**

```sql
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    email VARCHAR(255) UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT NOW()
);
```

**init-scripts/02-seed.sql:**

```sql
INSERT INTO users (email) VALUES
    ('alice@example.com'),
    ('bob@example.com');
```

| Concept | Purpose |
|---------|---------|
| `/docker-entrypoint-initdb.d` | Postgres runs `.sql`, `.sql.gz`, and `.sh` files here on first startup |
| Numbered filenames | Scripts execute in alphabetical order |
| Only on first run | Scripts are skipped if the data volume already has a database |

---

## Scenario 9: Reverse Proxy with Multiple Services

Use Nginx or Traefik as a gateway to route traffic to different services.

```yaml
services:
  proxy:
    image: nginx:latest
    ports:
      - "80:80"
    volumes:
      - ./nginx.conf:/etc/nginx/conf.d/default.conf:ro
    depends_on:
      - frontend
      - api

  frontend:
    build: ./frontend
    expose:
      - "3000"

  api:
    build: ./backend
    expose:
      - "8000"
    environment:
      DATABASE_URL: postgresql://dev:secret@db:5432/myapp
    depends_on:
      - db

  db:
    image: postgres:16
    environment:
      POSTGRES_USER: dev
      POSTGRES_PASSWORD: secret
      POSTGRES_DB: myapp
    volumes:
      - pgdata:/var/lib/postgresql/data

volumes:
  pgdata:
```

**nginx.conf:**

```nginx
server {
    listen 80;

    location / {
        proxy_pass http://frontend:3000;
    }

    location /api/ {
        proxy_pass http://api:8000/;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

| Concept | Purpose |
|---------|---------|
| `expose` vs `ports` | `expose` makes port visible to other containers only, `ports` publishes to host |
| Service names in `proxy_pass` | Compose DNS resolves `frontend` and `api` to the right containers |
| `:ro` mount flag | Read-only mount — container cannot modify the nginx config |

---

## Scenario 10: Custom Networks for Isolation

Separate services into network segments so they can only reach what they need.

```yaml
services:
  frontend:
    build: ./frontend
    ports:
      - "3000:3000"
    networks:
      - frontend-net

  api:
    build: ./backend
    ports:
      - "8000:8000"
    networks:
      - frontend-net
      - backend-net

  db:
    image: postgres:16
    environment:
      POSTGRES_USER: dev
      POSTGRES_PASSWORD: secret
      POSTGRES_DB: myapp
    volumes:
      - pgdata:/var/lib/postgresql/data
    networks:
      - backend-net

networks:
  frontend-net:
  backend-net:

volumes:
  pgdata:
```

| Concept | Purpose |
|---------|---------|
| `frontend-net` | Frontend and API can communicate |
| `backend-net` | API and DB can communicate |
| Frontend cannot reach DB | They share no network — enforced isolation |

---

## Scenario 11: Building with Build Args and Target Stages

Control the build process from the compose file.

**Dockerfile:**

```dockerfile
FROM node:22 AS base
WORKDIR /app
COPY package*.json ./
RUN npm ci

FROM base AS development
COPY . .
CMD ["npm", "run", "dev"]

FROM base AS production
COPY . .
RUN npm run build
CMD ["npm", "start"]
```

**docker-compose.yml:**

```yaml
services:
  app:
    build:
      context: .
      dockerfile: Dockerfile
      target: development
      args:
        NODE_ENV: development
        BUILD_VERSION: ${BUILD_VERSION:-dev}
    ports:
      - "3000:3000"
    volumes:
      - .:/app
      - /app/node_modules
```

**Production build:**

```yaml
# docker-compose.prod.yml
services:
  app:
    build:
      target: production
      args:
        NODE_ENV: production
        BUILD_VERSION: ${BUILD_VERSION}
    ports:
      - "3000:3000"
```

| Concept | Purpose |
|---------|---------|
| `target: development` | Stop the multi-stage build at the named stage |
| `args:` | Pass build-time variables to the Dockerfile's `ARG` instructions |
| `${BUILD_VERSION:-dev}` | Use env var with a default fallback |
| `/app/node_modules` anonymous volume | Prevent host bind mount from overwriting container's `node_modules` |

---

## Scenario 12: Resource Limits and Restart Policies

Control container behavior for stability.

```yaml
services:
  api:
    build: .
    ports:
      - "8000:8000"
    restart: unless-stopped
    deploy:
      resources:
        limits:
          memory: 512M
          cpus: "1.0"
        reservations:
          memory: 256M
          cpus: "0.5"
    logging:
      driver: json-file
      options:
        max-size: "10m"
        max-file: "3"

  db:
    image: postgres:16
    restart: always
    deploy:
      resources:
        limits:
          memory: 1G
    shm_size: "256m"
```

| Concept | Purpose |
|---------|---------|
| `restart: unless-stopped` | Auto-restart on crash, but not if you manually stopped it |
| `restart: always` | Always restart — even after `docker compose stop` + daemon restart |
| `deploy.resources.limits` | Hard ceiling on memory and CPU |
| `deploy.resources.reservations` | Guaranteed minimum resources |
| `shm_size` | Shared memory size — Postgres needs this for large queries |
| `logging` options | Cap log file size to prevent filling the disk |

---

## Scenario 13: Running One-Off Commands and Migrations

Use `docker compose run` and `docker compose exec` for ad-hoc tasks.

```bash
# Run a one-off command in a NEW container (inherits env, networks, volumes)
docker compose run --rm app python manage.py migrate
docker compose run --rm app python manage.py createsuperuser
docker compose run --rm app bash

# Run a command in an ALREADY RUNNING container
docker compose exec app python manage.py shell
docker compose exec db psql -U dev -d myapp

# Run with a different user
docker compose exec --user root app bash

# No TTY (for scripts and CI)
docker compose run --rm -T app pytest > results.txt
```

| Command | Behavior |
|---------|----------|
| `docker compose run --rm service cmd` | Creates a new container, runs the command, removes it |
| `docker compose exec service cmd` | Runs inside an existing running container |
| `-T` flag | Disable TTY allocation — needed when piping output in scripts |
| `run` inherits config | Volumes, networks, env vars all come from the compose file |

---

## Scenario 14: Profiles for Optional Services

Define services that only start when explicitly requested.

```yaml
services:
  app:
    build: .
    ports:
      - "8000:8000"
    environment:
      DATABASE_URL: postgresql://dev:secret@db:5432/myapp

  db:
    image: postgres:16
    environment:
      POSTGRES_USER: dev
      POSTGRES_PASSWORD: secret
      POSTGRES_DB: myapp
    volumes:
      - pgdata:/var/lib/postgresql/data

  adminer:
    image: adminer
    ports:
      - "9090:8080"
    profiles:
      - debug

  mailhog:
    image: mailhog/mailhog
    ports:
      - "1025:1025"
      - "8025:8025"
    profiles:
      - debug

  prometheus:
    image: prom/prometheus
    ports:
      - "9091:9090"
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml:ro
    profiles:
      - monitoring

volumes:
  pgdata:
```

**Commands:**

```bash
# Normal startup — only app + db
docker compose up -d

# Include debug tools
docker compose --profile debug up -d

# Include multiple profiles
docker compose --profile debug --profile monitoring up -d
```

| Concept | Purpose |
|---------|---------|
| `profiles:` | Service is excluded from default `up` unless its profile is activated |
| Services without profiles | Always start (app, db in this example) |
| `--profile debug` | Opt-in to extra services like DB admin UI, mail catcher |

---

## Scenario 15: Extending Services with `extends`

Reuse common service configuration across compose files.

**common.yml:**

```yaml
services:
  base-python:
    build: .
    environment:
      PYTHONDONTWRITEBYTECODE: "1"
      PYTHONUNBUFFERED: "1"
    volumes:
      - .:/app
    working_dir: /app
```

**docker-compose.yml:**

```yaml
services:
  api:
    extends:
      file: common.yml
      service: base-python
    command: uvicorn main:app --reload --host 0.0.0.0
    ports:
      - "8000:8000"
    environment:
      DATABASE_URL: postgresql://dev:secret@db:5432/myapp

  worker:
    extends:
      file: common.yml
      service: base-python
    command: celery -A tasks worker --loglevel=info
    environment:
      DATABASE_URL: postgresql://dev:secret@db:5432/myapp

  db:
    image: postgres:16
    environment:
      POSTGRES_USER: dev
      POSTGRES_PASSWORD: secret
      POSTGRES_DB: myapp
```

| Concept | Purpose |
|---------|---------|
| `extends.file` | Reference a service definition from another file |
| `extends.service` | Which service to inherit from |
| Local keys merge/override | Local `environment`, `command`, `ports` extend or replace the base |

---

## Scenario 16: Watch Mode (Auto-Rebuild on Change)

Compose Watch automatically syncs files or rebuilds when source changes.

```yaml
services:
  api:
    build: .
    ports:
      - "8000:8000"
    develop:
      watch:
        - action: sync
          path: ./src
          target: /app/src

        - action: rebuild
          path: ./requirements.txt

        - action: sync+restart
          path: ./config
          target: /app/config
```

**Run:**

```bash
docker compose watch
```

| Action | Behavior |
|--------|----------|
| `sync` | Copy changed files into the running container (like a live bind mount) |
| `rebuild` | Rebuild the image and recreate the container (for dependency changes) |
| `sync+restart` | Sync files then restart the container (for config changes that need a process restart) |

Use case: alternative to bind mounts that works better on macOS (no filesystem performance issues) and gives fine-grained control over what triggers a rebuild.

---

## Quick Reference: Common Compose Commands

| Command | What It Does |
|---------|--------------|
| `docker compose up -d` | Start all services in background |
| `docker compose up --build` | Rebuild images before starting |
| `docker compose down` | Stop and remove containers + default network |
| `docker compose down -v` | Also remove named volumes (wipes data) |
| `docker compose ps` | List running services and their ports |
| `docker compose logs -f` | Follow logs from all services |
| `docker compose logs -f api db` | Follow logs from specific services |
| `docker compose restart api` | Restart a single service |
| `docker compose stop` | Stop without removing containers |
| `docker compose pull` | Pull latest images for all services |
| `docker compose build --no-cache` | Force full rebuild of all images |
| `docker compose run --rm api bash` | One-off command in a new container |
| `docker compose exec api bash` | Shell into a running container |
| `docker compose config` | Validate and print the merged compose file |
| `docker compose top` | Show running processes in each container |

---

## Quick Reference: Compose File Top-Level Keys

```yaml
version: "3.8"     # optional in modern Compose, can be omitted

services:           # container definitions
  app:
    ...

volumes:            # named volumes
  pgdata:

networks:           # custom networks
  backend:

secrets:            # sensitive data (swarm mode or Docker Desktop)
  db_password:
    file: ./secrets/db_password.txt

configs:            # non-sensitive config files (swarm mode)
  nginx_conf:
    file: ./nginx.conf
```
