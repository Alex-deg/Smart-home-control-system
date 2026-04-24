from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, Body
from typing import Dict
import uuid
import json
import uvicorn
import database_worker

app = FastAPI()
db = database_worker.Database()

active_connections: Dict[str, WebSocket] = {}
temp_servers: Dict[str, Dict] = {}

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
    return db.get_servers(user_id)

@app.post("/api/users/{user_id}/servers/add")
async def add_server(user_id: int, name : str = Body(...)):
    server_token = str(uuid.uuid4())
    temp_servers[server_token] = {"user_id" : user_id, "server_name" : name}
    return {"server_token": server_token, "user_id": user_id}

@app.post("/api/users/{user_id}/servers/{server_id}/delete")
async def delete_server(user_id : int, server_id: int):
    owner_id = db.get_server_owner_id(server_id)
    if owner_id != user_id:
        raise HTTPException(status_code=403, detail="Not owned")
    # Если есть активное соединение, закрываем его
    server_info = db.get_server_info(server_id)
    if server_info["token"] in active_connections:
        ws = active_connections[server_info["token"]]
        try:
            await ws.close(code=1000, reason="Server deleted")
        except:
            pass
        del active_connections[server_info["token"]]
    # Удаление из таблиц servers и users_servers
    db.delete_server(server_id)
    return True

@app.get("/api/users/{user_id}/servers/{server_id}/modules")
async def show_modules(user_id: int, server_id: str):
    # Заглушка
    return []

@app.get("/api/users/{user_id}/servers/{server_id}/modules/{module_id}/capabilities")
async def show_capabilities(user_id: int, server_id: str, module_id: str):
    return []

@app.websocket("/ws/bind_server")
async def websocket_endpoint(websocket: WebSocket, server_token: str = Body(...)):
    if server_token not in temp_servers:
        await websocket.close(code=1008, reason="Unknown server_id")
        return

    db.add_server(temp_servers[server_token]["user_id"], temp_servers[server_token]["name"], server_token)
    del temp_servers[server_token]

    await websocket.accept()
    try:
        data = await websocket.receive_text()
        msg = json.loads(data)
        if msg.get("type") != "auth":
            await websocket.close(code=1008, reason="First message must be auth")
            return

        # Заменить старое соединение, если есть
        if server_token in active_connections:
            old = active_connections[server_token]
            try:
                await old.close(code=1000, reason="New connection")
            except:
                pass
            del active_connections[server_token]

        active_connections[server_token] = websocket
        await websocket.send_text(json.dumps({"status": "ok", "message": "Authenticated"}))

        while True:
            message = await websocket.receive_text()
            print(f"Message from {server_token}: {message}")
            # Эхо (или обработать результат команды)d
            await websocket.send_text(json.dumps({"echo": message}))
    except WebSocketDisconnect:
        pass
    finally:
        if server_token in active_connections:
            del active_connections[server_token]

@app.post("/api/users/{user_id}/servers/{server_id}/command")
async def send_command(user_id: int, server_id: int, command: dict = Body(...)):

    server_info = db.get_server_info(server_id)
    owner_id = db.get_server_owner_id(server_id)
    if owner_id != user_id:
        raise HTTPException(status_code=403, detail="Not owned")
    ws = active_connections.get(server_info["token"])
    if not ws:
        raise HTTPException(status_code=404, detail="RPi not connected")
    try:
        await ws.send_text(json.dumps(command))
        return {"status": "sent", "command": command}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)