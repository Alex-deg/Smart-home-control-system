from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, Body
from typing import Dict
import uuid
import json
import uvicorn

app = FastAPI()

active_connections: Dict[str, WebSocket] = {}
servers_db: Dict[str, int] = {}

@app.get("/")
async def get_home():
    return "Hello"

@app.get("/api/auth")
async def auth():
    return 1

@app.get("/api/registration")
async def registration():
    return 1

@app.get("/api/users/{user_id}/servers")
async def show_servers(user_id: int):
    # Возвращаем список серверов пользователя (из БД)
    user_servers = [{"server_id": sid, "user_id": uid} for sid, uid in servers_db.items() if uid == user_id]
    return user_servers

@app.post("/api/users/{user_id}/servers/add")
async def add_server(user_id: int):
    server_id = str(uuid.uuid4())
    servers_db[server_id] = user_id
    return {"server_id": server_id, "user_id": user_id}

@app.post("/api/users/{user_id}/servers/{server_id}/delete")
async def delete_server(user_id: int, server_id: str):
    if servers_db.get(server_id) != user_id:
        raise HTTPException(status_code=403, detail="Not owned")
    # Если есть активное соединение, закрываем его
    if server_id in active_connections:
        ws = active_connections[server_id]
        try:
            await ws.close(code=1000, reason="Server deleted")
        except:
            pass
        del active_connections[server_id]
    del servers_db[server_id]
    return True

@app.get("/api/users/{user_id}/servers/{server_id}/modules")
async def show_modules(user_id: int, server_id: str):
    # Заглушка
    return []

@app.get("/api/users/{user_id}/servers/{server_id}/modules/{module_id}/capabilities")
async def show_capabilities(user_id: int, server_id: str, module_id: str):
    return []

@app.websocket("/ws/{server_id}")
async def websocket_endpoint(websocket: WebSocket, server_id: str):
    if server_id not in servers_db:
        await websocket.close(code=1008, reason="Unknown server_id")
        return

    await websocket.accept()
    try:
        data = await websocket.receive_text()
        msg = json.loads(data)
        if msg.get("type") != "auth":
            await websocket.close(code=1008, reason="First message must be auth")
            return

        # Заменить старое соединение, если есть
        if server_id in active_connections:
            old = active_connections[server_id]
            try:
                await old.close(code=1000, reason="New connection")
            except:
                pass
            del active_connections[server_id]

        active_connections[server_id] = websocket
        await websocket.send_text(json.dumps({"status": "ok", "message": "Authenticated"}))

        while True:
            message = await websocket.receive_text()
            print(f"Message from {server_id}: {message}")
            # Эхо (или обработать результат команды)d
            await websocket.send_text(json.dumps({"echo": message}))
    except WebSocketDisconnect:
        pass
    finally:
        if server_id in active_connections:
            del active_connections[server_id]

@app.post("/api/users/{user_id}/servers/{server_id}/command")
async def send_command(user_id: int, server_id: str, command: dict = Body(...)):

    print(f"=== DEBUG ===")
    print(f"Request user_id: {user_id}")
    print(f"Request server_id: {server_id}")
    print(f"servers_db: {servers_db}")
    print(f"stored user_id for this server: {servers_db.get(server_id)}")
    print(f"active_connections: {list(active_connections.keys())}")

    if servers_db.get(server_id) != user_id:
        raise HTTPException(status_code=403, detail="Not owned")
    ws = active_connections.get(server_id)
    if not ws:
        raise HTTPException(status_code=404, detail="RPi not connected")
    try:
        await ws.send_text(json.dumps(command))
        return {"status": "sent", "command": command}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/active_connections")
async def list_connections():
    return list(active_connections.keys())

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)