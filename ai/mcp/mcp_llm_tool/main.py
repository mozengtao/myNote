"""
CLI entry point for the MCP Text Analysis Tool.

Supports two modes:
    1. server  — start the FastAPI HTTP server
    2. cli     — run a one-shot analysis from the command line
"""

import argparse
import json
import sys
import logging

import uvicorn

from config import load_config
from mcp_base import MCPRequest, MCPToolName, MCPValidationError, validate_request
from text_analyzer import TextAnalyzer

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s  %(levelname)-8s  %(name)s  %(message)s",
)
logger = logging.getLogger(__name__)


def run_server(args):
    """Launch the FastAPI server."""
    cfg = load_config()
    uvicorn.run(
        "api_server:app",
        host=args.host,
        port=args.port,
        reload=args.reload,
        log_level="info",
    )


def run_cli(args):
    """Execute a single text-analysis request and print the result."""
    cfg = load_config()
    analyzer = TextAnalyzer(cfg)

    mcp_req = MCPRequest(
        tool_name=MCPToolName.TEXT_ANALYSIS.value,
        parameters={
            "text": args.text,
            "export_csv": args.csv,
        },
    )

    try:
        validate_request(mcp_req)
    except MCPValidationError as exc:
        logger.error("Validation error: %s", exc)
        sys.exit(1)

    resp = analyzer.handle_request(mcp_req)
    print(json.dumps(resp.to_dict(), indent=2, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(
        description="MCP Text Analysis Tool",
    )
    sub = parser.add_subparsers(dest="command")

    # -- server sub-command ------------------------------------------------
    srv = sub.add_parser("server", help="Start the HTTP API server")
    srv.add_argument("--host", default="0.0.0.0")
    srv.add_argument("--port", type=int, default=8000)
    srv.add_argument("--reload", action="store_true")
    srv.set_defaults(func=run_server)

    # -- cli sub-command ---------------------------------------------------
    cli = sub.add_parser("cli", help="Run a one-shot analysis")
    cli.add_argument("text", help="Text to analyse")
    cli.add_argument("--csv", action="store_true", help="Export result to CSV")
    cli.set_defaults(func=run_cli)

    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        sys.exit(1)

    args.func(args)


if __name__ == "__main__":
    main()
