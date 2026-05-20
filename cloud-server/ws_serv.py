from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, Body
from typing import Dict
import uuid
import json
import uvicorn
import database_worker
import asyncio

app = FastAPI()

path_to_db = "Data/users_database.db"

db = database_worker.Database(path_to_db)

active_connections: Dict[str, WebSocket] = {}
temp_servers: Dict[str, Dict] = {}
pending_requests: Dict[str, asyncio.Future] = {}

@app.get("/")
async def get_home():
    return "Hello"

@app.post("/api/auth")
async def auth(login : str = Body(...), password : str = Body(...)):
    status, id = db.check_auth(login, password)
    if status:
        return f"You have been autorized with id = {id}"
    return "Incorrect login or password"

@app.post("/api/registration")
async def registration(login : str = Body(...), password : str = Body(...)):
    db.add_user(login, password)
    # Мб при создании чего-либо возвращать id созданной записи?
    return "success"

@app.get("/api/users/{user_id}/servers")
async def show_servers(user_id: int):
    return db.get_servers(user_id)

@app.post("/api/users/{user_id}/servers/add")
async def add_server(user_id: int, name : str = Body(...)):
    server_token = str(uuid.uuid4())
    temp_servers[server_token] = {"user_id" : user_id, "server_name" : name}
    return {"server_token": server_token}

@app.delete("/api/users/{user_id}/servers/{server_id}/delete")
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
    db.delete_server_from_tables(server_id)
    return True

@app.get("/api/users/{user_id}/servers/{server_id}/modules")
async def show_modules(user_id: int, server_id: int):
    return db.get_modules(server_id)

@app.post("/api/users/{user_id}/servers/{server_id}/modules/add")
async def add_module(server_id : int, name : str = Body(...), alias : str = Body(...), 
                     mqtt_topic : str = Body(...), description : str = Body(...)):
    db.add_module(server_id, name, alias, mqtt_topic, description)
    return True

@app.delete("/api/users/{user_id}/servers/{server_id}/modules/{module_id}/delete")
async def delete_module(module_id : int):
    db.delete_module_from_tables(module_id)

@app.get("/api/users/{user_id}/servers/{server_id}/modules/{module_id}/capabilities")
async def show_capabilities(user_id: int, server_id: str, module_id: str):
    return db.get_capabilities(module_id)

@app.post("/api/users/{user_id}/servers/{server_id}/modules/{module_id}/capabilities/add")
async def add_capability(module_id : int, name : str = Body(...)):
    db.add_capability(module_id, name)
    return True

@app.delete("/api/users/{user_id}/servers/{server_id}/modules/{module_id}/capabilities/{capability_id}/delete")
async def delete_capability(capability_id : int):
    db.delete_capability_from_tables(capability_id)

@app.post("/api/users/{user_id}/servers/{server_id}/modules/{module_id}/capabilities/{capability_id}/unbind")
async def undind_module_capability(module_id : int, capability_id : int):
    db.unbind_module_capability(module_id, capability_id)
    return True

@app.post("/api/users/{user_id}/servers/{server_id}/modules/{module_id}/capability/{capbility_id}/send_command")
async def send_command(user_id: int, server_id: int, module_id : int, capability_id : int):

    server_info = db.get_server_info(server_id)
    owner_id = db.get_server_owner_id(server_id)
    if owner_id != user_id:
        raise HTTPException(status_code=403, detail="Not owned")
    ws = active_connections.get(server_info["token"])
    if not ws:
        raise HTTPException(status_code=404, detail="RPi not connected")
    try:
        module_info = db.get_module_info(module_id)
        capability_info = db.get_capability_info(capability_id)

        request_id = str(uuid.uuid4())
        future = asyncio.Future()
        pending_requests[request_id] = future

        command  =  {
                        "type" : "command",
                        "request_id" : request_id,
                        "params" : {
                            "mqtt_topic" : module_info["mqtt_topic"], 
                            "payload" : capability_info["name"]
                        }
                    }
        await ws.send_text(json.dumps(command))
        try:
            # мб задать таймаут переменной
            response = await asyncio.wait_for(future, timeout=10.0)
            return {"status": "ok", "result": response}
        except asyncio.TimeoutError:
            raise HTTPException(status_code=408, detail="Request timeout")
        finally:
            pending_requests.pop(request_id, None)
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/users/{user_id}/servers/{server_id}/add_scenario")
async def add_scenario_and_send(user_id: int, server_id : int, scenario_json : json):
    server_info = db.get_server_info(server_id)
    owner_id = db.get_server_owner_id(server_id)
    if owner_id != user_id:
        raise HTTPException(status_code=403, detail="Not owned")
    ws = active_connections.get(server_info["token"])
    if not ws:
        raise HTTPException(status_code=404, detail="RPi not connected")
    try:
        await ws.send_text(scenario_json.dumps())
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.websocket("/ws/bind_server/{server_token}")
async def websocket_endpoint(websocket: WebSocket, server_token: str):
    if server_token not in temp_servers:
        await websocket.close(code=1008, reason="Unknown server_id")
        return

    db.add_server(temp_servers[server_token]["user_id"], temp_servers[server_token]["server_name"], server_token)
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
        await websocket.send_text(json.dumps({"type" : "info", "status": "ok", "message": "Authenticated"}))

        while True:
            data = await websocket.receive_text()
            message = json.loads(data)
            if "request_id" in message:
                request_id = message["request_id"]
                if request_id in pending_requests:
                    pending_requests[request_id].set_result(message["payload"])
                else:
                    print(f"Unknown request_id: {request_id}")
    except WebSocketDisconnect:
        pass
    finally:
        if server_token in active_connections:
            del active_connections[server_token]

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)