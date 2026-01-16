# Section 10: docker-compose (Multi-Container Systems)

## 10.1 Why docker-compose Exists

```
THE MULTI-CONTAINER PROBLEM:
+==================================================================+
||                                                                ||
||  Real applications need multiple containers:                   ||
||                                                                ||
||  +----------------------------------------------------------+  ||
||  |  Web Application Stack:                                  |  ||
||  |                                                          |  ||
||  |  [Nginx] -> [App] -> [Redis] -> [PostgreSQL]             |  ||
||  |                                                          |  ||
||  |  Each needs its own container                            |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  WITHOUT docker-compose:                                       ||
||  +----------------------------------------------------------+  ||
||  |  docker network create myapp                             |  ||
||  |  docker volume create pgdata                             |  ||
||  |  docker run -d --name postgres --network myapp \         |  ||
||  |    -v pgdata:/var/lib/postgresql/data \                  |  ||
||  |    -e POSTGRES_PASSWORD=secret postgres:13               |  ||
||  |  docker run -d --name redis --network myapp redis:6      |  ||
||  |  docker run -d --name app --network myapp \              |  ||
||  |    -e DB_HOST=postgres myapp:latest                      |  ||
||  |  docker run -d --name nginx --network myapp \            |  ||
||  |    -p 80:80 nginx:latest                                 |  ||
||  |                                                          |  ||
||  |  4 commands, easy to make mistakes, hard to share        |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  WITH docker-compose:                                          ||
||  +----------------------------------------------------------+  ||
||  |  docker-compose up -d                                    |  ||
||  |                                                          |  ||
||  |  One command, declarative, version-controlled            |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

多容器问题：真实应用需要多个容器——Nginx -> App -> Redis -> PostgreSQL。

无docker-compose：需要多条docker命令创建网络、卷、运行各容器。4条命令，容易出错，难以分享。

有docker-compose：docker-compose up -d一条命令，声明式，可版本控制。

---

## 10.2 docker-compose.yml Structure

```yaml
# docker-compose.yml
version: "3.8"

services:
  # Each service = one container type
  web:
    image: nginx:latest
    ports:
      - "80:80"
    depends_on:
      - app
    networks:
      - frontend

  app:
    build: ./app
    environment:
      - DB_HOST=db
      - REDIS_HOST=redis
    depends_on:
      - db
      - redis
    networks:
      - frontend
      - backend

  db:
    image: postgres:13
    volumes:
      - pgdata:/var/lib/postgresql/data
    environment:
      - POSTGRES_PASSWORD=secret
    networks:
      - backend

  redis:
    image: redis:6-alpine
    networks:
      - backend

# Named volumes (persistent)
volumes:
  pgdata:

# Custom networks
networks:
  frontend:
  backend:
```

```
STRUCTURE BREAKDOWN:
+==================================================================+
||                                                                ||
||  version: "3.8"                                                ||
||  -> Compose file format version                                ||
||  -> Determines available features                              ||
||                                                                ||
||  services:                                                     ||
||  -> Each service becomes one or more containers                ||
||  -> Service name becomes DNS name on network                   ||
||                                                                ||
||  SERVICE CONFIGURATION:                                        ||
||  +----------------------------------------------------------+  ||
||  |  image: nginx:latest       Use existing image            |  ||
||  |  build: ./app              Build from Dockerfile         |  ||
||  |  ports: ["80:80"]          Port mapping                  |  ||
||  |  volumes: [...]            Data persistence              |  ||
||  |  environment: [...]        Environment variables         |  ||
||  |  depends_on: [...]         Startup order                 |  ||
||  |  networks: [...]           Network membership            |  ||
||  |  restart: always           Restart policy                |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  volumes:                                                      ||
||  -> Declare named volumes                                      ||
||  -> Managed by Docker                                          ||
||                                                                ||
||  networks:                                                     ||
||  -> Custom networks with DNS                                   ||
||  -> Services on same network can communicate by name           ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

结构分解：

version: "3.8"：Compose文件格式版本，决定可用功能。

services：每个服务成为一个或多个容器，服务名成为网络上的DNS名。

服务配置：image（使用现有镜像）、build（从Dockerfile构建）、ports（端口映射）、volumes（数据持久化）、environment（环境变量）、depends_on（启动顺序）、networks（网络成员）、restart（重启策略）。

volumes：声明命名卷，由Docker管理。

networks：带DNS的自定义网络，同网络服务可按名称通信。

---

## 10.3 Real Example: App + Database

