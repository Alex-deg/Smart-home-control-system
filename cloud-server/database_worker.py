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
        sql = "CREATE TABLE users ( \
            id INTEGER PRIMARY KEY AUTOINCREMENT, \
            login TEXT NOT NULL, \
            password TEXT NOT NULL, \
        )"
        self.execute_request(sql)

    def create_servers_table(self):
        sql = "CREATE TABLE servers ( \
            id INTEGER PRIMARY KEY AUTOINCREMENT, \
            name TEXT NOT NULL, \
            token TEXT NOT NULL, \
        )"
        self.execute_request(sql)

    def create_users_servers_table(self):
        sql = "CREATE TABLE users_servers ( \
            id INTEGER PRIMARY KEY AUTOINCREMENT, \
            user_id INTEGER NOT NULL, \
            server_id INTEGER NOT NULL, \
            FOREIGN KEY (user_id) REFERENCES users(id), \
            FOREIGN KEY (server_id) REFERENCES server(id) \
        )"
        self.execute_request(sql)

    def clear_users_table(self):
        sql = "DELETE FROM users"
        self.execute_request(sql)

    def clear_servers_table(self):
        sql = "DELETE FROM servers"
        self.execute_request(sql)

    def clear_users_servers_table(self):
        sql = "DELETE FROM users_servers"
        self.execute_request(sql)
    
    def add_user(self, login, password):
        sql = "INSERT INTO users(login, password)" \
              "VALUES (?, ?, ?)"
        self.execute_request(sql, [login, password])
    
    def add_server(self, user_id, name, token):
        sql = "INSERT INTO servers(name, token)" \
              "VALUES (?, ?)"
        self.execute_request(sql, [name, token])
        sql = "SELECT" \
              "    id" \
              "FROM servers" \
              "ORDER BY id DESC" \
              "LIMIT 1;"
        response = self.execute_query(sql)
        server_id = response.get_int(0, "id")
        self.add_users_servers(user_id, server_id)

    def add_users_servers(self, user_id, server_id):
        sql = "INSERT INTO users_servers(user_id, server_id)"\
              "VALUES (?, ?)"
        self.execute_request(sql, [user_id, server_id])

    def delete_server(self, server_id):
        # Удаление из таблицы с серверами
        sql = "DELETE FROM servers" \
              "WHERE id = ?;"
        self.execute_request(sql, [server_id])
        # Удаление из сводной таблицы юзеры-серверы
        sql = "DELETE FROM users_servers" \
              "WHERE server_id = ?;"
        self.execute_request(sql, [server_id])

    def get_server_owner_id(self, server_id):
        sql = "SELECT" \
              "    user_id" \
              "FROM users_servers"\
              "WHERE server_id = ?"
        response = self.execute_query(sql, [server_id])
        return response.get_int(0, "user_id")

    def get_server_info(self, server_id):
        sql = "SELECT" \
              "    *" \
              "FROM servers" \
              "WHERE id = ?"
        response = self.execute_query(sql, [server_id])
        server_info = json()
        server_info["id"] = response.get_int(0, "id")
        server_info["name"] = response.get_str(0, "name")
        server_info["token"] = response.get_str(0, "token")
        return server_info

    def get_servers(self, user_id) -> List:
        list_of_servers = []
        sql = (
            "SELECT" 
            "    s.id as server_id,"
            "    s.name as server_name"
            "FROM users_servers us"
            "JOIN servers s ON us.server_id = s.id"
            "WHERE user_id = ?"
        )
        response = self.execute_query(sql, [user_id])
        server = {}
        for i in len(response):
            server["id"] = response.get_int(i, "server_id")
            server["name"] = response.get_str(i, "server_name")
            list_of_servers.append(json.dumps(server))
        return list_of_servers
       

