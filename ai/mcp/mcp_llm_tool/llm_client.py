"""
LLM client abstraction layer.

Provides a single interface for calling both OpenAI-hosted models and
local inference servers that expose an OpenAI-compatible /v1/chat/completions
endpoint (e.g., llama.cpp server, vLLM, Ollama with OpenAI shim).

Features:
    - Automatic retries with exponential back-off
    - Configurable timeouts
    - Structured JSON response parsing
    - Transparent backend switching via config
"""

import json
import time
import logging
from typing import Any, Dict, List, Optional

import requests
from openai import OpenAI, APIError, APITimeoutError, RateLimitError

from config import AppConfig, LLMBackend

logger = logging.getLogger(__name__)

_RETRY_BACKOFF_BASE = 1.5  # seconds


class LLMError(Exception):
    """Raised when the LLM call fails after all retries."""


class LLMClient:
    """
    Unified LLM gateway.

    Usage:
        client = LLMClient(config)
        response = client.generate("Analyse the sentiment of: ...")
    """

    def __init__(self, config: AppConfig):
        self.config = config
        self.backend = config.llm_backend

        if self.backend == LLMBackend.OPENAI:
            if not config.openai.api_key:
                raise LLMError(
                    "OPENAI_API_KEY is not set. Export it or switch to 'local' backend."
                )
            self._openai = OpenAI(api_key=config.openai.api_key)
        else:
            self._openai = None

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def generate(
        self,
        prompt: str,
        system_prompt: Optional[str] = None,
        temperature: Optional[float] = None,
        max_tokens: Optional[int] = None,
    ) -> str:
        """Send a chat-completion request to the configured backend."""
        if self.backend == LLMBackend.OPENAI:
            return self._call_openai(prompt, system_prompt, temperature, max_tokens)
        return self._call_local(prompt, system_prompt, temperature, max_tokens)

    def generate_json(
        self,
        prompt: str,
        system_prompt: Optional[str] = None,
        temperature: Optional[float] = None,
        max_tokens: Optional[int] = None,
    ) -> Dict[str, Any]:
        """Like generate(), but parses the response as JSON."""
        raw = self.generate(prompt, system_prompt, temperature, max_tokens)
        return self._parse_json(raw)

    # ------------------------------------------------------------------
    # OpenAI backend
    # ------------------------------------------------------------------

    def _call_openai(
        self,
        prompt: str,
        system_prompt: Optional[str],
        temperature: Optional[float],
        max_tokens: Optional[int],
    ) -> str:
        cfg = self.config.openai
        messages = self._build_messages(prompt, system_prompt)

        for attempt in range(1, self.config.max_retries + 1):
            try:
                resp = self._openai.chat.completions.create(
                    model=cfg.model,
                    messages=messages,
                    temperature=temperature or cfg.temperature,
                    max_tokens=max_tokens or cfg.max_tokens,
                    timeout=self.config.request_timeout,
                )
                return resp.choices[0].message.content.strip()
            except RateLimitError:
                self._backoff(attempt, "Rate-limited by OpenAI")
            except APITimeoutError:
                self._backoff(attempt, "OpenAI request timed out")
            except APIError as exc:
                self._backoff(attempt, f"OpenAI API error: {exc}")

        raise LLMError("All retries exhausted for OpenAI backend")

    # ------------------------------------------------------------------
    # Local model backend (OpenAI-compatible HTTP endpoint)
    # ------------------------------------------------------------------

    def _call_local(
        self,
        prompt: str,
        system_prompt: Optional[str],
        temperature: Optional[float],
        max_tokens: Optional[int],
    ) -> str:
        cfg = self.config.local_llm
        url = f"{cfg.base_url.rstrip('/')}/chat/completions"
        messages = self._build_messages(prompt, system_prompt)

        payload = {
            "model": cfg.model,
            "messages": messages,
            "temperature": temperature or cfg.temperature,
            "max_tokens": max_tokens or cfg.max_tokens,
        }

        for attempt in range(1, self.config.max_retries + 1):
            try:
                resp = requests.post(
                    url,
                    json=payload,
                    timeout=self.config.request_timeout,
                )
                resp.raise_for_status()
                data = resp.json()
                return data["choices"][0]["message"]["content"].strip()
            except requests.exceptions.Timeout:
                self._backoff(attempt, "Local LLM request timed out")
            except requests.exceptions.ConnectionError:
                self._backoff(attempt, f"Cannot reach local LLM at {url}")
            except (KeyError, IndexError) as exc:
                raise LLMError(f"Unexpected response structure: {exc}") from exc
            except requests.exceptions.HTTPError as exc:
                self._backoff(attempt, f"Local LLM HTTP error: {exc}")

        raise LLMError("All retries exhausted for local LLM backend")

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    @staticmethod
    def _build_messages(
        prompt: str,
        system_prompt: Optional[str],
    ) -> List[Dict[str, str]]:
        msgs: List[Dict[str, str]] = []
        if system_prompt:
            msgs.append({"role": "system", "content": system_prompt})
        msgs.append({"role": "user", "content": prompt})
        return msgs

    @staticmethod
    def _parse_json(raw: str) -> Dict[str, Any]:
        """
        Best-effort JSON extraction.

        LLMs sometimes wrap JSON in ```json ... ``` fences; strip those first.
        """
        cleaned = raw.strip()
        if cleaned.startswith("```"):
            lines = cleaned.splitlines()
            lines = [l for l in lines if not l.strip().startswith("```")]
            cleaned = "\n".join(lines)
        try:
            return json.loads(cleaned)
        except json.JSONDecodeError as exc:
            raise LLMError(f"LLM returned non-JSON response: {exc}\nRaw: {raw}")

    def _backoff(self, attempt: int, reason: str) -> None:
        if attempt >= self.config.max_retries:
            logger.error("%s — no retries left", reason)
            return
        wait = _RETRY_BACKOFF_BASE ** attempt
        logger.warning("%s — retrying in %.1fs (attempt %d)", reason, wait, attempt)
        time.sleep(wait)
