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
from make_button_menus import *

# Загружаем .env
load_dotenv()

auth_messages = []

class states:
    WAITING_USERS_NAME = 0                  # Ожидание ввода имени пользователя \ 
                                            #                                     authentification
    WAITING_USERS_PASSWORD = 1              # Ожидание ввода пароля             /
    WAITING_AUTHENTIFICATION_RESPONSE = 2   # Ожидание результата аутентификации
    AUTHORIZED = 3                          # Пользователь авторизован
    WAITING_SERVERS_NAME = 4                # Ожидание ввода имени сервера
    SERVER_NAME_RECIEVED = 5                # Название сервера получено
    WAITING_SERVERS_ID = 6                  # Ожидание id сервера из списка для редактирования/удаления
    SERVERS_ID_RECIEVED = 7                 # id сервера получен
    WAITING_MODULES_ID = 8                  # Ожидание id модуля
    MODULES_ID_RECIEVED = 9                 # id модуля получен


bot = telebot.TeleBot(os.getenv("TELEGRAM_BOT_TOKEN", ""))
BASE_API_URL = os.getenv("API_BASE_URL", "")

# словарь состояний пользователей
users_states = {}
# временные данные
temp_data = {}


current_server_id = -1

####################################ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ####################################
def get_username_and_password(callback):
    users_states[callback.message.chat.id] = states.WAITING_USERS_NAME
    auth_messages.append(bot.send_message(callback.message.chat.id, 'Введите логин:').message_id)
    while True:
        time.sleep(0.5)
        if users_states[callback.message.chat.id] == states.WAITING_USERS_PASSWORD:
            break
    username = temp_data[callback.message.chat.id]['username']
    auth_messages.append(bot.send_message(callback.message.chat.id, 'Введите пароль:').message_id)
    while True:
        time.sleep(0.5)
        if users_states[callback.message.chat.id] == states.WAITING_AUTHENTIFICATION_RESPONSE:
            break
    password = temp_data[callback.message.chat.id]['password']
    return username, password

def check_is_device_action_callback(callback):
    # ПОКА КОСТЫЛЬ С ':', НО НУЖНО БУДЕТ ПОЛНОСТЬЮ ПРОВЕРЯТЬ ЧТО ФОРМАТ СТРОКИ 'id:action'
    # ИЛИ ЧЕТКО В ДОКУМЕНТАЦИИ ОПРЕДЕЛИТЬ, ЧТО ТАКОЙ ФОРМАТ CALLBACKов ТОЛЬКО У ДЕЙСТВИЙ DEVICов
    return ':' in callback.data

def check_is_server_callback(callback):
    return 'server:' in callback.data

def parse_device_action(command):
    separator_index = command.find(':')
    return int(command[:separator_index]), command[separator_index + 1:]
###############################################################################################



####################################ОБРАБОТКА СОБЫТИЙ ОТ БОТА##################################
@bot.message_handler(commands=['start'])
def start(message):
    auth_messages.append(bot.send_message(message.chat.id, 'Привет!', reply_markup=make_auth()).message_id)

