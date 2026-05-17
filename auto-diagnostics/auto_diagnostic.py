import json
import logging
from datetime import datetime, timedelta
from pathlib import Path

import numpy as np
from sklearn.ensemble import IsolationForest
from sklearn.preprocessing import StandardScaler
import joblib
# import database_worker
import paho.mqtt.client as mqtt
import requests

LOCAL_DB = "../cpp-server/Data/smart_home.db"  # путь к БД на RPi
MODEL_DIR = "./models"                         # директория для моделей
MODEL_PREFIX = "isoforest"                     # префикс файлов модели

LOCAL_SERVER_IP = "192.168.0.105"
LOCAL_SERVER_PORT = 8080
BASE_API_URL = "http://" + LOCAL_SERVER_IP + ":" + LOCAL_SERVER_PORT + "/"

# Параметры обучения
TRAIN_LOOKBACK_DAYS = 30          # обучаем на данных за последние 30 дней
RETRAIN_DAY_OF_WEEK = 6           # воскресенье (0=пн, 6=вс)
CONTAMINATION = 0.05              # ожидаемая доля аномалий (5%)
N_ESTIMATORS = 100                # количество деревьев

# Параметры детекции
ANOMALY_THRESHOLD = -0.2          # порог score_samples (ниже -> аномалия)
                                  # можно не использовать, полагаясь на predict

RECORDS_THRESHOLD = 50 # необходимо минимум 50 записей в БД для одного модуля для проведения
                       # обучения на этом модуле

MQTT_BROKER = "localhost"  
MQTT_PORT = 1883
MQTT_TOPIC = "rpi/send_message/remote"

# database = database_worker.Database()
mqtt_client = mqtt.Client()

mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)


def model_paths(module_id):
    base = MODEL_DIR / f"{MODEL_PREFIX}_{module_id}"
    return str(base.with_suffix('.pkl')), str(base.with_suffix('_scaler.pkl'))

def save_anomaly_flag(module_id, record_ids):
    if not record_ids:
        return
    requests.post(BASE_API_URL + 'api/database/params/anomaly_tagging', json={'record_ids' : record_ids}) 

def send_alert(module_id, anomaly_records):

    latest = anomaly_records[-1]
    
    temp = latest['module_temp']
    free_bytes = latest['free_bytes']
    
    if temp > 75:
        tag = "critical"
        msg = f"Перегрев модуля {module_id}: {temp}°C"
    elif free_bytes < 20000:
        tag = "warning"
        msg = f"Критически мало памяти на модуле {module_id}: {free_bytes//1024} КБ"
    else:
        tag = "info"
        msg = f"Аномалия на модуле {module_id}: температура = {temp}°C, память = {free_bytes//1024} КБ"
    
    payload = {
        "module_id": module_id,
        "timestamp": latest['timestamp'],
        "severity": tag,
        "message": msg,
        "diagnostic": {
            "temperature": temp,
            "free_heap": free_bytes
        }
    }
    
    try:
        mqtt_client.publish(MQTT_TOPIC, json.dumps(payload), qos=1)
    except Exception as e:
        logging.error(f"Ошибка соединения с удалённым сервером: {e}")


def train_model_for_module(module_id):

    url = BASE_API_URL + 'api/database/params?module_id=' + str(module_id) + \
          '&time_interval=' + str(TRAIN_LOOKBACK_DAYS * 24) + '&anomaly=0'
    df = requests.get(url)

    if df is None or len(df) < RECORDS_THRESHOLD:
        return False
    
    X = []
    for record in df:
        X.append([
            record["module_temp"],   # температура
            record["free_bytes"]     # свободная память
        ])
    
    # Нормализация
    scaler = StandardScaler()
    X_scaled = scaler.fit_transform(X)
    
    # Обучение модели
    model = IsolationForest(
        contamination=CONTAMINATION,
        n_estimators=N_ESTIMATORS,
        random_state=42,
        n_jobs=-1
    )
    model.fit(X_scaled)
    
    model_path, scaler_path = model_paths(module_id)
    joblib.dump(model, model_path)
    joblib.dump(scaler, scaler_path)
    
    preds = model.predict(X_scaled)
    anomaly_ratio = (preds == -1).sum() / len(preds)
    print(f"Модуль {module_id}: модель сохранена, аномалий в обучении: {anomaly_ratio:.2%}")
    return True

def retrain_all_modules():

    url = BASE_API_URL + 'api/database/params/unique_modules'
    module_ids = requests.get(url)
    
    for module_id in module_ids:
        train_model_for_module(module_id)

def need_retraining():
    return datetime.today().weekday() == RETRAIN_DAY_OF_WEEK


def detect_anomalies_for_module(module_id):
    model_path, scaler_path = model_paths(module_id)
    if not Path(model_path).exists():
        if not train_model_for_module(module_id):
            return
    
    model = joblib.load(model_path)
    scaler = joblib.load(scaler_path)
    
    # Берём данные за последние 24 часа
    url = BASE_API_URL + 'api/database/params?module_id=' + str(module_id) + \
          '&time_interval=24&anomaly=0'
    df = requests.get(url)
    if not df:
        return
        
    X_new = []
    for record in df:
        X_new.append([
            record["module_temp"],
            record["free_bytes"]
        ])
    X_new = np.array(X_new)
    
    X_scaled = scaler.transform(X_new)
    predictions = model.predict(X_scaled)   # 1 = норма, -1 = аномалия
    
    anomaly_indices = np.where(predictions == -1)[0]
    if len(anomaly_indices) == 0:
        return
    
    # Получаем ID записей, которые нужно пометить
    record_ids = []
    anomaly_records = []
    for idx in anomaly_indices:
        record = df[idx]
        if 'id' in record:
            record_ids.append(record['id'])
        
        anomaly_records.append({
            'timestamp': record['timestamp'],
            'module_temp': record['module_temp'],
            'free_bytes': record['free_bytes']
        })
    
    save_anomaly_flag(module_id, record_ids)
    
    send_alert(module_id, anomaly_records)

def daily_detection():
    url = BASE_API_URL + 'api/database/params/unique_modules'
    module_ids = requests.get(url)
    for module_id in module_ids:
        detect_anomalies_for_module(module_id)

def main():

    if need_retraining():
        retrain_all_modules()

    daily_detection()
    
if __name__ == "__main__":
    main()
    mqtt_client.disconnect()