```yaml
# docker-compose.yml for Flask + PostgreSQL
version: "3.8"

services:
  web:
    build: .
    ports:
      - "5000:5000"
    environment:
      - DATABASE_URL=postgresql://user:pass@db:5432/myapp
      - FLASK_ENV=development
    volumes:
      - .:/app  # Bind mount for development
    depends_on:
      db:
        condition: service_healthy
    networks:
      - appnet

  db:
    image: postgres:13-alpine
    environment:
      - POSTGRES_USER=user
      - POSTGRES_PASSWORD=pass
      - POSTGRES_DB=myapp
    volumes:
      - pgdata:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U user -d myapp"]
      interval: 5s
      timeout: 5s
      retries: 5
    networks:
      - appnet

volumes:
  pgdata:

networks:
  appnet:
```

```
HOW IT WORKS:
+==================================================================+
||                                                                ||
||  docker-compose up                                             ||
||       |                                                        ||
||       v                                                        ||
||  1. Create network "project_appnet"                            ||
||  2. Create volume "project_pgdata"                             ||
||  3. Start db container (postgres)                              ||
||  4. Wait for db healthcheck to pass                            ||
||  5. Start web container (flask app)                            ||
||       |                                                        ||
||       v                                                        ||
||  +----------------------------------------------------------+  ||
||  |  Network: project_appnet                                 |  ||
||  |                                                          |  ||
||  |  +--------+          +--------+                          |  ||
||  |  |  web   | -------> |   db   |                          |  ||
||  |  | :5000  |  DNS     | :5432  |                          |  ||
||  |  +--------+  "db"    +--------+                          |  ||
||  |      ^                   |                               |  ||
||  |      |                   v                               |  ||
||  |  localhost:5000     pgdata volume                        |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  Web connects to db using:                                     ||
||  postgresql://user:pass@db:5432/myapp                          ||
||                        ^                                       ||
||                        |                                       ||
||                   DNS resolves "db" to db container IP         ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

工作原理：docker-compose up执行：1）创建网络"project_appnet"，2）创建卷"project_pgdata"，3）启动db容器（postgres），4）等待db健康检查通过，5）启动web容器（flask app）。

Web通过postgresql://user:pass@db:5432/myapp连接到db。DNS将"db"解析为db容器IP。

---

## 10.4 Common Commands

```
DOCKER-COMPOSE COMMANDS:
+------------------------------------------------------------------+
|                                                                  |
|  # Start all services                                            |
|  docker-compose up                  # Foreground                 |
|  docker-compose up -d               # Detached (background)      |
|                                                                  |
|  # Stop and remove                                               |
|  docker-compose down                # Stop and remove containers |
|  docker-compose down -v             # Also remove volumes        |
|                                                                  |
|  # View status                                                   |
|  docker-compose ps                  # List containers            |
|  docker-compose logs                # View logs                  |
|  docker-compose logs -f web         # Follow specific service    |
|                                                                  |
|  # Rebuild                                                       |
|  docker-compose build               # Build images               |
|  docker-compose up --build          # Build and start            |
|                                                                  |
|  # Scale                                                         |
|  docker-compose up --scale web=3    # Run 3 web containers       |
|                                                                  |
|  # Execute commands                                              |
|  docker-compose exec web bash       # Shell into web container   |
|  docker-compose run web pytest      # Run one-off command        |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

docker-compose命令：

启动：docker-compose up（前台）、docker-compose up -d（后台）。

停止：docker-compose down（停止移除容器）、docker-compose down -v（也移除卷）。

状态：docker-compose ps（列容器）、docker-compose logs（查日志）。

重建：docker-compose build（构建镜像）、docker-compose up --build（构建并启动）。

扩展：docker-compose up --scale web=3（运行3个web容器）。

执行：docker-compose exec web bash（进入容器shell）、docker-compose run web pytest（运行一次性命令）。

---

## Summary

```
DOCKER-COMPOSE KEY INSIGHTS:
+==================================================================+
||                                                                ||
||  PURPOSE:                                                      ||
||  - Define multi-container applications                         ||
||  - Single command to start/stop entire stack                   ||
||  - Version-controlled infrastructure                           ||
||                                                                ||
||  STRUCTURE:                                                    ||
||  - services: Container definitions                             ||
||  - volumes: Persistent storage                                 ||
||  - networks: Container communication                           ||
||                                                                ||
||  KEY FEATURES:                                                 ||
||  - Service names become DNS names                              ||
||  - depends_on controls startup order                           ||
||  - healthcheck ensures readiness                               ||
||                                                                ||
||  USE CASES:                                                    ||
||  - Local development environments                              ||
||  - CI/CD testing                                               ||
||  - Simple production deployments                               ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Using Docker in Real Projects](11-real-projects.md)
