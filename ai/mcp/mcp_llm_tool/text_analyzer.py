"""
Text Analysis tool — the business-logic layer.

Receives validated MCP requests, constructs LLM prompts for sentiment
analysis, parses the structured result, and optionally exports to CSV.
"""

import csv
import os
import time
import logging
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional

from config import AppConfig
from llm_client import LLMClient, LLMError
from mcp_base import (
    MCPRequest,
    MCPResponse,
    MCPStatus,
    build_error_response,
)

logger = logging.getLogger(__name__)

# The system prompt instructs the LLM to return machine-parseable JSON.
_SYSTEM_PROMPT = """\
You are a precise text-analysis engine.  Return ONLY valid JSON — no
explanatory text, no markdown fences.  Use the exact schema below:

{
  "sentiment": "<positive|negative|neutral|mixed>",
  "confidence": <float 0.0-1.0>,
  "keywords": ["<keyword1>", "<keyword2>", ...],
  "summary": "<one-sentence summary of the text>",
  "tone": "<formal|informal|technical|casual|urgent>",
  "language": "<detected language ISO-639-1 code>"
}
"""

_USER_PROMPT_TEMPLATE = "Analyse the following text:\n\n{text}"


@dataclass
class AnalysisResult:
    sentiment: str
    confidence: float
    keywords: List[str]
    summary: str
    tone: str
    language: str

    def to_dict(self) -> Dict[str, Any]:
        return {
            "sentiment": self.sentiment,
            "confidence": self.confidence,
            "keywords": self.keywords,
            "summary": self.summary,
            "tone": self.tone,
            "language": self.language,
        }


class TextAnalyzer:
    """Orchestrates LLM-based text analysis behind the MCP interface."""

    def __init__(self, config: AppConfig):
        self.config = config
        self.llm = LLMClient(config)

    def handle_request(self, request: MCPRequest) -> MCPResponse:
        """
        Entry point called by the API layer.

        Accepts a validated MCPRequest, runs the analysis, and wraps
        the result in an MCPResponse.
        """
        start = time.time()
        text = request.parameters["text"]
        export_csv = request.parameters.get("export_csv", False)

        # Guard against absurdly large inputs before calling the LLM.
        if len(text) > 50_000:
            return build_error_response(
                request,
                "Input text exceeds 50 000 characters. Please shorten it.",
                processing_time=time.time() - start,
            )

        try:
            result = self._analyse(text)
        except LLMError as exc:
            logger.exception("LLM call failed")
            return build_error_response(
                request,
                f"LLM processing error: {exc}",
                processing_time=time.time() - start,
            )
        except Exception as exc:
            logger.exception("Unexpected error during analysis")
            return build_error_response(
                request,
                f"Internal error: {exc}",
                processing_time=time.time() - start,
            )

        result_dict = result.to_dict()

        if export_csv:
            csv_path = self._export_csv(result)
            result_dict["csv_path"] = csv_path

        return MCPResponse(
            request_id=request.request_id,
            tool_name=request.tool_name,
            status=MCPStatus.SUCCESS,
            result=result_dict,
            processing_time=round(time.time() - start, 3),
        )

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _analyse(self, text: str) -> AnalysisResult:
        prompt = _USER_PROMPT_TEMPLATE.format(text=text)
        data = self.llm.generate_json(prompt, system_prompt=_SYSTEM_PROMPT)
        return self._parse_result(data)

    @staticmethod
    def _parse_result(data: Dict[str, Any]) -> AnalysisResult:
        """Coerce the raw LLM JSON into a validated AnalysisResult."""
        sentiment = str(data.get("sentiment", "unknown")).lower()
        if sentiment not in {"positive", "negative", "neutral", "mixed"}:
            sentiment = "unknown"

        confidence = data.get("confidence", 0.0)
        try:
            confidence = float(confidence)
            confidence = max(0.0, min(1.0, confidence))
        except (TypeError, ValueError):
            confidence = 0.0

        keywords = data.get("keywords", [])
        if not isinstance(keywords, list):
            keywords = []
        keywords = [str(k) for k in keywords]

        return AnalysisResult(
            sentiment=sentiment,
            confidence=round(confidence, 3),
            keywords=keywords,
            summary=str(data.get("summary", "")),
            tone=str(data.get("tone", "unknown")),
            language=str(data.get("language", "en")),
        )

    def _export_csv(self, result: AnalysisResult) -> str:
        """Write analysis result to a timestamped CSV file and return its path."""
        ts = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
        filename = f"analysis_{ts}.csv"
        path = os.path.join(self.config.csv_export_dir, filename)

        os.makedirs(self.config.csv_export_dir, exist_ok=True)

        with open(path, "w", newline="", encoding="utf-8") as fh:
            writer = csv.DictWriter(
                fh,
                fieldnames=["sentiment", "confidence", "keywords", "summary", "tone", "language"],
            )
            writer.writeheader()
            row = result.to_dict()
            row["keywords"] = "; ".join(row["keywords"])
            writer.writerow(row)

        logger.info("CSV exported to %s", path)
        return path
