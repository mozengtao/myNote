# Dockerfile Instructions Reference

A practical guide to every Dockerfile instruction with real-world examples.

---

## FROM

Create a new build stage from a base image. This is always the first instruction in a Dockerfile (except `ARG` used before `FROM`).

```dockerfile
# Simple base image
FROM ubuntu:24.04

# Specific Python version
FROM python:3.12-slim

# Multi-stage build with named stages
FROM golang:1.23 AS builder
FROM gcr.io/distroless/static-debian12 AS runtime

# Use ARG before FROM to parameterize the base image
ARG BASE_IMAGE=python:3.12-slim
FROM ${BASE_IMAGE}

# Start from scratch (completely empty image, for statically-linked binaries)
FROM scratch
```

| Form | Behavior |
|------|----------|
| `FROM image` | Use latest tag |
| `FROM image:tag` | Use specific version |
| `FROM image AS name` | Name the stage for `COPY --from=name` |
| `FROM scratch` | Empty base — nothing in the filesystem |

---

## ARG

Define build-time variables that users can pass with `docker build --build-arg`.

```dockerfile
# Declare with a default value
ARG PYTHON_VERSION=3.12
FROM python:${PYTHON_VERSION}-slim

# ARG after FROM must be re-declared to use inside the build stage
ARG APP_ENV=production

# Multiple ARGs
ARG GITHUB_TOKEN
ARG BUILD_DATE

RUN echo "Building for ${APP_ENV} on ${BUILD_DATE}"
```

**Build command:**

```bash
docker build \
  --build-arg PYTHON_VERSION=3.11 \
  --build-arg APP_ENV=staging \
  --build-arg BUILD_DATE=$(date -u +%Y-%m-%dT%H:%M:%SZ) \
  -t my-app .
```

| Key Point | Detail |
|-----------|--------|
| Scope | ARG before `FROM` is only available in `FROM`. ARG after `FROM` is available in that stage only |
| Not persisted | ARG values do NOT survive into the running container (use ENV for that) |
| Secrets warning | ARG values are visible in `docker history` — do NOT pass passwords this way |

---

## ENV

Set environment variables that persist in the built image and running container.

```dockerfile
FROM python:3.12-slim

# Single variable
ENV APP_ENV=production

# Multiple variables
ENV APP_HOME=/app \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_NO_CACHE_DIR=1

WORKDIR ${APP_HOME}
```

**Override at runtime:**

```bash
docker run -e APP_ENV=staging my-app
```

| ARG vs ENV | |
|------------|--|
| `ARG` | Available only at build time, not in the running container |
| `ENV` | Available at both build time and runtime, baked into the image |

---

## WORKDIR

Set the working directory for all subsequent `RUN`, `CMD`, `ENTRYPOINT`, `COPY`, and `ADD` instructions.

```dockerfile
FROM node:22

# Absolute path — creates the directory if it doesn't exist
WORKDIR /app

# Subsequent WORKDIR is relative to the previous one
WORKDIR src
# Now working directory is /app/src

COPY package.json .
RUN npm install
```

| Key Point | Detail |
|-----------|--------|
| Auto-creates | The directory is created if it doesn't exist |
| Prefer over `RUN mkdir` | Cleaner and sets context for following instructions |
| Multiple WORKDIRs | Each is relative to the last (unless absolute) |

---

## COPY

Copy files and directories from the build context (host) into the image.

```dockerfile
FROM python:3.12-slim
WORKDIR /app

# Copy a single file
COPY requirements.txt .

# Copy everything (respects .dockerignore)
COPY . .

# Copy with ownership set directly
COPY --chown=appuser:appuser . .

# Copy from a named build stage (multi-stage)
COPY --from=builder /app/dist ./dist

# Copy specific file patterns
COPY *.py ./
COPY config/ ./config/
```

**.dockerignore** (controls what COPY ignores):

```
.git
node_modules
__pycache__
*.pyc
.env
```

| Key Point | Detail |
|-----------|--------|
| Respects `.dockerignore` | Always create one to exclude unnecessary files |
| `--from=stage` | Copy artifacts from another build stage |
| `--chown=user:group` | Set ownership without a separate `RUN chown` layer |
| Prefer COPY over ADD | COPY is explicit and predictable |

---

## ADD

