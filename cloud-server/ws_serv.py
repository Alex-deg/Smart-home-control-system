from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, Body
from dotenv import load_dotenv
from typing import Dict
import database_worker
import uvicorn
import asyncio
import uuid
import json
import os

load_dotenv(".env") 

path_to_db = os.getenv("PATH_TO_DB")

app = FastAPI()
db = database_worker.Database(path_to_db)

active_connections: Dict[str, WebSocket] = {}
temp_servers: Dict[str, Dict] = {}
pending_requests: Dict[str, asyncio.Future] = {}

@app.get("/")
async def get_home():
    return "Hello"

@app.post("/api/auth")
async def auth(login : str = Body(...), password : str = Body(...)):
    status, id = db.auth(login, password)
    if status:
        return f"You have been autorized with id = {id}"
    return "Incorrect login or password"

@app.post("/api/registration")
async def registration(login : str = Body(...), password : str = Body(...)):
    db.add_user(login, password)
    return "success"

@app.get("/api/users/{user_id}/servers")
async def show_servers(user_id: int):
    if db.check_auth(user_id):
        return db.get_servers(user_id)
    return "non authenticated"

@app.post("/api/users/{user_id}/servers/add")
async def add_server(user_id: int, name : str = Body(...)):
    if db.check_auth(user_id):
        server_token = str(uuid.uuid4())
        temp_servers[server_token] = {"user_id" : user_id, "server_name" : name}
        return {"server_token": server_token}
    return "non authenticated"

@app.post("/api/users/{user_id}/servers/connect")
async def add_server(user_id: int, token : str = Body(...)):
    if db.check_auth(user_id):
        server_id = db.get_server_id_by_token(token)
        if server_id <= 0:
            return "incorrect token"
        db.add_users_servers(user_id, server_id)
        return "you are connected to an existing server"
    return "non authenticated"

@app.delete("/api/users/{user_id}/servers/{server_id}/delete")
async def delete_server(user_id : int, server_id: int):
    if db.check_auth(user_id):
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
    return "non authenticated"

@app.get("/api/users/{user_id}/servers/{server_id}/modules")
async def show_modules(user_id: int, server_id: int):
    if db.check_auth(user_id):
        return db.get_modules(server_id)
    return "non authenticated"

@app.post("/api/users/{user_id}/servers/{server_id}/modules/add")
async def add_module(user_id : int, server_id : int, name : str = Body(...), alias : str = Body(...), 
                     mqtt_topic : str = Body(...), description : str = Body(...)):
    
    if db.check_auth(user_id):
        db.add_module(server_id, name, alias, mqtt_topic, description)
        return True
    return "non authenticated"
    
@app.delete("/api/users/{user_id}/servers/{server_id}/modules/{module_id}/delete")
async def delete_module(user_id : int, module_id : int):
    if db.check_auth(user_id):
        db.delete_module_from_tables(module_id)
        return True
    return "non authenticated"

@app.get("/api/users/{user_id}/servers/{server_id}/modules/{module_id}/capabilities")
async def show_capabilities(user_id: int, server_id: str, module_id: str):
    if db.check_auth(user_id):
        return db.get_capabilities(module_id)
    return "non authenticated"

@app.post("/api/users/{user_id}/servers/{server_id}/modules/{module_id}/capabilities/add")
async def add_capability(user_id : int, module_id : int, name : str = Body(...)):
    if db.check_auth(user_id):
        db.add_capability(module_id, name)
        return True
    return "non authenticated"
    
@app.delete("/api/users/{user_id}/servers/{server_id}/modules/{module_id}/capabilities/{capability_id}/delete")
async def delete_capability(user_id : int, capability_id : int):
    if db.check_auth(user_id):
        db.delete_capability_from_tables(capability_id)
    return "non authenticated"

@app.post("/api/users/{user_id}/servers/{server_id}/modules/{module_id}/capabilities/{capability_id}/unbind")
async def undind_module_capability(user_id : int, module_id : int, capability_id : int):
    if db.check_auth(user_id):
        db.unbind_module_capability(module_id, capability_id)
        return True
    return "non authenticated"

@app.post("/api/users/{user_id}/servers/{server_id}/modules/{module_id}/capability/{capbility_id}/send_command")
async def send_command(user_id: int, server_id: int, module_id : int, capability_id : int):

    if db.check_auth(user_id):
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

            message  =  {
                            "type" : "command",
                            "request_id" : request_id,
                            "params" : {
                                "mqtt_topic" : module_info["mqtt_topic"], 
                                "payload" : capability_info["name"]
                            }
                        }
            await ws.send_text(json.dumps(message))
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
    return "non authenticated"

@app.post("/api/users/{user_id}/servers/{server_id}/add_scenario")
async def add_scenario_and_send(user_id: int, server_id : int, scenario_name : str, 
                                                               scenario_condition : str, 
                                                               scenario_acts : list[int]): 
    if db.check_auth(user_id):
        server_info = db.get_server_info(server_id)
        owner_id = db.get_server_owner_id(server_id)
        if owner_id != user_id:
            raise HTTPException(status_code=403, detail="Not owned")
        ws = active_connections.get(server_info["token"])
        if not ws:
            raise HTTPException(status_code=404, detail="RPi not connected")
        try:
            message  =  {
                            "type" : "scenario",
                            "scenario" : {
                                "name" : scenario_name,
                                "condition" : scenario_condition,
                                "acts" : scenario_acts
                            }
                        }
            await ws.send_text(json.dumps(message))
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))
    return "non authenticated"
    
@app.get("/api/get_act_info/{act_id}")
async def get_act_info(act_id : int):
    if db.check_auth(db.get_user_id_by_act_id(act_id)):
        return db.get_act_info(act_id)
    return "non authenticated"
    
@app.post("/api/auto_detect")
async def auto_detect(token : str = Body(...), name : str = Body(...), alias : str = Body(...), 
                      mqtt_topic : str = Body(...), description : str = Body(...)):
    
    server_id = db.get_server_id_by_token(token)
    if db.check_auth(db.get_user_id_by_server_id(server_id)):
        module_id = db.add_module(server_id, name, alias, mqtt_topic, description)
        return {"module_id" : module_id}
    return "non authenticated"



@app.websocket("/ws/bind_server/{server_token}")
async def websocket_endpoint(websocket: WebSocket, server_token: str):

    is_server_exist = db.is_server_exist(server_token)

    if not is_server_exist and server_token not in temp_servers:
        await websocket.close(code=1008, reason="Unknown server_id")
        return

    if not is_server_exist:
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
            if message["type"] == "response":
                request_id = message["request_id"]
                if request_id in pending_requests:
                    pending_requests[request_id].set_result(message["payload"])
                else:
                    print(f"Unknown request_id: {request_id}")
            elif message["type"] == "alert":
                print(message)
    except WebSocketDisconnect:
        pass
    finally:
        if server_token in active_connections:
            del active_connections[server_token]

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)