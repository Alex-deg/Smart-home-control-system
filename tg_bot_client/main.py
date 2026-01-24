import telebot
import asyncio
import requests
from datetime import date
from telebot import types
from telebot.types import ReplyKeyboardMarkup, ReplyKeyboardRemove
import os
import json
from dotenv import load_dotenv
import time

# Загружаем .env
load_dotenv()

class states:
    WAITING_USERS_NAME = 0                  # Ожидание ввода имени пользователя \ 
                                            #                                     authentification
    WAITING_USERS_PASSWORD = 1              # Ожидание ввода пароля             /
    WAITING_AUTHENTIFICATION_RESPONSE = 2   # Ожидание результата аутентификации
    AUTHORIZED = 3                          # Пользователь авторизован
    WAITING_ACTUATOR_DEVICE_ID = 4          # Ожидание ввода номера устройства для одиночного действия
    ACTUATOR_DEVICE_ID_RECIEVED = 5         # ID устройства для взаимодействия получен


bot = telebot.TeleBot(os.getenv("TELEGRAM_BOT_TOKEN", ""))
BASE_API_URL = os.getenv("API_BASE_URL", "")

# словарь состояний пользователей
users_states = {}
# временные данные
temp_data = {}

def make_main_menu():
    markup = types.InlineKeyboardMarkup()
    btn1 = types.InlineKeyboardButton('Вход', callback_data='log_in')
    markup.row(btn1)
    btn2 = types.InlineKeyboardButton('Регистрация', callback_data='registration')
    markup.row(btn2)
    return markup

def make_control_panel():
    markup = types.InlineKeyboardMarkup()
    btn1 = types.InlineKeyboardButton('Одиночное действие', callback_data='single_action')
    markup.row(btn1)
    btn2 = types.InlineKeyboardButton('Настройка сценариев', callback_data='scenarios')
    markup.row(btn2)
    btn3 = types.InlineKeyboardButton('Охранная функция', callback_data='security')
    markup.row(btn3)
    return markup

def make_functional_panel(chosen_device):
    markup = types.InlineKeyboardMarkup()
    for action in chosen_device['actions']['modes']:
        btn = types.InlineKeyboardButton(action, callback_data=str(chosen_device['id'])+':'+action)
        markup.row(btn)
    return markup

def get_username_and_password(callback):
    users_states[callback.message.chat.id] = states.WAITING_USERS_NAME
    bot.send_message(callback.message.chat.id, 'Введите логин:')
    while True:
        time.sleep(0.5)
        if users_states[callback.message.chat.id] == states.WAITING_USERS_PASSWORD:
            break
    username = temp_data[callback.message.chat.id]['username']
    bot.send_message(callback.message.chat.id, 'Введите пароль:')
    while True:
        time.sleep(0.5)
        if users_states[callback.message.chat.id] == states.WAITING_AUTHENTIFICATION_RESPONSE:
            break
    password = temp_data[callback.message.chat.id]['password']
    return username, password

def make_device_table_from_json(json_data):
    s = "```\n"
    max_len_device_name = len('Устройство')
    max_len_device_type = len('Тип')
    for device in json_data:
        max_len_device_name = max(max_len_device_name, len(device['name']))
        max_len_device_type = max(max_len_device_type, len(device['type']))
    s += '-' * (10 + max_len_device_name + max_len_device_type + len(json_data)) + '\n'
    s += '| ' + '№' + ' ' * len(json_data) + '| ' + 'Устройство' + ' ' * (max_len_device_name - len('Устройство') + 1) + '| ' + \
                'Тип' + ' ' * (max_len_device_type - len('Тип') + 1) + '|\n'
    i = 1
    for device in json_data:
        s += '-' * (10 + max_len_device_name + max_len_device_type + len(json_data)) + '\n'
        s += '| ' + str(i) + ' ' * len(json_data) + '| ' + device['name'] + \
             ' ' * (max_len_device_name - len(device['name']) + 1) \
           + '| ' + device['type'] + ' ' * (max_len_device_type - len(device['type']) + 1) + '|\n' 
        i += 1
    s += '-' * (10 + max_len_device_name + max_len_device_type + len(json_data)) + '\n'
    return s + "```\n"