Copy files into the image — like COPY, but with two extra capabilities: auto-extracting tar archives and fetching remote URLs.

```dockerfile
FROM ubuntu:24.04

# Auto-extract a tar archive into the image
ADD app.tar.gz /opt/app/

# Download a file from a URL (prefer curl in RUN for better caching)
ADD https://example.com/config.json /etc/app/config.json

# Same as COPY for regular files
ADD index.html /var/www/html/
```

| COPY vs ADD | |
|-------------|--|
| `COPY file .` | Copies the file as-is — always predictable |
| `ADD file.tar.gz .` | Automatically extracts the archive |
| `ADD https://... .` | Downloads from URL (but no cache control, prefer `RUN curl`) |

**Best practice:** Use `COPY` by default. Only use `ADD` when you specifically need tar extraction.

---

## RUN

Execute commands during the image build, creating a new layer.

```dockerfile
FROM ubuntu:24.04

# Shell form (runs via /bin/sh -c)
RUN apt-get update && apt-get install -y curl git vim

# Combine commands to reduce layers and clean up in the same layer
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        curl \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Exec form (no shell processing)
RUN ["pip", "install", "--no-cache-dir", "-r", "requirements.txt"]

# Multi-line script
RUN set -eux; \
    groupadd -r appuser; \
    useradd -r -g appuser -d /app -s /sbin/nologin appuser; \
    mkdir -p /app; \
    chown appuser:appuser /app
```

**With mount caches (BuildKit):**

```dockerfile
# Cache pip downloads across builds
RUN --mount=type=cache,target=/root/.cache/pip \
    pip install -r requirements.txt

# Cache apt packages across builds
RUN --mount=type=cache,target=/var/cache/apt \
    apt-get update && apt-get install -y curl
```

| Form | Behavior |
|------|----------|
| `RUN command` | Shell form — interpreted by `/bin/sh -c`, supports pipes and variables |
| `RUN ["executable", "arg"]` | Exec form — no shell, no variable expansion |
| `RUN --mount=type=cache` | BuildKit cache mount — speeds up repeated builds |

---

## CMD

Specify the default command to run when a container starts. Easily overridden by the user.

```dockerfile
FROM python:3.12-slim
WORKDIR /app
COPY . .

# Exec form (preferred — no shell wrapping, signals go directly to the process)
CMD ["python", "main.py"]

# Shell form (runs via /bin/sh -c)
CMD python main.py

# As default arguments to ENTRYPOINT
ENTRYPOINT ["python"]
CMD ["main.py"]
```

**Override at runtime:**

```bash
# Replaces CMD entirely
docker run my-app python other_script.py

# When used with ENTRYPOINT, replaces only the CMD arguments
docker run my-app other_script.py
```

| Key Point | Detail |
|-----------|--------|
| Only one CMD | If multiple CMD lines exist, only the last one takes effect |
| Overridable | `docker run <image> <command>` replaces CMD |
| Exec form preferred | Avoids shell signal-handling issues |

---

## ENTRYPOINT

Set the main executable for the container. Unlike CMD, it is NOT easily overridden.

```dockerfile
# Container always runs python — CMD provides default arguments
FROM python:3.12-slim
ENTRYPOINT ["python"]
CMD ["main.py"]
```

```dockerfile
# Container as a CLI tool
FROM alpine:3.20
RUN apk add --no-cache curl
ENTRYPOINT ["curl"]
CMD ["--help"]
```

```dockerfile
# Common pattern: entrypoint script for initialization
FROM postgres:16
COPY docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh
ENTRYPOINT ["docker-entrypoint.sh"]
CMD ["postgres"]
```

**Runtime behavior:**

```bash
# CMD overridden, ENTRYPOINT stays
docker run my-python-app test.py         # runs: python test.py

# Override ENTRYPOINT explicitly
docker run --entrypoint bash my-app      # runs: bash (ignores ENTRYPOINT and CMD)
```

| ENTRYPOINT vs CMD | |
|-------------------|--|
| `ENTRYPOINT` | The fixed executable — defines *what* the container is |
| `CMD` | Default arguments — defines *how* it runs by default |
| Combined | `ENTRYPOINT ["curl"]` + `CMD ["-s", "https://example.com"]` |

---

## EXPOSE

