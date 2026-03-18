# MCP Text Analysis Tool

An **MCP-compliant, LLM-powered text analysis service** built with Python and FastAPI.

Demonstrates how the **Model Context Protocol (MCP)** standardizes communication
between LLMs, tools, and applications — from concept through production deployment.

## Quick Start

```bash
# 1. Install dependencies
pip install -r requirements.txt

# 2. Set your OpenAI key (or switch to local backend)
export OPENAI_API_KEY="sk-..."

# 3a. Start the HTTP server
python main.py server

# 3b. Or run a one-shot CLI analysis
python main.py cli "The product quality is amazing and delivery was fast."
```

## API Usage

```bash
curl -X POST http://localhost:8000/analyze \
  -H "Content-Type: application/json" \
  -d '{"text": "This service is fantastic.", "export_csv": true}'
```

## Configuration

| Variable | Default | Description |
|---|---|---|
| `OPENAI_API_KEY` | — | OpenAI API key |
| `LLM_BACKEND` | `openai` | `openai` or `local` |
| `LOCAL_LLM_URL` | `http://localhost:8080/v1` | Local model endpoint |
| `LOCAL_LLM_MODEL` | `llama3` | Model name for local server |
| `CSV_EXPORT_DIR` | `./exports` | Directory for CSV output |

## Documentation

See the full tutorial in `docs/`:

- **Part 1** — Conceptual Foundations & Architecture
- **Part 2** — Implementation Deep Dive
- **Part 3** — Deployment & Testing Guide
