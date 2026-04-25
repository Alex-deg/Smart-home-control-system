import sqlite3
import json
from typing import Any, List, Dict, Tuple, Union, Optional
from dataclasses import dataclass, field
from contextlib import contextmanager

SQLValue = Union[str, int, float, None]


class DataBaseException(Exception):
    pass

@dataclass
class QueryResult:
    rows: List[List[SQLValue]] = field(default_factory=list)
    column_names: List[str] = field(default_factory=list)
    
    def empty(self) -> bool:
        return len(self.rows) == 0
    
    def size(self) -> int:
        return len(self.rows)
    
    def get(self, row: int, col: Union[int, str]) -> SQLValue:
        if row < 0 or row >= len(self.rows):
            raise IndexError(f"Row {row} out of range (0-{len(self.rows)-1})")
        
        if isinstance(col, int):
            if col < 0 or col >= len(self.rows[row]):
                raise IndexError(f"Column {col} out of range (0-{len(self.rows[row])-1})")
            return self.rows[row][col]
        
        elif isinstance(col, str):
            if col not in self.column_names:
                raise DataBaseException(f"Column not found: {col}")
            col_idx = self.column_names.index(col)
            return self.rows[row][col_idx]
        
        else:
            raise TypeError(f"col must be int or str, got {type(col)}")
    
    def get_int(self, row: int, col: Union[int, str]) -> int:
        val = self.get(row, col)
        if val is None:
            raise ValueError(f"Value at ({row}, {col}) is NULL")
        return int(val)
    
    def get_float(self, row: int, col: Union[int, str]) -> float:
        val = self.get(row, col)
        if val is None:
            raise ValueError(f"Value at ({row}, {col}) is NULL")
        return float(val)
    
    def get_str(self, row: int, col: Union[int, str]) -> str:
        val = self.get(row, col)
        if val is None:
            raise ValueError(f"Value at ({row}, {col}) is NULL")
        return str(val)
    
    def get_or_default(self, row: int, col: Union[int, str], default: SQLValue = None) -> SQLValue:
        try:
            return self.get(row, col)
        except (IndexError, DataBaseException):
            return default
    
    def to_dict_list(self) -> List[Dict[str, SQLValue]]:
        if not self.column_names:
            return []
        
        result = []
        for row in self.rows:
            row_dict = {}
            for i, col_name in enumerate(self.column_names):
                row_dict[col_name] = row[i] if i < len(row) else None
            result.append(row_dict)
        return result
    
    def to_json(self, indent: Optional[int] = None) -> str:
        return json.dumps(self.to_dict_list(), indent=indent, default=str)
    
    def __repr__(self) -> str:
        return f"QueryResult(rows={len(self.rows)}, cols={len(self.column_names)})"

class IDataBase:
    
    def __init__(self, path_to_database: Optional[str] = None):
        self._db: Optional[sqlite3.Connection] = None
        if path_to_database is not None:
            self.open(path_to_database)
    
    def __del__(self):
        self.close()
    
    def open(self, path_to_database: str) -> None:
        try:
            self._db = sqlite3.connect(path_to_database)
            self._db.row_factory = sqlite3.Row
            self._db.execute("PRAGMA foreign_keys = ON")
        except sqlite3.Error as e:
            raise DataBaseException(f"Failed to open database: {e}")
    
    def close(self) -> None:
        if self._db is not None:
            self._db.close()
            self._db = None
    
    def is_open(self) -> bool:
        return self._db is not None
    
    def execute_request(self, sql: str, params: Optional[List[SQLValue]] = None) -> None:
        if not self._db:
            raise DataBaseException("Database not opened")
        
        try:
            cursor = self._db.cursor()
            if params:
                cursor.execute(sql, params)
            else:
                cursor.execute(sql)
            self._db.commit()
            cursor.close()
        except sqlite3.Error as e:
            raise DataBaseException(f"SQL error: {e}\nQuery: {sql}")
    
    def execute_query(self, sql: str, params: Optional[List[SQLValue]] = None) -> QueryResult:
        if not self._db:
            raise DataBaseException("Database not opened")
        
        result = QueryResult()
        
        try:
            cursor = self._db.cursor()
            
            if params:
                cursor.execute(sql, params)
            else:
                cursor.execute(sql)
            
            if cursor.description:
                result.column_names = [col[0] for col in cursor.description]
            
            rows = cursor.fetchall()
            for row in rows:
                row_list = []
                for value in row:
                    if isinstance(value, (int, float, str, type(None))):
                        row_list.append(value)
                    elif isinstance(value, bytes):
                        row_list.append(value.decode('utf-8'))
                    else:
                        row_list.append(str(value))
                result.rows.append(row_list)
            
            cursor.close()
            
        except sqlite3.Error as e:
            raise DataBaseException(f"Query failed: {e}\nQuery: {sql}")
        
        return result
    
    @contextmanager
    def transaction(self):
        if not self._db:
            raise DataBaseException("Database not opened")
        
        try:
            self._db.execute("BEGIN TRANSACTION")
            yield
            self._db.commit()
        except Exception as e:
            self._db.rollback()
            raise DataBaseException(f"Transaction failed: {e}")
    
    def table_exists(self, table_name: str) -> bool:
        result = self.execute_query(
            "SELECT name FROM sqlite_master WHERE type='table' AND name=?",
            [table_name]
        )
        return not result.empty()
    
    def get_table_info(self, table_name: str) -> QueryResult:
        return self.execute_query(f"PRAGMA table_info({table_name})")
    
    def vacuum(self) -> None:
        self.execute_request("VACUUM")
    
    def backup(self, backup_path: str) -> None:
        if not self._db:
            raise DataBaseException("Database not opened")
        
        try:
            backup_conn = sqlite3.connect(backup_path)
            self._db.backup(backup_conn)
            backup_conn.close()
        except sqlite3.Error as e:
            raise DataBaseException(f"Backup failed: {e}")