Document which ports the application listens on. This is metadata — it does NOT actually publish the port.

```dockerfile
FROM node:22
WORKDIR /app
COPY . .

# Single port
EXPOSE 3000

# Multiple ports
EXPOSE 8080 8443

# UDP port
EXPOSE 53/udp

CMD ["node", "server.js"]
```

**You still need `-p` to actually publish:**

```bash
docker run -p 3000:3000 my-app      # map explicitly
docker run -P my-app                 # auto-map all EXPOSE'd ports to random host ports
```

| Key Point | Detail |
|-----------|--------|
| Documentation only | Does not make the port accessible from the host |
| `-p` required | `docker run -p host:container` actually publishes |
| `-P` (uppercase) | Publishes all EXPOSE'd ports to random host ports |

---

## VOLUME

Create a mount point and mark it as holding externally-mounted or persistent data.

```dockerfile
FROM postgres:16

# Declare a volume — data written here is persisted even if container is removed
VOLUME /var/lib/postgresql/data

# Multiple volumes
VOLUME ["/data", "/logs"]
```

**What happens at runtime:**

```bash
# Docker creates an anonymous volume automatically
docker run -d postgres:16

# Better: use a named volume explicitly
docker run -d -v pgdata:/var/lib/postgresql/data postgres:16
```

| Key Point | Detail |
|-----------|--------|
| Anonymous volume | If no `-v` is specified, Docker creates an unnamed volume |
| Named volume preferred | `docker run -v name:/path` — easier to manage and back up |
| Cannot be undone in image | Once declared, writes to that path always go to a volume, not the image layer |

---

## USER

Set the user (and optionally group) for subsequent `RUN`, `CMD`, and `ENTRYPOINT` instructions.

```dockerfile
FROM python:3.12-slim

# Create a non-root user
RUN groupadd -r appuser && useradd -r -g appuser -d /app appuser

WORKDIR /app
COPY --chown=appuser:appuser . .
RUN pip install --no-cache-dir -r requirements.txt

# Switch to non-root user for runtime
USER appuser

CMD ["python", "main.py"]
```

```dockerfile
# Alpine variant
FROM node:22-alpine
RUN addgroup -S appgroup && adduser -S appuser -G appgroup
USER appuser
```

| Key Point | Detail |
|-----------|--------|
| Security best practice | Never run production containers as root |
| Create user first | Use `RUN useradd` or `RUN adduser` before switching |
| Affects subsequent instructions | All `RUN`, `CMD`, `ENTRYPOINT` after `USER` run as that user |

---

## LABEL

Add key-value metadata to the image. Used for organization, automation, and tooling.

```dockerfile
FROM python:3.12-slim

LABEL maintainer="dev@example.com"
LABEL version="1.2.0"
LABEL description="API service for user management"

# Multiple labels in one instruction (reduces layers)
LABEL org.opencontainers.image.title="my-api" \
      org.opencontainers.image.version="1.2.0" \
      org.opencontainers.image.source="https://github.com/org/repo" \
      org.opencontainers.image.created="2026-02-26"
```

**Query labels:**

```bash
docker inspect --format='{{json .Config.Labels}}' my-api | jq
```

| Key Point | Detail |
|-----------|--------|
| OCI standard keys | Use `org.opencontainers.image.*` for interoperability |
| No runtime effect | Labels are purely metadata |
| Useful for CI/CD | Filter images by label: `docker images --filter label=version=1.2.0` |

---

## MAINTAINER (Deprecated)

Specify the author of the image. **Deprecated in favor of LABEL.**

```dockerfile
# Old way (deprecated)
MAINTAINER dev@example.com

# Modern way (use this instead)
LABEL maintainer="dev@example.com"
```

---

## ONBUILD

Register a trigger instruction that executes when this image is used as a base for another build.

```dockerfile
# Base image: company-python-base
FROM python:3.12-slim
WORKDIR /app
ONBUILD COPY requirements.txt .
ONBUILD RUN pip install --no-cache-dir -r requirements.txt
ONBUILD COPY . .
```

```dockerfile
# Child image: my-app (triggers fire automatically)
FROM company-python-base
CMD ["python", "main.py"]
# At build time, the ONBUILD instructions run:
#   1. COPY requirements.txt .
#   2. RUN pip install -r requirements.txt
#   3. COPY . .
```

