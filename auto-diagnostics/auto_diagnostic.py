from sklearn.ensemble import IsolationForest
from sklearn.preprocessing import StandardScaler
from typing import List, Dict, Any
from dotenv import load_dotenv
from datetime import datetime
from pathlib import Path
import paho.mqtt.client as mqtt
import numpy as np
import requests
import joblib
import json
import os

load_dotenv(".env") 

BASE_API_URL = f"http://{os.getenv("LOCAL_SERVER_IP")}:{os.getenv("LOCAL_API_PORT")}"
MQTT_BROKER  = os.getenv("MQTT_BROKER")
MQTT_PORT    = os.getenv("MQTT_PORT")
MQTT_TOPIC   = os.getenv("MQTT_TOPIC") # topic for send message to remote server
MODEL_DIR    = os.getenv("MODEL_DIR")
FETCH_DIAGNOSTIC_DATA_ENDPOINT = os.getenv("FETCH_DIAGNOSTIC_DATA_ENDPOINT")
FETCH_MODULE_IDS_ENDPOINT      = os.getenv("FETCH_MODULE_IDS_ENDPOINT")
ANOMALY_TAGGING_ENDPOINT       = os.getenv("ANOMALY_TAGGING_ENDPOINT")

# Параметры обучения
TRAIN_LOOKBACK_DAYS = 30          # обучаем на данных за последние 30 дней
RETRAIN_DAY_OF_WEEK = 6           # воскресенье (0=пн, 6=вс)
CONTAMINATION       = 0.05        # ожидаемая доля аномалий (5%)
N_ESTIMATORS        = 100         # количество деревьев

# Параметры детекции
ANOMALY_THRESHOLD   = -0.2        # порог (ниже -> аномалия) (значение параметра -1 <= x <= 1)
RECORDS_THRESHOLD   = 50          # необходимо минимум 50 записей в БД для одного модуля для проведения
                                  # обучения на этом модуле (рекомендуемый минимум)