def check_is_device_action_callback(callback):
    # ПОКА КОСТЫЛЬ С ':', НО НУЖНО БУДЕТ ПОЛНОСТЬЮ ПРОВЕРЯТЬ ЧТО ФОРМАТ СТРОКИ 'id:action'
    # ИЛИ ЧЕТКО В ДОКУМЕНТАЦИИ ОПРЕДЕЛИТЬ, ЧТО ТАКОЙ ФОРМАТ CALLBACKов ТОЛЬКО У ДЕЙСТВИЙ DEVICов
    return ':' in callback.data

def parse_device_action(command):
    separator_index = command.find(':')
    return int(command[:separator_index]), command[separator_index + 1:]


@bot.message_handler(commands=['start'])
def start(message):
    bot.send_message(message.chat.id, 'Привет!', reply_markup=make_main_menu())

@bot.callback_query_handler(func=lambda callback: True)
def callback_message(callback):
    if callback.data == 'log_in':
        username, password = get_username_and_password(callback)
        url = BASE_API_URL + "/api/users/auth"
        auth_data = {'username' : username, 'password' : password}
        response = requests.post(url, json=auth_data)
        data = response.json()
        bot.send_message(callback.message.chat.id, data["message"])
        if data["status"]:
            users_states[callback.message.chat.id] = states.AUTHORIZED
            bot.send_message(callback.message.chat.id, 'Панель управления умным домом', \
                             reply_markup=make_control_panel())
    elif callback.data == 'registration':
        username, password = get_username_and_password(callback)
        url = BASE_API_URL + "/api/users/registration"
        registration_data = {'username' : username, 'password' : password, \
                             'tg_chat_id' : callback.message.chat.id}
        response = requests.post(url, json=registration_data)
        data = response.json()
        bot.send_message(callback.message.chat.id, data["message"])
    elif callback.data == 'single_action':
        url = BASE_API_URL + "/api/devices/actuators"
        response = requests.get(url)
        if response.status_code == 200:
            bot.send_message(callback.message.chat.id, 'Список устройств для взаимодействия:')
            bot.send_message(callback.message.chat.id, make_device_table_from_json(response.json()), parse_mode='MarkdownV2')
            bot.send_message(callback.message.chat.id, 'Выберите устройство')
            users_states[callback.message.chat.id] = states.WAITING_ACTUATOR_DEVICE_ID
            while True:
                time.sleep(0.5)
                if users_states[callback.message.chat.id] == states.ACTUATOR_DEVICE_ID_RECIEVED:
                    break
            actuator_device_id = temp_data[callback.message.chat.id]['actuator_device_id']
            actuator_device_id -= 1
            chosen_device = response.json()[actuator_device_id]
            markup = make_functional_panel(chosen_device)
            bot.send_message(callback.message.chat.id, 'Выбранное Вами устройство: ' + chosen_device['name'], \
                             reply_markup=markup)
        else:
            bot.send_message(callback.message.chat.id, 'Технические неполадки :(')
    elif check_is_device_action_callback(callback):
        url = BASE_API_URL + '/api/actions/single'
        id, action = parse_device_action(callback.data)
        request_data = {'id' : id, 'action' : action}
        response = requests.post(url, json=request_data)
        data = response.json()
        if data['status']:
            bot.send_message(callback.message.chat.id, data['message'] + '\n' + data['mqtt_topic'])

@bot.message_handler(content_types=['text'])
def handle_text(message):
    if users_states[message.chat.id] == states.WAITING_USERS_NAME:
        users_states[message.chat.id] = states.WAITING_USERS_PASSWORD
        temp_data[message.chat.id] = {'username' : message.text}
    elif users_states[message.chat.id] == states.WAITING_USERS_PASSWORD:
        users_states[message.chat.id] = states.WAITING_AUTHENTIFICATION_RESPONSE
        temp_data[message.chat.id] = {'password' : message.text}
    elif users_states[message.chat.id] == states.WAITING_ACTUATOR_DEVICE_ID:
        # сделать проверку на дурака (возможно пользователь ввведет не число)
        users_states[message.chat.id] = states.ACTUATOR_DEVICE_ID_RECIEVED
        temp_data[message.chat.id] = {'actuator_device_id' : int(message.text)}

bot.polling(none_stop=True)