| Key Point | Detail |
|-----------|--------|
| Deferred execution | Runs in the child image's build, not the parent's |
| Use case | Shared base images that enforce a project structure |
| Caveat | Can be confusing — child builds fail if expected files are missing |

---

## SHELL

Change the default shell used by shell-form `RUN`, `CMD`, and `ENTRYPOINT` instructions.

```dockerfile
# Default on Linux is ["/bin/sh", "-c"]
# Default on Windows is ["cmd", "/S", "/C"]

# Switch to bash for better scripting support
FROM ubuntu:24.04
SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# Now RUN uses bash — pipefail catches errors in pipes
RUN curl -fsSL https://example.com/setup.sh | bash

# PowerShell on Windows
FROM mcr.microsoft.com/windows/servercore:ltsc2022
SHELL ["powershell", "-Command"]
RUN Get-ChildItem Env:
```

| Key Point | Detail |
|-----------|--------|
| `-o pipefail` | Bash option — fail the entire pipe if any command fails (prevents silent errors) |
| Affects shell form only | Exec form `RUN ["cmd", "arg"]` is unaffected |
| Per-stage | Each build stage can set a different SHELL |

---

## STOPSIGNAL

Specify the system call signal that will be sent to the container to request it to exit.

```dockerfile
FROM nginx:latest

# Default is SIGTERM. Override if your app needs a different signal.
STOPSIGNAL SIGQUIT
```

```dockerfile
FROM node:22
# Graceful shutdown — Node.js listens for SIGTERM by default, which is fine
STOPSIGNAL SIGTERM
```

```dockerfile
# Use signal number instead of name
STOPSIGNAL 9    # SIGKILL — immediate termination (not recommended)
```

| Signal | Behavior |
|--------|----------|
| `SIGTERM` (default) | Polite request to terminate — app can clean up |
| `SIGQUIT` | Used by Nginx for graceful shutdown (finish current requests) |
| `SIGKILL` | Forceful kill — no cleanup, use only as last resort |
| `SIGUSR1` | Some apps use this for log rotation or custom behavior |

---

## HEALTHCHECK

Define a command that Docker runs periodically to check if the container is still healthy.

```dockerfile
FROM python:3.12-slim
WORKDIR /app
COPY . .

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:8000/health || exit 1

CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
```

```dockerfile
# For containers without curl — use a Python script
HEALTHCHECK --interval=30s --timeout=5s --retries=3 \
    CMD python -c "import urllib.request; urllib.request.urlopen('http://localhost:8000/health')" || exit 1
```

```dockerfile
# Disable health check inherited from base image
HEALTHCHECK NONE
```

**Check status:**

```bash
docker inspect --format='{{json .State.Health}}' my-app | jq
docker ps    # HEALTH column shows: starting, healthy, or unhealthy
```

| Parameter | Default | Purpose |
|-----------|---------|---------|
| `--interval` | 30s | Time between checks |
| `--timeout` | 30s | Max time for a single check to complete |
| `--start-period` | 0s | Grace period for startup (failures don't count) |
| `--retries` | 3 | Number of consecutive failures before marking unhealthy |

---

## Instruction Execution Order — Typical Dockerfile

Putting it all together in a real-world Dockerfile:

```dockerfile
ARG PYTHON_VERSION=3.12

FROM python:${PYTHON_VERSION}-slim

LABEL maintainer="dev@example.com" \
      org.opencontainers.image.title="my-api" \
      org.opencontainers.image.version="2.0.0"

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    APP_HOME=/app

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN groupadd -r appuser && useradd -r -g appuser -d ${APP_HOME} appuser

WORKDIR ${APP_HOME}

COPY requirements.txt .
RUN --mount=type=cache,target=/root/.cache/pip \
    pip install -r requirements.txt

COPY --chown=appuser:appuser . .

VOLUME ["/app/uploads"]

EXPOSE 8000

HEALTHCHECK --interval=30s --timeout=5s --start-period=15s --retries=3 \
    CMD curl -f http://localhost:8000/health || exit 1

STOPSIGNAL SIGTERM

USER appuser

ENTRYPOINT ["uvicorn"]
CMD ["main:app", "--host", "0.0.0.0", "--port", "8000"]
```