class DiagnosticModule:

    def __init__(self, base_api_url: str, model_dir: str,
                 train_lookback_days: int, retrain_day_of_week: int,
                 contamination: float, n_estimators: int,
                 records_threshold: int, mqtt_broker: str,
                 mqtt_port: int, mqtt_topic: str):
        
        self._base_api_url = base_api_url.rstrip('/')
        self._model_dir = Path(model_dir)
        self._model_dir.mkdir(parents=True, exist_ok=True)

        self._train_lookback_days = train_lookback_days
        self._retrain_day_of_week = retrain_day_of_week
        self._contamination = contamination
        self._n_estimators = n_estimators
        self._records_threshold = records_threshold

        self._mqtt_broker = mqtt_broker
        self._mqtt_port = mqtt_port
        self._mqtt_topic = mqtt_topic

        self._mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self._mqtt_client.connect(self._mqtt_broker, self._mqtt_port, 60)
        self._mqtt_client.loop_start()

    
    def run(self) -> None:
        if self._need_retraining():
            self._retrain_all_modules()
        self._daily_detection()

    def shutdown(self) -> None:
        self._mqtt_client.loop_stop()
        self._mqtt_client.disconnect()
    
    def _need_retraining(self) -> bool:
        return datetime.today().weekday() == self._retrain_day_of_week

    def _model_paths(self, module_id: int) -> tuple[str, str]:
        base = self._model_dir / f"isoforest_{module_id}"
        return str(base.with_suffix('.pkl')), str(base.with_suffix('_scaler.pkl'))

    def _fetch_module_ids(self) -> List[int]:
        url = f"{self._base_api_url}{FETCH_MODULE_IDS_ENDPOINT}"
        resp = requests.get(url, timeout=10)
        resp.raise_for_status()
        return resp.json() 
    
    def _fetch_diagnostic_data(self, module_id: int, hours_back: int, anomaly_flag: bool) -> List[Dict[str, Any]]:
        url = (f"{self._base_api_url}{FETCH_DIAGNOSTIC_DATA_ENDPOINT}"
               f"?module_id={module_id}&time_interval={hours_back}&anomaly={anomaly_flag}")
        resp = requests.get(url, timeout=10)
        resp.raise_for_status()
        return resp.json()

    def _save_anomaly_flag(self, module_id: int, record_ids: List[int]) -> None:
        if not record_ids:
            return
        url = f"{self._base_api_url}{ANOMALY_TAGGING_ENDPOINT}"
        requests.post(url, json={'record_ids': record_ids}, timeout=5)

    def _send_alert(self, module_id: int, anomaly_records: List[Dict[str, Any]]) -> None:
        
        latest = anomaly_records[-1]
        temp = latest['module_temp']
        free_bytes = latest['free_bytes']

        if temp > 75:
            severity = "critical"
            msg = f"Перегрев модуля {module_id}: {temp}°C"
        elif free_bytes < 20000:
            severity = "warning"
            msg = f"Критически мало памяти на модуле {module_id}: {free_bytes // 1024} КБ"
        else:
            severity = "info"
            msg = f"Аномалия на модуле {module_id}: температура = {temp}°C, память = {free_bytes // 1024} КБ"

        payload = {
            "type" : "alert",
            "module_id": module_id,
            "timestamp": latest['timestamp'],
            "severity": severity,
            "message": msg,
            "diagnostic": {
                "temperature": temp,
                "free_heap": free_bytes
            }
        }
        try:
            self._mqtt_client.publish(self._mqtt_topic, json.dumps(payload), qos=1)
        except Exception as e:
            print(f"Ошибка MQTT публикации: {e}")

    def _train_model_for_module(self, module_id: int) -> bool:
        data = self._fetch_diagnostic_data(module_id,
                                           self._train_lookback_days * 24,
                                           anomaly_flag=0)  
        if len(data) < self._records_threshold:
            return False

        X = np.array([[record['module_temp'], record['free_bytes']] for record in data])

        scaler = StandardScaler()
        X_scaled = scaler.fit_transform(X)

        model = IsolationForest(contamination=self._contamination,
                                n_estimators=self._n_estimators,
                                random_state=42,
                                n_jobs=-1)
        model.fit(X_scaled)

        model_path, scaler_path = self._model_paths(module_id)
        joblib.dump(model, model_path)
        joblib.dump(scaler, scaler_path)

        preds = model.predict(X_scaled)
        anomaly_ratio = (preds == -1).sum() / len(preds)
        print(f"Модуль {module_id}: модель сохранена, аномалий в обучении: {anomaly_ratio:.2%}")
        return True

    def _retrain_all_modules(self) -> None:
        modules = self._fetch_module_ids()
        for module_id in modules:
            self._train_model_for_module(module_id)

    def _detect_anomalies_for_module(self, module_id: int) -> None:
        model_path, scaler_path = self._model_paths(module_id)
        if not Path(model_path).exists():
            if not self._train_model_for_module(module_id):
                return

        model = joblib.load(model_path)
        scaler = joblib.load(scaler_path)

        data = self._fetch_diagnostic_data(module_id, hours_back=24, anomaly_flag=0)
        if not data:
            return

        X_new = np.array([[record['module_temp'], record['free_bytes']] for record in data])

        X_scaled = scaler.transform(X_new)
        predictions = model.predict(X_scaled)   # 1 = норма, -1 = аномалия

        anomaly_indices = np.where(predictions == -1)[0]
        if len(anomaly_indices) == 0:
            return

        record_ids = []
        anomaly_records = []
        for idx in anomaly_indices:
            record = data[idx]
            if 'id' in record:
                record_ids.append(record['id'])
            anomaly_records.append({
                'timestamp': record['timestamp'],
                'module_temp': record['module_temp'],
                'free_bytes': record['free_bytes']
            })

        self._save_anomaly_flag(module_id, record_ids)
        self._send_alert(module_id, anomaly_records)

    def _daily_detection(self) -> None:
        modules = self._fetch_module_ids()
        for module_id in modules:
            self._detect_anomalies_for_module(module_id)


if __name__ == "__main__":
    diagnostic = DiagnosticModule(base_api_url=BASE_API_URL,
                                  model_dir=MODEL_DIR,
                                  train_lookback_days=TRAIN_LOOKBACK_DAYS,
                                  retrain_day_of_week=RETRAIN_DAY_OF_WEEK,
                                  contamination=CONTAMINATION,
                                  n_estimators=N_ESTIMATORS,
                                  records_threshold=RECORDS_THRESHOLD,
                                  mqtt_broker=MQTT_BROKER,
                                  mqtt_port=MQTT_PORT,
                                  mqtt_topic=MQTT_TOPIC)
    try:
        diagnostic.run()
    finally:
        diagnostic.shutdown()