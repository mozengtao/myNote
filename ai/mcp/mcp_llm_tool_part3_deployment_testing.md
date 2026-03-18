# MCP LLM Tool — Part 3: Deployment & Testing Guide

> **Series:** MCP LLM Tool Tutorial (3 parts)
> **Part 1** — Concepts, protocol design, and system architecture
> **Part 2** — Implementation deep dive (code walkthrough)
> **Part 3** — Deployment and testing guide

---

## Table of Contents

1. [Local Deployment](#1-local-deployment)
2. [Cloud Deployment](#2-cloud-deployment)
3. [Functional Testing](#3-functional-testing)
4. [API Testing](#4-api-testing)
5. [Failure Testing](#5-failure-testing)
6. [Automated Test Suite](#6-automated-test-suite)

---

## 1. Local Deployment

### 1.1 Prerequisites

- Python 3.8+ installed
- An OpenAI API key **or** a running local LLM server
- Network access to `api.openai.com` (if using OpenAI backend)

### 1.2 Step-by-Step

```bash
# 1. Navigate to the project
cd mcp_llm_tool

# 2. Create and activate a virtual environment
python -m venv .venv
source .venv/bin/activate   # Linux / macOS
# .venv\Scripts\activate    # Windows

# 3. Install dependencies
pip install -r requirements.txt

# 4. Configure the backend
#    Option A: OpenAI
export OPENAI_API_KEY="sk-..."

#    Option B: Local LLM
export LLM_BACKEND="local"
export LOCAL_LLM_URL="http://localhost:8080/v1"
export LOCAL_LLM_MODEL="llama3"

# 5. Start the server
python main.py server
```

Expected output:

```
INFO:     Started server process [12345]
INFO:     Waiting for application startup.
INFO:     Application startup complete.
INFO:     Uvicorn running on http://0.0.0.0:8000 (Press CTRL+C to quit)
```

### 1.3 Verify the Server

```bash
# Health check
curl http://localhost:8000/health

# Expected response:
# {"status":"healthy","backend":"openai","timestamp":1710432000.0}
```

### 1.4 Using a Local LLM

If you want to avoid OpenAI costs, run a local model. Common options:

**Ollama (easiest):**

```bash
# Install Ollama (https://ollama.ai)
ollama pull llama3
ollama serve
# Exposes OpenAI-compatible endpoint at http://localhost:11434/v1
```

Then configure:

```bash
export LLM_BACKEND="local"
export LOCAL_LLM_URL="http://localhost:11434/v1"
export LOCAL_LLM_MODEL="llama3"
```

**llama.cpp server:**

```bash
./server -m models/llama3-8b.gguf --host 0.0.0.0 --port 8080
```

```bash
export LLM_BACKEND="local"
export LOCAL_LLM_URL="http://localhost:8080/v1"
export LOCAL_LLM_MODEL="llama3"
```

**LM Studio:**

Enable the OpenAI-compatible server in LM Studio settings (default port 1234).

```bash
export LLM_BACKEND="local"
export LOCAL_LLM_URL="http://localhost:1234/v1"
```

---

## 2. Cloud Deployment

### 2.1 Server Setup

This guide uses a standard Linux server (Ubuntu 22.04+). The same steps
apply to any VPS provider (AWS EC2, DigitalOcean, Hetzner, etc.).

```bash
# Update system packages
sudo apt update && sudo apt upgrade -y

# Install Python 3 and pip
sudo apt install -y python3 python3-pip python3-venv

# Verify
python3 --version   # Should be 3.8+
```

### 2.2 Deploy the Application

```bash
# Create application directory
sudo mkdir -p /opt/mcp-tool
sudo chown $USER:$USER /opt/mcp-tool

# Copy project files (from your local machine)
scp -r mcp_llm_tool/* user@SERVER_IP:/opt/mcp-tool/

# On the server:
cd /opt/mcp-tool
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# Set environment variables
export OPENAI_API_KEY="sk-..."
```

### 2.3 Manual Start

```bash
cd /opt/mcp-tool
source .venv/bin/activate
uvicorn api_server:app --host 0.0.0.0 --port 8000
```

### 2.4 Process Management with Supervisor

For production, use **Supervisor** to auto-restart the process.

**Install Supervisor:**

```bash
sudo apt install -y supervisor
```

**Create configuration:**

```ini
# /etc/supervisor/conf.d/mcp-tool.conf

[program:mcp-tool]
directory=/opt/mcp-tool
command=/opt/mcp-tool/.venv/bin/uvicorn api_server:app --host 0.0.0.0 --port 8000
user=www-data
autostart=true
autorestart=true
stderr_logfile=/var/log/mcp-tool/err.log
stdout_logfile=/var/log/mcp-tool/out.log
environment=OPENAI_API_KEY="sk-...",LLM_BACKEND="openai",CSV_EXPORT_DIR="/opt/mcp-tool/exports"
```

**Activate:**

```bash
sudo mkdir -p /var/log/mcp-tool
sudo supervisorctl reread
sudo supervisorctl update
sudo supervisorctl start mcp-tool
```

**Management commands:**

```bash
sudo supervisorctl status mcp-tool      # Check status
sudo supervisorctl restart mcp-tool     # Restart
sudo supervisorctl tail mcp-tool        # View recent logs
```

### 2.5 Firewall Configuration

```bash
# Allow HTTP traffic on port 8000
sudo ufw allow 8000/tcp
sudo ufw enable
sudo ufw status
```

### 2.6 Accessing the Service

From any machine:

```bash
curl http://SERVER_IP:8000/health
curl http://SERVER_IP:8000/docs       # Swagger UI in a browser
```

### 2.7 (Optional) Reverse Proxy with Nginx

For HTTPS and a proper domain name:

```nginx
# /etc/nginx/sites-available/mcp-tool

server {
    listen 80;
    server_name mcp.example.com;

    location / {
        proxy_pass http://127.0.0.1:8000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

```bash
sudo ln -s /etc/nginx/sites-available/mcp-tool /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx

# Add HTTPS with Let's Encrypt
sudo apt install certbot python3-certbot-nginx
sudo certbot --nginx -d mcp.example.com
```

---

## 3. Functional Testing

### 3.1 Positive Sentiment

**Input:**

```
"The product quality is amazing and delivery was fast."
```

**Expected result:**

```json
{
  "sentiment": "positive",
  "confidence": 0.92,
  "keywords": ["amazing", "quality", "fast", "delivery"],
  "summary": "The user is highly satisfied with both product quality and delivery speed.",
  "tone": "informal",
  "language": "en"
}
```

**Verification checklist:**
- [ ] `sentiment` is `"positive"`
- [ ] `confidence` is above 0.8
- [ ] `keywords` contains relevant terms
- [ ] `summary` is a coherent single sentence
- [ ] `tone` is reasonable for the input

### 3.2 Negative Sentiment

**Input:**

```
"The service was terrible. I waited 3 hours and nobody helped me."
```

**Expected result:**

```json
{
  "sentiment": "negative",
  "confidence": 0.95,
  "keywords": ["terrible", "waited", "hours", "nobody", "helped"],
  "summary": "The user is frustrated with poor service and long wait times.",
  "tone": "informal",
  "language": "en"
}
```

### 3.3 Mixed Sentiment

**Input:**

```
"The food was delicious but the restaurant was too noisy and the staff was rude."
```

**Expected:**

- `sentiment`: `"mixed"`
- Keywords should span both positive and negative aspects

### 3.4 CSV Export Verification

```bash
# Run with CSV export enabled
python main.py cli "Great product, fast shipping!" --csv

# Check the exports directory
ls exports/
# analysis_20260314_153042.csv

# Inspect the CSV
cat exports/analysis_*.csv
# sentiment,confidence,keywords,summary,tone,language
# positive,0.93,Great; product; fast; shipping,...
```

---

## 4. API Testing

### 4.1 curl

**Basic request:**

```bash
curl -X POST http://localhost:8000/analyze \
  -H "Content-Type: application/json" \
  -d '{
    "text": "This service is fantastic and the support team was incredibly helpful."
  }'
```

**With CSV export:**

```bash
curl -X POST http://localhost:8000/analyze \
  -H "Content-Type: application/json" \
  -d '{
    "text": "This service is fantastic.",
    "export_csv": true
  }'
```

**With metadata:**

```bash
curl -X POST http://localhost:8000/analyze \
  -H "Content-Type: application/json" \
  -d '{
    "text": "Excellent work on the new feature release.",
    "metadata": {"source": "customer_survey", "user_id": "u-123"}
  }'
```

**Health check:**

```bash
curl http://localhost:8000/health
```

**List tools:**

```bash
curl http://localhost:8000/tools
```

### 4.2 Python requests

```python
import requests

resp = requests.post(
    "http://localhost:8000/analyze",
    json={
        "text": "The new update broke everything. Very disappointed.",
        "export_csv": True,
    },
)

data = resp.json()
print(f"Sentiment: {data['result']['sentiment']}")
print(f"Confidence: {data['result']['confidence']}")
print(f"Keywords: {data['result']['keywords']}")
```

### 4.3 Postman

1. **Create a new request.**
2. Set method to `POST` and URL to `http://localhost:8000/analyze`.
3. Go to the **Body** tab → select **raw** → set type to **JSON**.
4. Paste:

```json
{
  "text": "This is the best conference I have ever attended.",
  "export_csv": false
}
```

5. Click **Send**.
6. Verify the response body contains `status: "success"` and a populated
   `result` object.

### 4.4 Swagger UI (Built-in)

Navigate to `http://localhost:8000/docs` in a browser. The interactive
Swagger UI lets you:

- Browse all endpoints
- Fill in request parameters
- Execute requests directly
- Inspect response bodies and status codes

---

## 5. Failure Testing

### 5.1 Invalid API Key

```bash
export OPENAI_API_KEY="sk-invalid-key-12345"
python main.py server
```

Then send a request:

```bash
curl -X POST http://localhost:8000/analyze \
  -H "Content-Type: application/json" \
  -d '{"text": "Test"}'
```

**Expected:** HTTP 502 with error message indicating authentication failure.

### 5.2 Network Timeout (Local Backend Unreachable)

```bash
export LLM_BACKEND="local"
export LOCAL_LLM_URL="http://localhost:9999/v1"   # nothing listening here
python main.py server
```

```bash
curl -X POST http://localhost:8000/analyze \
  -H "Content-Type: application/json" \
  -d '{"text": "Test"}'
```

**Expected:** After retries with back-off, HTTP 502 with connection error.

Server logs should show:

```
WARNING  Cannot reach local LLM at http://localhost:9999/v1/chat/completions — retrying in 1.5s (attempt 1)
WARNING  Cannot reach local LLM at http://localhost:9999/v1/chat/completions — retrying in 2.2s (attempt 2)
ERROR    Cannot reach local LLM ... — no retries left
```

### 5.3 Extremely Long Input

```bash
# Generate a 60,000-character input (exceeds the 50,000 limit)
python -c "print('word ' * 12000)" | \
  xargs -I{} curl -X POST http://localhost:8000/analyze \
    -H "Content-Type: application/json" \
    -d "{\"text\": \"{}\"}"
```

Or more reliably:

```python
import requests

long_text = "word " * 12000  # 60,000 chars
resp = requests.post(
    "http://localhost:8000/analyze",
    json={"text": long_text},
)
print(resp.status_code)  # 422 — Pydantic rejects text > 50,000 chars
print(resp.json())
```

**Expected:** HTTP 422 with validation error about `max_length`.

### 5.4 Malformed Request

**Missing required field:**

```bash
curl -X POST http://localhost:8000/analyze \
  -H "Content-Type: application/json" \
  -d '{}'
```

**Expected:** HTTP 422 — `text` field is required.

**Wrong type:**

```bash
curl -X POST http://localhost:8000/analyze \
  -H "Content-Type: application/json" \
  -d '{"text": 12345}'
```

**Expected:** HTTP 422 — `text` must be a string.

**Invalid JSON:**

```bash
curl -X POST http://localhost:8000/analyze \
  -H "Content-Type: application/json" \
  -d 'not json at all'
```

**Expected:** HTTP 422 — JSON decode error.

### 5.5 Empty Text

```bash
curl -X POST http://localhost:8000/analyze \
  -H "Content-Type: application/json" \
  -d '{"text": ""}'
```

**Expected:** HTTP 422 — `text` must have `min_length` of 1.

---

## 6. Automated Test Suite

For repeatable testing, create a test file.

### 6.1 test_mcp_tool.py

```python
"""
Automated tests for the MCP Text Analysis Tool.

Run with:
    pytest test_mcp_tool.py -v
"""

import pytest
from fastapi.testclient import TestClient

from api_server import app


client = TestClient(app)


class TestHealthEndpoint:
    def test_health_returns_200(self):
        resp = client.get("/health")
        assert resp.status_code == 200
        data = resp.json()
        assert data["status"] == "healthy"
        assert "backend" in data
        assert "timestamp" in data


class TestToolsEndpoint:
    def test_tools_lists_text_analysis(self):
        resp = client.get("/tools")
        assert resp.status_code == 200
        tools = resp.json()["tools"]
        names = [t["name"] for t in tools]
        assert "text_analysis" in names


class TestAnalyzeEndpoint:
    def test_missing_text_returns_422(self):
        resp = client.post("/analyze", json={})
        assert resp.status_code == 422

    def test_empty_text_returns_422(self):
        resp = client.post("/analyze", json={"text": ""})
        assert resp.status_code == 422

    def test_wrong_type_returns_422(self):
        resp = client.post("/analyze", json={"text": 12345})
        assert resp.status_code == 422

    def test_valid_request_returns_200(self):
        """Requires a working LLM backend."""
        resp = client.post(
            "/analyze",
            json={"text": "The product is wonderful and I love it."},
        )
        # This will be 200 if the LLM is reachable, 502 otherwise.
        assert resp.status_code in (200, 502)

        if resp.status_code == 200:
            data = resp.json()
            assert data["status"] == "success"
            assert "sentiment" in data["result"]
            assert "confidence" in data["result"]
            assert "keywords" in data["result"]


class TestMCPBase:
    """Unit tests for the MCP protocol layer (no LLM needed)."""

    def test_validate_request_rejects_unknown_tool(self):
        from mcp_base import MCPRequest, MCPValidationError, validate_request

        req = MCPRequest(tool_name="nonexistent", parameters={})
        with pytest.raises(MCPValidationError, match="Unknown tool"):
            validate_request(req)

    def test_validate_request_rejects_missing_param(self):
        from mcp_base import MCPRequest, MCPValidationError, validate_request

        req = MCPRequest(tool_name="text_analysis", parameters={})
        with pytest.raises(MCPValidationError, match="Missing required"):
            validate_request(req)

    def test_validate_request_rejects_wrong_type(self):
        from mcp_base import MCPRequest, MCPValidationError, validate_request

        req = MCPRequest(
            tool_name="text_analysis",
            parameters={"text": 123},
        )
        with pytest.raises(MCPValidationError, match="must be str"):
            validate_request(req)

    def test_validate_request_accepts_valid(self):
        from mcp_base import MCPRequest, validate_request

        req = MCPRequest(
            tool_name="text_analysis",
            parameters={"text": "Hello world"},
        )
        validate_request(req)  # Should not raise


class TestContextManager:
    def test_session_tracks_interactions(self):
        from mcp_base import MCPContextManager, MCPRequest, MCPResponse, MCPStatus

        ctx = MCPContextManager()
        req = MCPRequest(tool_name="text_analysis", parameters={"text": "hi"})
        resp = MCPResponse(
            request_id=req.request_id,
            tool_name=req.tool_name,
            status=MCPStatus.SUCCESS,
            result={"sentiment": "neutral"},
        )

        ctx.add_interaction(req, resp)
        summary = ctx.get_summary()
        assert summary["interaction_count"] == 1
```

### 6.2 Running Tests

```bash
# Install pytest
pip install pytest

# Run all tests
pytest test_mcp_tool.py -v

# Run only unit tests (no LLM required)
pytest test_mcp_tool.py -v -k "MCPBase or ContextManager or Health or Tools"
```

---

## Summary

| Topic | Key Takeaway |
|---|---|
| **Local deployment** | One command: `python main.py server` |
| **Cloud deployment** | Supervisor for process management, Nginx for HTTPS |
| **Functional testing** | Verify sentiment, confidence, keywords, and CSV export |
| **API testing** | curl, Python requests, Postman, or built-in Swagger UI |
| **Failure testing** | Invalid keys, timeouts, long input, malformed requests |
| **Automated tests** | pytest with `TestClient` — MCP validation needs no LLM |

### Complete Request/Response Example

**Request:**

```bash
curl -s -X POST http://localhost:8000/analyze \
  -H "Content-Type: application/json" \
  -d '{"text": "The product quality is amazing and delivery was fast."}' \
  | python -m json.tool
```

**Response:**

```json
{
    "request_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
    "tool_name": "text_analysis",
    "status": "success",
    "result": {
        "sentiment": "positive",
        "confidence": 0.93,
        "keywords": ["amazing", "quality", "fast", "delivery"],
        "summary": "The user expresses high satisfaction with product quality and delivery speed.",
        "tone": "informal",
        "language": "en"
    },
    "error": null,
    "processing_time": 1.234,
    "metadata": {}
}
```

---

**End of tutorial.** You now have a fully functional, MCP-compliant,
LLM-powered text analysis tool — from concept through deployment.
