"""
MCP (Model Context Protocol) base layer.

Defines the request/response schemas and context management that
standardize communication between LLMs, tools, and applications.
All tool implementations receive MCPRequest and return MCPResponse,
ensuring a uniform contract regardless of the underlying LLM backend.
"""

import uuid
import time
import logging
from dataclasses import dataclass, field, asdict
from enum import Enum
from typing import Any, Dict, List, Optional

logger = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Enumerations
# ---------------------------------------------------------------------------

class MCPStatus(Enum):
    SUCCESS = "success"
    ERROR = "error"
    PARTIAL = "partial"


class MCPToolName(Enum):
    TEXT_ANALYSIS = "text_analysis"
    SUMMARIZATION = "summarization"
    ENTITY_EXTRACTION = "entity_extraction"


# ---------------------------------------------------------------------------
# Request / Response schemas
# ---------------------------------------------------------------------------

@dataclass
class MCPRequest:
    """Inbound request to an MCP tool."""

    tool_name: str
    parameters: Dict[str, Any]
    context: Dict[str, Any] = field(default_factory=dict)
    request_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    timestamp: float = field(default_factory=time.time)

    # Optional metadata the caller can attach (e.g. user_id, session_id).
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class MCPResponse:
    """Outbound response from an MCP tool."""

    request_id: str
    tool_name: str
    status: MCPStatus
    result: Dict[str, Any] = field(default_factory=dict)
    error: Optional[str] = None
    processing_time: float = 0.0
    metadata: Dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        d = asdict(self)
        d["status"] = self.status.value
        return d


# ---------------------------------------------------------------------------
# Parameter validation
# ---------------------------------------------------------------------------

# Maps tool names to their required/optional parameters.
_TOOL_SCHEMAS: Dict[str, Dict[str, Any]] = {
    MCPToolName.TEXT_ANALYSIS.value: {
        "required": ["text"],
        "optional": ["language", "export_csv"],
        "types": {
            "text": str,
            "language": str,
            "export_csv": bool,
        },
    },
}


class MCPValidationError(Exception):
    """Raised when an MCP request fails validation."""


def validate_request(request: MCPRequest) -> None:
    """
    Validate that *request* conforms to the schema for its tool.

    Raises MCPValidationError on any violation.
    """
    schema = _TOOL_SCHEMAS.get(request.tool_name)
    if schema is None:
        raise MCPValidationError(
            f"Unknown tool: '{request.tool_name}'. "
            f"Available: {list(_TOOL_SCHEMAS)}"
        )

    params = request.parameters

    for key in schema["required"]:
        if key not in params:
            raise MCPValidationError(
                f"Missing required parameter '{key}' for tool '{request.tool_name}'"
            )

    allowed = set(schema["required"]) | set(schema["optional"])
    unexpected = set(params) - allowed
    if unexpected:
        raise MCPValidationError(
            f"Unexpected parameters for tool '{request.tool_name}': {unexpected}"
        )

    for key, value in params.items():
        expected_type = schema["types"].get(key)
        if expected_type and not isinstance(value, expected_type):
            raise MCPValidationError(
                f"Parameter '{key}' must be {expected_type.__name__}, "
                f"got {type(value).__name__}"
            )


# ---------------------------------------------------------------------------
# Context manager — carries session-level metadata across requests
# ---------------------------------------------------------------------------

class MCPContextManager:
    """
    Lightweight session context that travels with a stream of MCP requests.

    Stores conversation history, accumulated tool results, and arbitrary
    key-value metadata so that multi-turn interactions remain coherent.
    """

    def __init__(self, session_id: Optional[str] = None):
        self.session_id: str = session_id or str(uuid.uuid4())
        self.history: List[Dict[str, Any]] = []
        self.metadata: Dict[str, Any] = {}
        self._created = time.time()

    def add_interaction(self, request: MCPRequest, response: MCPResponse) -> None:
        self.history.append({
            "request_id": request.request_id,
            "tool_name": request.tool_name,
            "parameters": request.parameters,
            "status": response.status.value,
            "result_summary": _summarise_result(response.result),
            "timestamp": time.time(),
        })

    def get_summary(self) -> Dict[str, Any]:
        return {
            "session_id": self.session_id,
            "interaction_count": len(self.history),
            "uptime_seconds": round(time.time() - self._created, 2),
            "metadata": self.metadata,
        }


def _summarise_result(result: Dict[str, Any], max_len: int = 200) -> str:
    text = str(result)
    return text[:max_len] + "..." if len(text) > max_len else text


# ---------------------------------------------------------------------------
# Convenience builder
# ---------------------------------------------------------------------------

def build_error_response(
    request: MCPRequest,
    error_message: str,
    processing_time: float = 0.0,
) -> MCPResponse:
    """Shortcut for returning a well-formed error response."""
    return MCPResponse(
        request_id=request.request_id,
        tool_name=request.tool_name,
        status=MCPStatus.ERROR,
        error=error_message,
        processing_time=processing_time,
    )