@bot.callback_query_handler(func=lambda callback: True)
def callback_message(callback):
    
    if callback.data == 'log_in':
        username, password = get_username_and_password(callback)
        url = BASE_API_URL + "/api/users/auth"
        auth_data = {'username' : username, 'password' : password}
        response = requests.post(url, json=auth_data)
        data = response.json()
        bot.delete_message(callback.message.chat.id, auth_messages[0] - 1)
        bot.delete_messages(callback.message.chat.id, auth_messages)
        if data["status"]:
            users_states[callback.message.chat.id] = states.AUTHORIZED
            url = BASE_API_URL + "/api/servers"
            user_data = {'tg_chat_id' : callback.message.chat.id}
            response = requests.get(url, json=user_data)
            list_of_servers = response.json()
            bot.send_message(callback.message.chat.id, 'Доступные сервера:', \
                             reply_markup=make_server_menu(list_of_servers))
        else:
            bot.send_message(callback.message.chat.id, data["message"], reply_markup=make_auth())
    elif callback.data == 'registration':
        username, password = get_username_and_password(callback)
        url = BASE_API_URL + "/api/users/registration"
        registration_data = {'username' : username, 'password' : password, \
                             'tg_chat_id' : callback.message.chat.id}
        response = requests.post(url, json=registration_data)
        data = response.json()
        bot.send_message(callback.message.chat.id, data["message"])
    elif callback.data == 'servers_actions':
        bot.send_message(callback.message.chat.id, 'Выберите действие:', reply_markup=make_servers_action_menu())
    elif callback.data == 'servers_add':
        bot.send_message(callback.message.chat.id, 'Введите имя сервера')
        users_states[callback.message.chat.id] = states.WAITING_SERVERS_NAME
        while True:
            time.sleep(0.5)
            if users_states[callback.message.chat.id] == states.SERVER_NAME_RECIEVED:
                break
        server_name = temp_data[callback.message.chat.id]['server_name']
        url = BASE_API_URL + '/api/servers/add'
        response = requests.post(url, json={'tg_chat_id' : callback.message.chat.id, \
                                            'server_name' : server_name})
        data = response.json()
        bot.send_message(callback.message.chat.id, data['message'])
    elif callback.data == 'servers_edit':
        url = BASE_API_URL + "/api/servers"
        user_data = {'tg_chat_id' : callback.message.chat.id}
        response = requests.get(url, json=user_data)
        list_of_servers = response.json()
        servers = ''
        if len(list_of_servers) > 0:
            i = 1
            for server in list_of_servers:
                servers += str(i) + '. ' + server['name'] + '\n'
                i += 1
            bot.send_message(callback.message.chat.id, servers)
            bot.send_message(callback.message.chat.id, 'Введите № сервера для редактирования')
            users_states[callback.message.chat.id] = states.WAITING_SERVERS_ID
            while True:
                time.sleep(0.5)
                if users_states[callback.message.chat.id] == states.SERVERS_ID_RECIEVED:
                    break
            server_id = temp_data[callback.message.chat.id]['server_id']
            bot.send_message(callback.message.chat.id, 'Введите новое имя сервера')
            users_states[callback.message.chat.id] = states.WAITING_SERVERS_NAME
            while True:
                time.sleep(0.5)
                if users_states[callback.message.chat.id] == states.SERVER_NAME_RECIEVED:
                    break
            new_server_name = temp_data[callback.message.chat.id]['server_name']
            url = BASE_API_URL + '/api/servers/edit'
            update_data = {'server_id' : list_of_servers[server_id]['serverID'], \
                           'new_server_name' : new_server_name}
            response = requests.patch(url, json=update_data)
            bot.send_message(callback.message.chat.id, response.json()['message'])
        else:
            bot.send_message(callback.message.chat.id, 'У вас пока нет ни одного созданного сервера')
    elif callback.data == 'servers_delete':
        url = BASE_API_URL + "/api/servers"
        user_data = {'tg_chat_id' : callback.message.chat.id}
        response = requests.get(url, json=user_data)
        list_of_servers = response.json()
        servers = ''
        if len(list_of_servers) > 0:
            i = 1
            for server in list_of_servers:
                servers += str(i) + '. ' + server['name'] + '\n'
                i += 1
            bot.send_message(callback.message.chat.id, servers)
            bot.send_message(callback.message.chat.id, 'Введите № сервера для удаления')
            users_states[callback.message.chat.id] = states.WAITING_SERVERS_ID
            while True:
                time.sleep(0.5)
                if users_states[callback.message.chat.id] == states.SERVERS_ID_RECIEVED:
                    break
            server_id = temp_data[callback.message.chat.id]['server_id']
            url = BASE_API_URL + '/api/servers/delete'
            update_data = {'server_id' : list_of_servers[server_id]['serverID']}
            response = requests.delete(url, json=update_data)
            bot.send_message(callback.message.chat.id, response.json()['message'])
        else:
            bot.send_message(callback.message.chat.id, 'У вас пока нет ни одного созданного сервера')    
    elif 'server:' in callback.data:
        str_without_tag = callback.data[((callback.data.find(':')) + 1):]
        current_server_name = str_without_tag[:(str_without_tag.find(':'))]
        current_server_id = int(str_without_tag[(str_without_tag.find(':')) + 1:])
        url = BASE_API_URL + '/api/modules'
        response = requests.get(url, json={'server_id' : current_server_id})
        bot.send_message(callback.message.chat.id, \
                         f"Выбран сервер: {current_server_name}", \
                         reply_markup=make_modules_menu(response.json()))
        temp_data[callback.message.chat.id]['server_id'] = current_server_id
    elif callback.data == 'modules_actions':
        bot.send_message(callback.message.chat.id, 'Выберите действие:', reply_markup=make_modules_action_menu())
    elif callback.data == 'modules_add':
        
        # ПРИ ДОБАВЛЕНИИ МОДУЛЯ СРАЗУ ДОБАВЛЯТЬ НЕОБХОДИМЫЕ УСТРОЙСТВА
        # Идет опрос какого типа устройство пользователь хочет добавить
        # (relay, MK, camera)
        # Добавляется запись в твблицу devices и выдается клиенту MQTT topic
        # Также добавляется запись в таблицу modules_devices

        cur_server_id = temp_data[callback.message.chat.id]['server_id']
        url = BASE_API_URL + '/api/modules/all'
        response = requests.get(url)
        list_of_modules = response.json()
        modules = ''
        if len(list_of_modules) > 0:
            i = 1
            for module in list_of_modules:
                modules += str(i) + '. ' + module['name'] + '\n'
                i += 1
            bot.send_message(callback.message.chat.id, modules)
            bot.send_message(callback.message.chat.id, 'Выберите № модуля')
            users_states[callback.message.chat.id] = states.WAITING_MODULES_ID
            while True:
                time.sleep(0.5)
                if users_states[callback.message.chat.id] == states.MODULES_ID_RECIEVED:
                    break
            module_id = list_of_modules[temp_data[callback.message.chat.id]['module_id']]['id']
            
            ################ДОДЕЛАТЬ ДОБАВЛЕНИЕ УСТРОЙСТВ ПО СПИСКУ НЕОБХОДИМЫХ КОМПЛЕКТУЮЩИХ###################
            # url = BASE_API_URL + '/api/modules/necessary_devices'
            # module_data = {"module_id" : module_id}
            # response = requests.get(url, json=module_data)
            # print(response.json())
            # bot.send_message(callback.message.chat.id, 'Перед началом использования модуля \
            #                                             необходимо добавить следующие устройства:', \
            #                 reply_markup=make_modules_necessary_devices(response.json()))
            ####################################################################################################

            url = BASE_API_URL + '/api/modules/add'
            module_data = {'server_id' : cur_server_id, 'module_id' : module_id}
            response = requests.post(url, json=module_data)
            data = response.json()
            bot.send_message(callback.message.chat.id, data['message'])
        else:
            bot.send_message(callback.message.chat.id, 'Еще не добавлено ни одного модуля')
    elif callback.data == 'modules_delete':
        cur_server_id = temp_data[callback.message.chat.id]['server_id']
        url = BASE_API_URL + '/api/modules'
        server_data = {'server_id' : temp_data[callback.message.chat.id]['server_id']}
        response = requests.get(url, json=server_data)
        list_of_modules = response.json()
        modules = ''
        if len(list_of_modules) > 0:
            i = 1
            for module in list_of_modules:
                modules += str(i) + '. ' + module['name'] + '\n'
                i += 1
            bot.send_message(callback.message.chat.id, modules)
            bot.send_message(callback.message.chat.id, 'Выберите № модуля')
            users_states[callback.message.chat.id] = states.WAITING_MODULES_ID
            while True:
                time.sleep(0.5)
                if users_states[callback.message.chat.id] == states.MODULES_ID_RECIEVED:
                    break
            record_id = list_of_modules[temp_data[callback.message.chat.id]['module_id']]['record_id']
            url = BASE_API_URL + '/api/modules/delete'
            response = requests.delete(url, json={'record_id' : record_id})
            data = response.json()
            bot.send_message(callback.message.chat.id, data['message'])
        else:
            bot.send_message(callback.message.chat.id, 'Еще не добавлено ни одного модуля')
    elif 'module:' in callback.data:
        str_without_tag = callback.data[((callback.data.find(':')) + 1):]
        current_module_name = str_without_tag[:(str_without_tag.find(':'))]
        current_record_id = int(str_without_tag[(str_without_tag.find(':')) + 1:])
        bot.send_message(callback.message.chat.id, \
                         f"Выбран модуль: {current_module_name}\n")
        url = BASE_API_URL + '/api/modules/capabilities'
        response = requests.get(url, json={'record_id' : current_record_id})
        capabilities = response.json()
        bot.send_message(callback.message.chat.id, 'Функционал:', reply_markup=make_modules_capabilities(current_record_id, capabilities))
    elif 'action:' in callback.data:
        print()

    
    # elif callback.data == 'single_action':
    #     url = BASE_API_URL + "/api/devices/actuators"
    #     response = requests.get(url)
    #     if response.status_code == 200:
    #         bot.send_message(callback.message.chat.id, 'Список устройств для взаимодействия:')
    #         bot.send_message(callback.message.chat.id, make_device_table_from_json(response.json()), parse_mode='MarkdownV2')
    #         bot.send_message(callback.message.chat.id, 'Выберите устройство')
    #         users_states[callback.message.chat.id] = states.WAITING_ACTUATOR_DEVICE_ID
    #         while True:
    #             time.sleep(0.5)
    #             if users_states[callback.message.chat.id] == states.ACTUATOR_DEVICE_ID_RECIEVED:
    #                 break
    #         actuator_device_id = temp_data[callback.message.chat.id]['actuator_device_id']
    #         actuator_device_id -= 1
    #         chosen_device = response.json()[actuator_device_id]
    #         markup = make_functional_panel(chosen_device)
    #         bot.send_message(callback.message.chat.id, 'Выбранное Вами устройство: ' + chosen_device['name'], \
    #                          reply_markup=markup)
    #     else:
    #         bot.send_message(callback.message.chat.id, 'Технические неполадки :(')
    # elif check_is_device_action_callback(callback):
    #     url = BASE_API_URL + '/api/actions/single'
    #     id, action = parse_device_action(callback.data)
    #     request_data = {'id' : id, 'action' : action}
    #     response = requests.post(url, json=request_data)
    #     data = response.json()
    #     if data['status']:
    #         bot.send_message(callback.message.chat.id, data['message'] + '\n' + data['mqtt_topic'])

