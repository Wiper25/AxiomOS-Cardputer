#!/usr/bin/env python3
"""Minimal OpenAI-compatible + WebSocket AI backend for AxiomOS Cardputer.

Run:
  pip install fastapi uvicorn
  uvicorn server:app --host 0.0.0.0 --port 8080

Endpoints used by firmware:
  POST /v1/chat/completions   (stream + non-stream)
  WS   /v1/chat/ws
"""

from __future__ import annotations

import asyncio
import json
from typing import Any, AsyncIterator, Dict, List, Optional

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import StreamingResponse
from pydantic import BaseModel, Field

app = FastAPI(title="AxiomOS AI Server", version="1.0.0")


class ChatMessage(BaseModel):
    role: str
    content: str


class ChatRequest(BaseModel):
    model: str = "axiom-local"
    stream: bool = False
    messages: List[ChatMessage] = Field(default_factory=list)


def local_reply(user_text: str) -> str:
    t = (user_text or "").lower()
    if "wifi" in t or "wifi" in t:
        return "Проверь RSSI и пароль. При RSSI < -75 подойди ближе к AP."
    if "nrf" in t or "радио" in t:
        return "Смотри спектр: каналы с высокой активностью лучше обойти."
    if "reboot" in t or "перезагруз" in t:
        return "Частая причина reboot — brownout. Проверь 3.3V и ток PA+LNA."
    return (
        f"Axiom server: получил «{user_text[:120]}». "
        "Подключи реальный LLM здесь (OpenAI/Ollama) — клиент Cardputer уже готов."
    )


async def sse_chunks(text: str) -> AsyncIterator[bytes]:
    # Simulate token streaming
    step = 12
    for i in range(0, len(text), step):
        piece = text[i : i + step]
        payload = {
            "choices": [{"delta": {"content": piece}, "index": 0}],
        }
        yield f"data: {json.dumps(payload, ensure_ascii=False)}\n\n".encode("utf-8")
        await asyncio.sleep(0.03)
    yield b"data: [DONE]\n\n"


@app.get("/health")
def health() -> Dict[str, Any]:
    return {"ok": True, "service": "axiom-ai"}


@app.post("/v1/chat/completions")
async def chat_completions(req: ChatRequest):
    user = ""
    for m in reversed(req.messages):
        if m.role == "user":
            user = m.content
            break
    answer = local_reply(user)

    if req.stream:
        return StreamingResponse(sse_chunks(answer), media_type="text/event-stream")

    return {
        "id": "chatcmpl-axiom",
        "object": "chat.completion",
        "model": req.model,
        "choices": [
            {
                "index": 0,
                "message": {"role": "assistant", "content": answer},
                "finish_reason": "stop",
            }
        ],
    }


@app.websocket("/v1/chat/ws")
async def chat_ws(ws: WebSocket) -> None:
    await ws.accept()
    try:
        while True:
            raw = await ws.receive_text()
            try:
                data = json.loads(raw)
            except json.JSONDecodeError:
                data = {"content": raw}
            content = str(data.get("content") or data.get("message") or "")
            answer = local_reply(content)
            # stream as JSON deltas
            step = 16
            for i in range(0, len(answer), step):
                await ws.send_text(
                    json.dumps({"content": answer[i : i + step]}, ensure_ascii=False)
                )
                await asyncio.sleep(0.03)
            await ws.send_text("[DONE]")
    except WebSocketDisconnect:
        return


if __name__ == "__main__":
    import uvicorn

    uvicorn.run(app, host="0.0.0.0", port=8080)
