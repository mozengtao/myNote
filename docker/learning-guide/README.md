# Docker Learning Guide: From First Principles

A comprehensive guide to understanding Docker from the ground up, focusing on **concepts first, commands second**.

---

## Who This Guide Is For

- Developers with **zero Docker experience**
- People with **basic Linux and C/C++ systems knowledge**
- Anyone who wants to understand **why** Docker works, not just how to use it

---

## Design Principles

This guide follows strict pedagogical principles:

1. **Concepts before commands** - Understand the "why" before the "how"
2. **Design connected to constraint** - Every feature exists because of a real problem
3. **No marketing language** - Precise technical explanations only
4. **Visual learning** - ASCII diagrams and tables throughout
5. **Bilingual explanations** - English diagrams with Chinese explanations

---

## Guide Structure

```
+==================================================================+
||                                                                ||
||  PART 1: UNDERSTANDING (Sections 1-6)                          ||
||  +----------------------------------------------------------+  ||
||  |  The Problem, Containers vs VMs, Architecture,           |  ||
||  |  Linux Primitives, Images, Containers                    |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  PART 2: USING (Sections 7-11)                                 ||
||  +----------------------------------------------------------+  ||
||  |  Dockerfile, Networking, Volumes,                        |  ||
||  |  docker-compose, Real Projects                           |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  PART 3: MASTERY (Sections 12-15)                              ||
||  +----------------------------------------------------------+  ||
||  |  Common Mistakes, Docker vs Kubernetes,                  |  ||
||  |  Repositories to Study, Mental Model                     |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

---

## Table of Contents

### Part 1: Understanding Docker

| Section | Title | Description |
|---------|-------|-------------|
| [01](01-the-problem-docker-is-solving.md) | The Problem Docker Is Solving | Why containers exist, what Docker actually is |
| [02](02-containers-vs-vms.md) | Containers vs Virtual Machines | Technical comparison, why containers won |
| [03](03-docker-architecture.md) | Docker Architecture | CLI, daemon, containerd, runc, client-server model |
| [04](04-linux-primitives.md) | Linux Primitives Behind Docker | Namespaces, cgroups, union filesystems |
| [05](05-docker-images.md) | Docker Images | Layers, content-addressable storage, caching |
| [06](06-docker-containers.md) | Docker Containers | Lifecycle, PID 1 problem, signal handling |

### Part 2: Using Docker

| Section | Title | Description |
|---------|-------|-------------|
| [07](07-dockerfile.md) | Dockerfile (From Zero) | Mental model, common instructions, best practices |
| [08](08-docker-networking.md) | Docker Networking | Bridge networks, DNS, container communication |
| [09](09-volumes.md) | Volumes and Data Persistence | Why containers are ephemeral, volumes vs bind mounts |
| [10](10-docker-compose.md) | docker-compose | Multi-container systems, YAML structure |
| [11](11-real-projects.md) | Using Docker in Real Projects | Development workflow, production considerations |

### Part 3: Mastery

| Section | Title | Description |
|---------|-------|-------------|
| [12](12-common-mistakes.md) | Common Beginner Mistakes | Pitfalls to avoid |
| [13](13-docker-vs-kubernetes.md) | Docker vs Kubernetes | High-level comparison, when to use what |
| [14](14-repositories.md) | Open Source Repositories to Study | Learn from the source code |
| [15](15-mental-model.md) | Mental Model Summary | Complete Docker mental model |

---

## Recommended Reading Order

```
BEGINNER PATH:
+------------------------------------------------------------------+
|                                                                  |
|  Week 1: Foundation                                              |
|  01 -> 02 -> 03 -> 04                                            |
|  (Problem, VMs, Architecture, Linux Primitives)                  |
|                                                                  |
|  Week 2: Core Concepts                                           |
|  05 -> 06 -> 07                                                  |
|  (Images, Containers, Dockerfile)                                |
|                                                                  |
|  Week 3: Practical Usage                                         |
|  08 -> 09 -> 10 -> 11                                            |
|  (Networking, Volumes, Compose, Real Projects)                   |
|                                                                  |
|  Week 4: Mastery                                                 |
|  12 -> 13 -> 14 -> 15                                            |
|  (Mistakes, Kubernetes, Source Code, Mental Model)               |
|                                                                  |
+------------------------------------------------------------------+
```

---

## Learning Goals

By completing this guide, you will be able to:

1. **Explain** what Docker truly is (and is not)
2. **Describe** Docker's architecture and design decisions
3. **Understand** how containers work internally on Linux
4. **Write** correct Dockerfiles and docker-compose.yml files
5. **Use** Docker confidently in real projects
6. **Avoid** common misconceptions and mistakes
7. **Decide** when Docker is (and isn't) appropriate

---

## Quick Reference

### Essential Commands

```bash
# Build and run
docker build -t myapp:v1 .
docker run -d -p 8080:80 myapp:v1

# Inspect
docker ps
docker logs <container>
docker exec -it <container> /bin/sh

# Clean up
docker stop <container>
docker rm <container>
docker rmi <image>

# Compose
docker-compose up -d
docker-compose down
docker-compose logs -f
```

### Key Files

| File | Purpose |
|------|---------|
| `Dockerfile` | Build recipe for single image |
| `docker-compose.yml` | Multi-container orchestration |
| `.dockerignore` | Files to exclude from build context |

---

## Additional Resources

### Official Documentation
- [Docker Documentation](https://docs.docker.com/)
- [Dockerfile Reference](https://docs.docker.com/engine/reference/builder/)
- [Docker Compose Reference](https://docs.docker.com/compose/compose-file/)

### Source Code Repositories
- [moby/moby](https://github.com/moby/moby) - Docker Engine
- [containerd/containerd](https://github.com/containerd/containerd) - Container runtime
- [opencontainers/runc](https://github.com/opencontainers/runc) - OCI runtime
- [opencontainers/runtime-spec](https://github.com/opencontainers/runtime-spec) - OCI specification

---

## License

This guide is provided for educational purposes.