@bot.message_handler(content_types=['text'])
def handle_text(message):
    if users_states[message.chat.id] == states.WAITING_USERS_NAME:
        auth_messages.append(message.message_id)
        users_states[message.chat.id] = states.WAITING_USERS_PASSWORD
        temp_data[message.chat.id] = {'username' : message.text}
    elif users_states[message.chat.id] == states.WAITING_USERS_PASSWORD:
        auth_messages.append(message.message_id)
        users_states[message.chat.id] = states.WAITING_AUTHENTIFICATION_RESPONSE
        temp_data[message.chat.id] = {'password' : message.text}
    elif users_states[message.chat.id] == states.WAITING_SERVERS_NAME:
        users_states[message.chat.id] = states.SERVER_NAME_RECIEVED
        temp_data[message.chat.id] = {'server_name' : message.text}
    elif users_states[message.chat.id] == states.WAITING_SERVERS_ID:
        users_states[message.chat.id] = states.SERVERS_ID_RECIEVED
        temp_data[message.chat.id] = {'server_id' : int(message.text) - 1}
    elif users_states[message.chat.id] == states.WAITING_MODULES_ID:
        users_states[message.chat.id] = states.MODULES_ID_RECIEVED
        temp_data[message.chat.id] = {'module_id' : int(message.text) - 1}

bot.polling(none_stop=True)
###############################################################################################