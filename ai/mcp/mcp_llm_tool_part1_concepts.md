# MCP LLM Tool — Part 1: Conceptual Foundations & Architecture

> **Series:** MCP LLM Tool Tutorial (3 parts)
> **Part 1** — Concepts, protocol design, and system architecture
> **Part 2** — Implementation deep dive (code walkthrough)
> **Part 3** — Deployment and testing guide

---

## Table of Contents

1. [What is MCP?](#1-what-is-mcp-model-context-protocol)
2. [Why MCP Exists](#2-why-mcp-exists)
3. [Core Concepts](#3-core-concepts)
4. [System Architecture](#4-system-architecture)
5. [Request Lifecycle](#5-request-lifecycle)
6. [Layer Responsibilities](#6-layer-responsibilities)

---

## 1. What is MCP (Model Context Protocol)?

**MCP — Model Context Protocol** — is a communication standard that governs
how Large Language Models (LLMs) interact with external tools, data sources,
and application layers.

Think of it as the "HTTP of LLM-tool integration": just as HTTP defines how
browsers and servers exchange data, MCP defines how an LLM invokes a tool,
what parameters it sends, and what structured result it receives back.

### Definition

MCP is a **protocol specification** that provides:

- A **request schema** describing tool invocations (tool name, parameters, context).
- A **response schema** describing structured results (status, data, errors).
- A **context management** model that carries session state across multi-turn
  interactions.
- A **validation contract** ensuring tools only receive well-typed, expected inputs.

### What MCP is NOT

| MCP is NOT | Explanation |
|---|---|
| An LLM model | It orchestrates model calls — it does not run inference itself. |
| A prompt template | Prompts are internal to the tool — MCP wraps the request/response. |
| An API framework | It is a protocol layer *on top of* any transport (HTTP, stdio, gRPC). |

---

## 2. Why MCP Exists

Before MCP, every LLM-tool integration was **bespoke**. Each application
invented its own way to:

- describe available tools to the LLM
- validate tool-call parameters
- format tool results back into the conversation

This led to several recurring problems:

### 2.1 The Problem Space

```
+------------------------------------------------------------+
| Without MCP                                                |
+------------------------------------------------------------+
|                                                            |
|  App A  --(custom JSON)-->  Tool 1                         |
|  App A  --(XML blob)------>  Tool 2                        |
|  App B  --(ad-hoc dict)--->  Tool 1  (different contract!) |
|  App B  --(raw string)---->  Tool 3                        |
|                                                            |
|  Result: N apps x M tools = N*M integration contracts      |
+------------------------------------------------------------+
```

**Pain points:**

1. **Fragmentation** — every tool has a different calling convention.
2. **Brittle validation** — parameter types are checked inconsistently (or not at all).
3. **No context continuity** — multi-turn interactions lose state between calls.
4. **Poor error semantics** — tools return ad-hoc error strings with no structure.
5. **Vendor lock-in** — switching from OpenAI to a local model means rewriting
   the integration layer.

### 2.2 What MCP Solves

```
+------------------------------------------------------------+
| With MCP                                                   |
+------------------------------------------------------------+
|                                                            |
|  App A  --[MCPRequest]-->  MCP Layer  -->  Tool 1          |
|  App A  --[MCPRequest]-->  MCP Layer  -->  Tool 2          |
|  App B  --[MCPRequest]-->  MCP Layer  -->  Tool 1          |
|  App B  --[MCPRequest]-->  MCP Layer  -->  Tool 3          |
|                                                            |
|  Result: 1 universal contract, tools are plug-and-play     |
+------------------------------------------------------------+
```

| Benefit | How MCP delivers it |
|---|---|
| **Standardized invocation** | Every tool receives the same `MCPRequest` shape. |
| **Structured context** | `MCPContextManager` carries session history and metadata. |
| **Type-safe validation** | Schemas enforce required params, types, and allowed values. |
| **Uniform error model** | `MCPResponse` always includes status + optional error string. |
| **Backend interoperability** | The LLM client sits *behind* the MCP layer — swap models freely. |

---

## 3. Core Concepts

### 3.1 MCPRequest

The unit of work entering the system. Contains:

| Field | Type | Purpose |
|---|---|---|
| `tool_name` | string | Which tool to invoke (`text_analysis`, etc.) |
| `parameters` | dict | Tool-specific inputs (`text`, `language`, ...) |
| `context` | dict | Session-level context (conversation history, user prefs) |
| `request_id` | string (UUID) | Correlation ID for tracing |
| `timestamp` | float | Unix epoch of request creation |
| `metadata` | dict | Arbitrary caller-attached data |

### 3.2 MCPResponse

The result envelope leaving the system.

| Field | Type | Purpose |
|---|---|---|
| `request_id` | string | Echoes the inbound request's ID |
| `tool_name` | string | Confirms which tool produced the result |
| `status` | enum | `success`, `error`, or `partial` |
| `result` | dict | The tool's structured output |
| `error` | string or null | Human-readable error message (if status is `error`) |
| `processing_time` | float | Wall-clock seconds |
| `metadata` | dict | Optional response metadata |

### 3.3 MCPContextManager

Carries session state across a series of requests:

- **history** — ordered list of past request/response summaries
- **metadata** — arbitrary key-value store (user preferences, session config)
- **session_id** — stable identifier for the conversation

This lets downstream tools make decisions based on *what has already happened*
in the conversation, without the caller needing to resend everything.

### 3.4 Validation Contract

Each registered tool declares a schema:

```python
{
    "required": ["text"],
    "optional": ["language", "export_csv"],
    "types": {
        "text": str,
        "language": str,
        "export_csv": bool,
    },
}
```

Before any LLM call is made, the MCP layer validates the request against
this schema. Invalid requests are rejected immediately with a descriptive
`MCPValidationError`, avoiding wasted LLM tokens.

---

## 4. System Architecture

### 4.1 High-Level Diagram

```
+-------------------------------------------------------------------+
|                       MCP Text Analysis Tool                      |
+-------------------------------------------------------------------+
|                                                                   |
|   +------------------+                                            |
|   |   Client / User  |  (curl, Postman, another LLM agent)        |
|   +--------+---------+                                            |
|            |                                                      |
|            | HTTP POST /analyze                                   |
|            v                                                      |
|   +------------------+                                            |
|   |  API Layer       |  api_server.py (FastAPI)                   |
|   |  - deserialize   |                                            |
|   |  - build MCPReq  |                                            |
|   +--------+---------+                                            |
|            |                                                      |
|            v                                                      |
|   +------------------+                                            |
|   |  MCP Protocol    |  mcp_base.py                               |
|   |  - validate      |                                            |
|   |  - context mgmt  |                                            |
|   +--------+---------+                                            |
|            |                                                      |
|            v                                                      |
|   +------------------+                                            |
|   |  Tool Logic      |  text_analyzer.py                          |
|   |  - prompt build  |                                            |
|   |  - result parse  |                                            |
|   |  - CSV export    |                                            |
|   +--------+---------+                                            |
|            |                                                      |
|            v                                                      |
|   +------------------+     +----------------------------------+   |
|   |  LLM Client      |---->  OpenAI API  or  Local LLM Server |   |
|   |  - retries       |     +----------------------------------+   |
|   |  - timeout       |                                            |
|   |  - JSON parse    |                                            |
|   +------------------+                                            |
|            |                                                      |
|            v                                                      |
|   +------------------+                                            |
|   |  MCP Response    |  Structured JSON envelope                  |
|   |  Formatter       |                                            |
|   +--------+---------+                                            |
|            |                                                      |
|            v                                                      |
|   +------------------+                                            |
|   |  Client / User   |  <-- receives JSON + optional CSV path     |
|   +------------------+                                            |
|                                                                   |
+-------------------------------------------------------------------+
```

### 4.2 Module Dependency Graph

```
main.py
  |
  +---> api_server.py
  |       |
  |       +---> mcp_base.py      (validation, schemas)
  |       +---> text_analyzer.py  (tool logic)
  |               |
  |               +---> llm_client.py  (LLM calls)
  |               +---> mcp_base.py    (response building)
  |
  +---> config.py  (shared by all modules)
```

---

## 5. Request Lifecycle

A complete request flows through five stages:

```
Stage 1          Stage 2           Stage 3          Stage 4          Stage 5
+-----------+    +-----------+    +-----------+    +-----------+    +-----------+
| Receive   |--->| Validate  |--->| Process   |--->| Format    |--->| Return    |
| HTTP req  |    | MCP req   |    | LLM call  |    | MCP resp  |    | HTTP resp |
+-----------+    +-----------+    +-----------+    +-----------+    +-----------+
```

### Stage-by-stage breakdown

| Stage | Module | What Happens |
|---|---|---|
| **1. Receive** | `api_server.py` | FastAPI deserializes the JSON body into a Pydantic model, then constructs an `MCPRequest`. |
| **2. Validate** | `mcp_base.py` | `validate_request()` checks the tool name exists, required params are present, types match. Rejects bad requests early. |
| **3. Process** | `text_analyzer.py` + `llm_client.py` | The tool builds a prompt, calls the LLM (OpenAI or local), parses the JSON response into an `AnalysisResult`. Optionally writes CSV. |
| **4. Format** | `mcp_base.py` | The result (or error) is wrapped in an `MCPResponse` envelope with status, timing, and metadata. |
| **5. Return** | `api_server.py` | FastAPI serializes the `MCPResponse` dict into the HTTP response body with appropriate status code. |

### Error paths

Errors can occur at any stage. The system handles each distinctly:

| Error Origin | Handling |
|---|---|
| Malformed HTTP body | FastAPI returns 422 with Pydantic validation details |
| MCP validation failure | `MCPValidationError` → 422 with descriptive message |
| LLM timeout / rate limit | `LLMClient` retries with back-off; if exhausted, returns 502 |
| LLM returns invalid JSON | `LLMError` → 502 with raw response excerpt |
| Unexpected exception | Caught at tool level → 502 with generic error |

---

## 6. Layer Responsibilities

### 6.1 API Layer (`api_server.py`)

**Single responsibility:** HTTP transport.

- Accepts HTTP requests and returns HTTP responses.
- Performs *transport-level* validation (content type, body shape).
- Knows nothing about LLMs, prompts, or sentiment analysis.
- Could be swapped for a CLI, gRPC, or WebSocket transport with zero changes
  to the layers below.

### 6.2 MCP Protocol Layer (`mcp_base.py`)

**Single responsibility:** contract enforcement.

- Defines the `MCPRequest` / `MCPResponse` data shapes.
- Validates tool parameters against registered schemas.
- Manages session context (`MCPContextManager`).
- Is transport-agnostic and LLM-agnostic.

### 6.3 Tool Logic Layer (`text_analyzer.py`)

**Single responsibility:** business functionality.

- Knows *what* to analyse and *how* to interpret the result.
- Constructs domain-specific prompts.
- Parses and validates LLM output into typed data classes.
- Handles export side-effects (CSV).
- Does not know which LLM backend is in use — delegates via `LLMClient`.

### 6.4 LLM Client Layer (`llm_client.py`)

**Single responsibility:** model communication.

- Abstracts the difference between OpenAI and local models.
- Handles retries, timeouts, and structured JSON parsing.
- Returns plain strings or parsed dicts — no domain knowledge.

### 6.5 Configuration Layer (`config.py`)

**Single responsibility:** runtime settings.

- Reads environment variables once at startup.
- Provides typed, defaulted dataclasses to every other module.
- No business logic, no I/O beyond `os.getenv` and `os.makedirs`.

---

## Summary

This part established the **conceptual framework**:

1. **MCP** is a protocol that standardizes LLM-tool communication with typed
   requests, responses, validation, and context management.
2. The system is **layered**: transport → protocol → tool logic → LLM client,
   each with a single responsibility.
3. The **request lifecycle** flows through five stages with well-defined error
   handling at every boundary.
4. The architecture is **backend-agnostic** — switching from OpenAI to a local
   model changes one environment variable, not the tool code.

**Next:** [Part 2 — Implementation Deep Dive](mcp_llm_tool_part2_implementation.md)
