# MCP LLM Tool — Part 2: Implementation Deep Dive

> **Series:** MCP LLM Tool Tutorial (3 parts)
> **Part 1** — Concepts, protocol design, and system architecture
> **Part 2** — Implementation deep dive (code walkthrough)
> **Part 3** — Deployment and testing guide

---

## Table of Contents

1. [Project Structure](#1-project-structure)
2. [Environment Setup](#2-environment-setup)
3. [Configuration Layer](#3-configuration-layer-configpy)
4. [MCP Base Protocol](#4-mcp-base-protocol-mcp_basepy)
5. [LLM Client Layer](#5-llm-client-layer-llm_clientpy)
6. [Tool Logic Layer](#6-tool-logic-layer-text_analyzerpy)
7. [API Layer](#7-api-layer-api_serverpy)
8. [CLI Entry Point](#8-cli-entry-point-mainpy)

---

## 1. Project Structure

```
mcp_llm_tool/
|
|-- config.py            # Runtime configuration (env vars, defaults)
|-- mcp_base.py          # MCP protocol: schemas, validation, context
|-- llm_client.py        # LLM abstraction (OpenAI + local backends)
|-- text_analyzer.py     # Business logic: sentiment analysis + CSV export
|-- api_server.py        # FastAPI HTTP layer
|-- main.py              # CLI entry point (server / one-shot modes)
|
|-- requirements.txt     # Python dependencies
|-- README.md            # Quick-start guide
|-- exports/             # (created at runtime) CSV output directory
```

Each file maps to exactly one architectural layer from Part 1.

---

## 2. Environment Setup

### 2.1 Python Version

Python **3.8+** is required. Python 3.10+ is recommended for the best
`dataclass` and type-hint ergonomics.

### 2.2 Dependencies

| Package | Version | Purpose |
|---|---|---|
| `fastapi` | >= 0.110 | HTTP API framework with automatic OpenAPI docs |
| `uvicorn[standard]` | >= 0.29 | ASGI server to run FastAPI |
| `openai` | >= 1.30 | Official OpenAI Python SDK (chat completions) |
| `requests` | >= 2.31 | HTTP client for local LLM endpoint |
| `pydantic` | >= 2.7 | Data validation (used by FastAPI internally) |

### 2.3 Installation

```bash
# Clone or create the project directory
cd mcp_llm_tool

# (Optional) Create a virtual environment
python -m venv .venv
source .venv/bin/activate

# Install dependencies
pip install -r requirements.txt
```

### 2.4 Environment Variables

Set the variables your chosen backend requires:

```bash
# --- OpenAI backend (default) ---
export OPENAI_API_KEY="sk-..."

# --- Local backend ---
export LLM_BACKEND="local"
export LOCAL_LLM_URL="http://localhost:8080/v1"
export LOCAL_LLM_MODEL="llama3"

# --- Optional ---
export CSV_EXPORT_DIR="./exports"
```

---

## 3. Configuration Layer (`config.py`)

The configuration module reads environment variables **once** at import time
and exposes typed dataclasses consumed by every other module.

### 3.1 Design Decisions

- **Dataclasses over dicts** — typos in config keys are caught at attribute
  access time, and IDE autocomplete works.
- **Enum for backend selection** — impossible to misspell "openai" vs. "local"
  once it reaches application code.
- **`__post_init__` hooks** — each sub-config reads its own env vars, keeping
  the logic co-located with the data it populates.

### 3.2 Key Classes

```python
class LLMBackend(Enum):
    OPENAI = "openai"
    LOCAL = "local"

@dataclass
class AppConfig:
    llm_backend: LLMBackend
    openai: OpenAIConfig
    local_llm: LocalLLMConfig
    server: ServerConfig
    request_timeout: float = 30.0
    max_retries: int = 3
    csv_export_dir: str = "./exports"
```

`load_config()` is the single public entry point:

```python
from config import load_config
config = load_config()
```

---

## 4. MCP Base Protocol (`mcp_base.py`)

This is the **heart of the MCP design**. Every tool in the system speaks
through these types.

### 4.1 Request and Response Schemas

```python
@dataclass
class MCPRequest:
    tool_name: str                          # e.g. "text_analysis"
    parameters: Dict[str, Any]              # tool-specific inputs
    context: Dict[str, Any] = field(...)    # session context
    request_id: str = field(...)            # UUID for tracing
    timestamp: float = field(...)           # creation time
    metadata: Dict[str, Any] = field(...)   # caller-attached data
```

```python
@dataclass
class MCPResponse:
    request_id: str
    tool_name: str
    status: MCPStatus          # SUCCESS | ERROR | PARTIAL
    result: Dict[str, Any]
    error: Optional[str]
    processing_time: float
    metadata: Dict[str, Any]
```

The `to_dict()` method on `MCPResponse` converts the enum to its string
value, making the object directly JSON-serializable.

### 4.2 Validation

Tool schemas are registered in a module-level dict:

```python
_TOOL_SCHEMAS = {
    "text_analysis": {
        "required": ["text"],
        "optional": ["language", "export_csv"],
        "types": {"text": str, "language": str, "export_csv": bool},
    },
}
```

`validate_request()` enforces three rules:

1. **Tool exists** — rejects unknown tool names.
2. **Required params present** — rejects requests missing mandatory fields.
3. **Type correctness** — rejects params whose runtime type doesn't match the schema.

Unexpected extra parameters are also rejected to prevent silent bugs.

### 4.3 Context Management

```python
class MCPContextManager:
    session_id: str
    history: List[Dict]       # past request/response summaries
    metadata: Dict[str, Any]  # arbitrary session-level state
```

After each tool call, the API layer can call
`context.add_interaction(request, response)` to append a summary to the
history. This enables multi-turn reasoning: a future tool call can inspect
`context.history` to see what has already been analysed.

### 4.4 Error Helper

```python
def build_error_response(request, error_message, processing_time=0.0):
    ...
```

Centralizes error-response construction so that every tool returns errors
in the same shape, with the same fields populated.

---

## 5. LLM Client Layer (`llm_client.py`)

### 5.1 Architecture

```
+-------------------+
|    LLMClient      |
|                   |
|  generate()       |------+----> OpenAI SDK
|  generate_json()  |      |
|                   |      +----> requests.post (local)
+-------------------+
```

The caller never needs to know which backend is active. They call:

```python
client = LLMClient(config)
text = client.generate("Analyse this text...")
data = client.generate_json("Return JSON for...")
```

### 5.2 OpenAI Backend

Uses the official `openai` Python SDK:

```python
resp = self._openai.chat.completions.create(
    model=cfg.model,
    messages=messages,
    temperature=temperature or cfg.temperature,
    max_tokens=max_tokens or cfg.max_tokens,
    timeout=self.config.request_timeout,
)
return resp.choices[0].message.content.strip()
```

Handles three exception types:
- `RateLimitError` — back-off and retry
- `APITimeoutError` — back-off and retry
- `APIError` — back-off and retry (covers 5xx, auth errors, etc.)

### 5.3 Local Backend

Sends a raw HTTP POST to an OpenAI-compatible `/v1/chat/completions`
endpoint. This works with:

- **llama.cpp** server (`--api-oai`)
- **vLLM** (`--served-model-name`)
- **Ollama** (with the OpenAI compatibility shim)
- **LM Studio** (built-in OpenAI endpoint)

```python
payload = {
    "model": cfg.model,
    "messages": messages,
    "temperature": ...,
    "max_tokens": ...,
}
resp = requests.post(url, json=payload, timeout=...)
```

### 5.4 Retry Strategy

Both backends share the same retry loop:

```
Attempt 1  -->  fail  -->  sleep 1.5s
Attempt 2  -->  fail  -->  sleep 2.25s
Attempt 3  -->  fail  -->  raise LLMError
```

The back-off base (`1.5s`) is exponential: `wait = 1.5 ^ attempt`.

### 5.5 JSON Parsing

LLMs sometimes wrap JSON in markdown fences. `_parse_json()` strips
those before calling `json.loads()`:

```python
@staticmethod
def _parse_json(raw: str) -> Dict[str, Any]:
    cleaned = raw.strip()
    if cleaned.startswith("```"):
        lines = cleaned.splitlines()
        lines = [l for l in lines if not l.strip().startswith("```")]
        cleaned = "\n".join(lines)
    return json.loads(cleaned)
```

If parsing fails, the raw text is included in the `LLMError` for debugging.

---

## 6. Tool Logic Layer (`text_analyzer.py`)

### 6.1 Prompt Design

Two prompts work together:

**System prompt** — instructs the LLM to behave as a text-analysis engine
and return *only* valid JSON in a fixed schema:

```json
{
  "sentiment": "<positive|negative|neutral|mixed>",
  "confidence": 0.0-1.0,
  "keywords": ["..."],
  "summary": "one sentence",
  "tone": "<formal|informal|technical|casual|urgent>",
  "language": "ISO-639-1 code"
}
```

**User prompt** — inserts the text to analyse:

```
Analyse the following text:

{text}
```

This separation (system vs. user) leverages the chat-completion API's
role-based messaging to give the model clear, persistent instructions.

### 6.2 Result Parsing

The raw JSON from the LLM is coerced into a typed `AnalysisResult` dataclass:

```python
@dataclass
class AnalysisResult:
    sentiment: str       # positive | negative | neutral | mixed | unknown
    confidence: float    # clamped to [0.0, 1.0]
    keywords: List[str]
    summary: str
    tone: str
    language: str
```

Defensive parsing handles common LLM quirks:
- Sentiment values outside the expected set default to `"unknown"`.
- Confidence is clamped to `[0.0, 1.0]`.
- Non-list keyword values are replaced with an empty list.

### 6.3 CSV Export

When `export_csv=True`, the result is written to a timestamped CSV:

```
exports/analysis_20260314_153042.csv
```

The CSV has one row per analysis with columns matching the `AnalysisResult`
fields. Keywords are joined with `"; "` for readability in spreadsheets.

### 6.4 Error Boundaries

The `handle_request()` method wraps the entire flow in a try/except:

```python
def handle_request(self, request: MCPRequest) -> MCPResponse:
    start = time.time()
    ...
    try:
        result = self._analyse(text)
    except LLMError as exc:
        return build_error_response(request, f"LLM processing error: {exc}", ...)
    except Exception as exc:
        return build_error_response(request, f"Internal error: {exc}", ...)
    ...
```

This ensures the caller **always** gets a well-formed `MCPResponse`, even
when the LLM is unreachable or returns garbage.

---

## 7. API Layer (`api_server.py`)

### 7.1 FastAPI Application

```python
app = FastAPI(
    title="MCP Text Analysis Tool",
    version="1.0.0",
)
```

CORS is wide-open for development. In production, restrict `allow_origins`.

### 7.2 Endpoints

| Method | Path | Purpose |
|---|---|---|
| `POST` | `/analyze` | Run text analysis |
| `GET` | `/health` | Readiness probe |
| `GET` | `/tools` | List registered MCP tools and their schemas |

### 7.3 Request Flow (POST /analyze)

```python
@app.post("/analyze")
async def analyze(body: AnalyzeRequestBody):
    # 1. Build MCPRequest from Pydantic model
    mcp_req = MCPRequest(tool_name="text_analysis", parameters={...})

    # 2. Validate against MCP schema
    validate_request(mcp_req)

    # 3. Delegate to tool
    mcp_resp = analyzer.handle_request(mcp_req)

    # 4. Return structured response
    return AnalyzeResponseBody(**mcp_resp.to_dict())
```

HTTP status codes:

| Code | Meaning |
|---|---|
| 200 | Analysis succeeded |
| 422 | Validation failure (bad params, missing fields) |
| 502 | LLM backend error (timeout, rate limit, bad response) |

### 7.4 Pydantic Models

FastAPI uses Pydantic for automatic request parsing and response serialization:

```python
class AnalyzeRequestBody(BaseModel):
    text: str = Field(..., min_length=1, max_length=50000)
    language: Optional[str] = None
    export_csv: bool = False
    metadata: Dict[str, Any] = {}
```

This gives us:
- Automatic input validation with descriptive error messages
- OpenAPI schema generation at `/docs`
- Type-safe access in the handler

### 7.5 Interactive Documentation

FastAPI auto-generates interactive API docs:

- **Swagger UI:** `http://localhost:8000/docs`
- **ReDoc:** `http://localhost:8000/redoc`

No additional configuration is needed.

---

## 8. CLI Entry Point (`main.py`)

### 8.1 Two Modes

```bash
# Start the HTTP server
python main.py server --host 0.0.0.0 --port 8000 --reload

# Run a one-shot analysis
python main.py cli "The product quality is amazing." --csv
```

### 8.2 Server Mode

Delegates to `uvicorn.run()`:

```python
def run_server(args):
    uvicorn.run(
        "api_server:app",
        host=args.host,
        port=args.port,
        reload=args.reload,
    )
```

### 8.3 CLI Mode

Builds an `MCPRequest`, validates it, runs the tool, and prints JSON:

```python
def run_cli(args):
    cfg = load_config()
    analyzer = TextAnalyzer(cfg)

    mcp_req = MCPRequest(
        tool_name="text_analysis",
        parameters={"text": args.text, "export_csv": args.csv},
    )
    validate_request(mcp_req)

    resp = analyzer.handle_request(mcp_req)
    print(json.dumps(resp.to_dict(), indent=2))
```

This is useful for quick testing without starting a server.

---

## Summary

This part walked through every file in the project:

| File | Layer | Key Responsibility |
|---|---|---|
| `config.py` | Configuration | Env var loading, typed defaults |
| `mcp_base.py` | Protocol | Request/response schemas, validation, context |
| `llm_client.py` | LLM Client | Backend abstraction, retries, JSON parsing |
| `text_analyzer.py` | Tool Logic | Prompt construction, result parsing, CSV export |
| `api_server.py` | API Transport | HTTP endpoints, Pydantic serialization |
| `main.py` | Entry Point | Server and CLI modes |

The code is intentionally **layered and decoupled**:
- Swap the LLM backend by changing one env var.
- Add a new tool by registering a schema in `mcp_base.py` and writing a new
  handler module — no changes to the API or client layers.
- Replace FastAPI with Flask, gRPC, or a CLI-only interface — the tool logic
  and MCP protocol remain untouched.

**Next:** [Part 3 — Deployment & Testing Guide](mcp_llm_tool_part3_deployment_testing.md)
