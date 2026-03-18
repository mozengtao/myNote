"""
FastAPI REST layer — exposes the MCP tool over HTTP.

Endpoints:
    POST /analyze    — run text analysis
    GET  /health     — readiness probe
    GET  /tools      — list registered MCP tools and their schemas
"""

import logging
import time
from typing import Any, Dict, Optional

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field

from config import load_config
from mcp_base import (
    MCPRequest,
    MCPStatus,
    MCPToolName,
    MCPValidationError,
    validate_request,
)
from text_analyzer import TextAnalyzer

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s  %(levelname)-8s  %(name)s  %(message)s",
)
logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# App bootstrap
# ---------------------------------------------------------------------------
config = load_config()
analyzer = TextAnalyzer(config)

app = FastAPI(
    title="MCP Text Analysis Tool",
    description=(
        "An MCP-compliant LLM-powered text analysis service. "
        "Supports sentiment analysis with structured JSON output and CSV export."
    ),
    version="1.0.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)


# ---------------------------------------------------------------------------
# Pydantic models for request/response serialization
# ---------------------------------------------------------------------------

class AnalyzeRequestBody(BaseModel):
    text: str = Field(..., min_length=1, max_length=50000, description="Text to analyse")
    language: Optional[str] = Field(None, description="ISO-639-1 hint (e.g. 'en')")
    export_csv: bool = Field(False, description="Write result to CSV file")
    metadata: Dict[str, Any] = Field(default_factory=dict)


class AnalyzeResponseBody(BaseModel):
    request_id: str
    tool_name: str
    status: str
    result: Dict[str, Any] = {}
    error: Optional[str] = None
    processing_time: float = 0.0
    metadata: Dict[str, Any] = {}


# ---------------------------------------------------------------------------
# Endpoints
# ---------------------------------------------------------------------------

@app.post("/analyze", response_model=AnalyzeResponseBody)
async def analyze(body: AnalyzeRequestBody):
    """Run MCP text analysis on the supplied text."""
    mcp_req = MCPRequest(
        tool_name=MCPToolName.TEXT_ANALYSIS.value,
        parameters={
            "text": body.text,
            "export_csv": body.export_csv,
            **({"language": body.language} if body.language else {}),
        },
        metadata=body.metadata,
    )

    try:
        validate_request(mcp_req)
    except MCPValidationError as exc:
        raise HTTPException(status_code=422, detail=str(exc))

    mcp_resp = analyzer.handle_request(mcp_req)

    if mcp_resp.status == MCPStatus.ERROR:
        raise HTTPException(status_code=502, detail=mcp_resp.error)

    return AnalyzeResponseBody(**mcp_resp.to_dict())


@app.get("/health")
async def health():
    """Readiness probe."""
    return {
        "status": "healthy",
        "backend": config.llm_backend.value,
        "timestamp": time.time(),
    }


@app.get("/tools")
async def list_tools():
    """Return the catalogue of registered MCP tools."""
    return {
        "tools": [
            {
                "name": MCPToolName.TEXT_ANALYSIS.value,
                "description": "LLM-powered sentiment and tone analysis",
                "parameters": {
                    "text": {"type": "string", "required": True},
                    "language": {"type": "string", "required": False},
                    "export_csv": {"type": "boolean", "required": False},
                },
            }
        ]
    }