class Database(IDataBase):

    def create_users_table(self):
        sql = (
            "CREATE TABLE users ( " 
            "    id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "    login TEXT NOT NULL, "
            "    password TEXT NOT NULL "
            ");"
            )
        self.execute_request(sql)

    def create_servers_table(self):
        sql = (
            "CREATE TABLE servers ( "
            "    id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "    name TEXT NOT NULL, "
            "    token TEXT NOT NULL "
            ");"
        )
        self.execute_request(sql)

    def create_users_servers_table(self):
        sql = (
            "CREATE TABLE users_servers ( "
            "    id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "    user_id INTEGER NOT NULL, "
            "    server_id INTEGER NOT NULL, "
            "    FOREIGN KEY (user_id) REFERENCES users(id), "
            "    FOREIGN KEY (server_id) REFERENCES servers(id) "
            ");"
        )
        self.execute_request(sql)

    def create_servers_modules_table(self):
        sql = (
            "CREATE TABLE server_modules ( "
            "    id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "    server_id INTEGER NOT NULL, "
            "    module_id INTEGER NOT NULL, "
            "    FOREIGN KEY (server_id) REFERENCES servers(id), "
            "    FOREIGN KEY (module_id) REFERENCES modules(id) "
            ");"
        )
        self.execute_request(sql)

    def create_modules_table(self):
        sql = (
            "CREATE TABLE modules ( "
            "    id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "    name TEXT NOT NULL, "
            "    alias TEXT NOT NULL, "
            "    mqtt_topic TEXT NOT NULL, "
            "    description TEXT "
            ");"
        )
        self.execute_request(sql)

    def create_capabilities_table(self):
        sql = (
            "CREATE TABLE capabilities ( "
            "    id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "    name TEXT NOT NULL "
            ");"
        )
        self.execute_request(sql)

    def create_modules_capabilities_table(self):
        sql = (
            "CREATE TABLE modules_capabilities ( "
            "    id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "    module_id INTEGER NOT NULL, "
            "    capability_id INTEGER NOT NULL, "
            "    FOREIGN KEY (module_id) REFERENCES modules(id), "
            "    FOREIGN KEY (capability_id) REFERENCES capabilities(id) "
            ");"
        )
        self.execute_request(sql)

    def clear_users_table(self):
        sql = "DELETE FROM users; "
        self.execute_request(sql)

    def clear_servers_table(self):
        sql = "DELETE FROM servers; "
        self.execute_request(sql)

    def clear_users_servers_table(self):
        sql = "DELETE FROM users_servers; "
        self.execute_request(sql)

    def clear_modules_table(self):
        sql = "DELETE FROM modules; "
        self.execute_request(sql)
    
    def clear_servers_modules_table(self):
        sql = "DELETE FROM servers_modules; "
        self.execute_request(sql)

    def clear_capabilities_table(self):
        sql = "DELETE FROM capabilities; "
        self.execute_request(sql)

    def clear_modules_capabilities_table(self):
        sql = "DELETE FROM modules_capabilities; "
        self.execute_request(sql)

    def add_user(self, login, password):
        sql = (
            "INSERT INTO users(login, password) " 
            "VALUES (?, ?);"
        )
        self.execute_request(sql, [login, password])
    
    def add_server(self, user_id, name, token):
        # Добавление в таблицу servers
        sql = (
            "INSERT INTO servers(name, token) "
            "VALUES (?, ?);"
        )
        self.execute_request(sql, [name, token])
        sql = (
            "SELECT " 
            "    id " 
            "FROM servers " 
            "ORDER BY id DESC " 
            "LIMIT 1;"
        )
        response = self.execute_query(sql)
        server_id = response.get_int(0, "id")
        # Добавление в сводную таблицу users_servers
        self.add_users_servers(user_id, server_id)

    def add_users_servers(self, user_id, server_id):
        sql = (
            "INSERT INTO users_servers(user_id, server_id) "
            "VALUES (?, ?);"
        )
        self.execute_request(sql, [user_id, server_id])

    def add_module(self, server_id, name, alias, 
                         mqtt_topic, description = ""):
        # Добавление в таблицу modules
        sql = (
            "INSERT INTO modules(name, alias, mqtt_topic, description) " 
            "VALUES (?, ?, ?, ?);"
        )
        self.execute_request(sql, [name, alias, mqtt_topic, description])
        sql = (
            "SELECT " 
            "    id " 
            "FROM modules " 
            "ORDER BY id DESC " 
            "LIMIT 1;"
        )
        response = self.execute_query(sql)
        module_id = response.get_int(0, "id")
        # Добавление в сводную таблицу servers_modules
        self.add_servers_modules(server_id, module_id)

    def add_servers_modules(self, server_id, module_id):
        sql = (
            "INSERT INTO servers_modules(server_id, module_id) " 
            "VALUES (?, ?);"
        )
        self.execute_request(sql, [server_id, module_id])

    def add_capability(self, module_id, name):
        # Добавление в таблицу modules
        sql = (
            "INSERT INTO capabilities(name) " 
            "VALUES (?);"
        )
        self.execute_request(sql, [name])
        sql = (
            "SELECT " 
            "    id " 
            "FROM capabilities " 
            "ORDER BY id DESC " 
            "LIMIT 1; "
        )
        response = self.execute_query(sql)
        capability_id = response.get_int(0, "id")
        # Добавление в сводную таблицу modules_capabilities
        self.add_modules_capabilities(module_id, capability_id)

    def add_modules_capabilities(self, module_id, capability_id):
        sql = (
            "INSERT INTO modules_capabilities(module_id, capability_id) " 
            "VALUES (?, ?);"
        )
        self.execute_request(sql, [module_id, capability_id])

    def delete_server_from_tables(self, server_id):
        # Удаление из сводной таблицы юзеры-серверы
        sql = (
            "DELETE FROM users_servers " 
            "WHERE server_id = ?;"
        )
        self.execute_request(sql, [server_id])
        # Удаление из таблицы с серверами
        sql = (
            "DELETE FROM servers " 
            "WHERE id = ?;"
        )
        self.execute_request(sql, [server_id])
        sql = (
            "SELECT " 
            "    module_id " 
            "FROM servers_modules " 
            "WHERE server_id = ?;"
        )
        response = self.execute_query(sql, [server_id])
        # Удаление привязанных модулей
        for i in range(response.size()):
            module_id = response.get_int(i, "module_id")
            self.delete_module_from_tables(module_id)

    def delete_module_from_tables(self, module_id):
        # Удаление из сводной таблицы серверы-модули
        sql = (
            "DELETE FROM servers_modules " 
            "WHERE module_id = ?;"
        )
        self.execute_request(sql, [module_id])
        # Удаление из таблицы с модулями
        sql = (
            "DELETE FROM modules " 
            "WHERE id = ?;"
        )
        self.execute_request(sql, [module_id])
        # Удаление из сводной таблицы модули-возможности
        sql = (
            "DELETE FROM modules_capabilities " 
            "WHERE module_id = ?;"
        )
        self.execute_request(sql, [module_id])

    def delete_capability_from_tables(self, capability_id):
        # Удаление из сводной таблицы модули-возможности
        sql = (
            "DELETE FROM modules_capabilities " 
            "WHERE capability_id = ?;"
        )
        self.execute_request(sql, [capability_id])
        # Удаление из таблицы с возможностями
        sql = (
            "DELETE FROM capabilities " 
            "WHERE id = ?;"
        )
        self.execute_request(sql, [capability_id])

    def unbind_module_capability(self, module_id, capability_id):
        sql = (
            "DELETE FROM modules_capabilities " 
            "WHERE module_id = ? AND capability_id = ?;"
        )
        self.execute_request(sql, [module_id, capability_id])

    def get_servers(self, user_id) -> List:
        list_of_servers = []
        sql = (
            "SELECT " 
            "    s.id as server_id, "
            "    s.name as server_name "
            "FROM users_servers us "
            "JOIN servers s ON us.server_id = s.id "
            "WHERE user_id = ?; "
        )
        response = self.execute_query(sql, [user_id])
        server = {}
        for i in response.size():
            server["id"] = response.get_int(i, "server_id")
            server["name"] = response.get_str(i, "server_name")
            list_of_servers.append(json.dumps(server))
        return list_of_servers
       
    def get_modules(self, server_id) -> List:
        list_of_modules = []
        sql = (
            "SELECT " 
            "    m.id, "
            "    m.name, "
            "    m.alias, "
            "    m.mqtt_topic, "
            "    m.description "
            "FROM servers_modules sm "
            "JOIN modules m ON sm.module_id = m.id "
            "WHERE server_id = ?;"
        )
        response = self.execute_query(sql, [server_id])
        module = {}
        for i in response.size():
            module["id"] = response.get_int(i, "id")
            module["name"] = response.get_str(i, "name")
            module["alias"] = response.get_str(i, "alias")
            module["mqtt_topic"] = response.get_str(i, "mqtt_topic")
            module["description"] = response.get_str(i, "description")
            list_of_modules.append(json.dumps(module))
        return list_of_modules
       
    def get_capabilities(self, module_id) -> List:
        list_of_capabilities = []
        sql = (
            "SELECT " 
            "    c.id, "
            "    c.name "
            "FROM modules_capabilities mc "
            "JOIN capabilities c ON mc.capability = c.id "
            "WHERE module_id = ?;"
        )
        response = self.execute_query(sql, [module_id])
        capability = {}
        for i in response.size():
            capability["id"] = response.get_int(i, "id")
            capability["name"] = response.get_str(i, "name")
            list_of_capabilities.append(json.dumps(capability))
        return list_of_capabilities
       
    def get_server_owner_id(self, server_id):
        sql = (
            "SELECT " 
            "    user_id " 
            "FROM users_servers "
            "WHERE server_id = ?;"
        )
        response = self.execute_query(sql, [server_id])
        return response.get_int(0, "user_id")

    def get_user_info(self, user_id):
        sql = (
            "SELECT " 
            "    * " 
            "FROM users " 
            "WHERE id = ?;"
        )
        response = self.execute_query(sql, [user_id])
        user_info = json()
        user_info["id"] = response.get_int(0, "id")
        user_info["login"] = response.get_str(0, "login")
        user_info["password"] = response.get_str(0, "password")
        return user_info        

    def get_server_info(self, server_id):
        sql = (
            "SELECT "
            "    * " 
            "FROM servers "
            "WHERE id = ?;"
        )
        response = self.execute_query(sql, [server_id])
        server_info = json()
        server_info["id"] = response.get_int(0, "id")
        server_info["name"] = response.get_str(0, "name")
        server_info["token"] = response.get_str(0, "token")
        return server_info
    
    def get_module_info(self, module_id):
        sql = (
            "SELECT " 
            "    * " 
            "FROM modules " 
            "WHERE id = ?;"
        )
        response = self.execute_query(sql, [module_id])
        module_info = json()
        module_info["id"] = response.get_int(0, "id")
        module_info["name"] = response.get_str(0, "name")
        module_info["alias"] = response.get_str(0, "alias")
        module_info["mqtt_topic"] = response.get_str(0, "mqtt_topic")
        module_info["description"] = response.get_str(0, "description")
        return module_info
    
    def get_capability_info(self, capability_id):
        sql = (
            "SELECT " 
            "    * " 
            "FROM capabilities " 
            "WHERE id = ?;"
        )
        response = self.execute_query(sql, [capability_id])
        capability_info = json()
        capability_info["id"] = response.get_int(0, "id")
        capability_info["name"] = response.get_str(0, "name")
        return capability_info
    
    def check_auth(self, login, password):
        sql = (
            "SELECT " 
            "    id, "
            "    login, "
            "    password "
            "FROM users WHERE login = ?;"
        )
        response = response = self.execute_query(sql, [login])

        if response.size() == 0:
            return False, -1
        if response.get_str(0, "password") != password:
            return False, -1

        return True, response.get_int(0, "id")

    def delete_table_by_name(self, name : str):
        sql = "DROP TABLE " + name
        self.execute_request(sql)
