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
    WAITING_ALIAS_FOR_MODULE = 10           # Ожидание псевднонима для модуля
    ALIAS_FOR_MODULE_RECIEVED = 11          # псевдоним для модуля получен
    WAITING_ALIAS_FOR_DEVICE = 12           # Ожидание псевднонима для устройства
    ALIAS_FOR_DEVICE_RECIEVED = 13          # псевдоним для устройства получен

bot = telebot.TeleBot(os.getenv("TELEGRAM_BOT_TOKEN", ""))
BASE_API_URL = os.getenv("API_BASE_URL", "")

# словарь состояний пользователей
users_states = {}
# временные данные
temp_data = {}


current_server_id = -1

#################################### ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ##################################
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



#################################### ОБРАБОТКА СОБЫТИЙ ОТ БОТА ################################
@bot.message_handler(commands=['start'])
def start(message):
    # This is a stub for now. In the future, we will need to send a request to the c++ client server 
    # to get a list of servers 
    servers_list = [{'name' : 'Home', 'server_id' : 1}, {'name' : 'Dacha', 'server_id' : 2}]
    bot.send_message(message.chat.id, 'Привет! Вот доступные сервера:', reply_markup=make_servers_menu(servers_list))
    temp_data[message.chat.id] = {}

@bot.callback_query_handler(func=lambda callback: True)
def callback_message(callback):
    
    ########################## ВЫВОД ВОЗМОЖНЫХ ОПЕРАЦИЙ С СЕРВЕРОМ ############################
    if callback.data == 'servers_actions':
        bot.send_message(callback.message.chat.id, 'Выберите действие:', reply_markup=make_servers_action_menu())
    ###########################################################################################
    

    ################################### ДОБАВЛЕНИЕ СЕРВЕРА ####################################
    elif callback.data == 'servers_add':
        print()
        # # The token generator needs to be implemented later.
        # token = '9sd12nj9fds9nj14n25df6qsa'
        # bot.send_message(callback.message.chat.id, f'Токен для идентификации вашего сервера: {token}')
        # # Waiting for connection request from rpi...
        # cur_time = time.time()
        # while(not temp_data[callback.message.chat.id]['initial_request']['status']):
        #     if time.time() - cur_time > 2:
        #         temp_data[callback.message.chat.id]['initial_request']['token'] = token
        #         break
        # # Extract the token from the request
        # received_token = temp_data[callback.message.chat.id]['initial_request']['token']
        # if received_token == token:
        #     # Establishing a connection
        #     bot.send_message(callback.message.chat.id, 'Соединение установлено!')
        # else:
        #     bot.send_message(callback.message.chat.id, 'Соединение не удалось установить')
        # # Sending the operation status to the RPI
    ###########################################################################################


    ################################### ИЗМЕНЕНИЕ СЕРВЕРА #####################################
    elif callback.data == 'servers_edit':
        print()
        # list_of_servers = [{'name' : 'Home', 'server_id' : 1}, {'name' : 'Dacha', 'server_id' : 2}]
        # servers = ''
        # if len(list_of_servers) > 0:
        #     i = 1
        #     for server in list_of_servers:
        #         servers += str(i) + '. ' + server['name'] + '\n'
        #         i += 1
        #     bot.send_message(callback.message.chat.id, servers)
        #     bot.send_message(callback.message.chat.id, 'Введите № сервера для редактирования')
        #     users_states[callback.message.chat.id] = states.WAITING_SERVERS_ID
        #     while True:
        #         time.sleep(0.5)
        #         if users_states[callback.message.chat.id] == states.SERVERS_ID_RECIEVED:
        #             break
        #     server_id = temp_data[callback.message.chat.id]['server_id']
        #     bot.send_message(callback.message.chat.id, 'Введите новое имя сервера')
        #     users_states[callback.message.chat.id] = states.WAITING_SERVERS_NAME
        #     while True:
        #         time.sleep(0.5)
        #         if users_states[callback.message.chat.id] == states.SERVER_NAME_RECIEVED:
        #             break
        #     new_server_name = temp_data[callback.message.chat.id]['server_name']
        #     url = BASE_API_URL + f'/api/users/{temp_data[callback.message.chat.id]['user_id']}/' + \
        #                          f'servers/{list_of_servers[server_id]['server_id']}/edit'
        #     update_data = {'new_server_name' : new_server_name}
        #     response = requests.patch(url, json=update_data)
        #     bot.send_message(callback.message.chat.id, response.json()['message'])
        # else:
        #     bot.send_message(callback.message.chat.id, 'У вас пока нет ни одного созданного сервера')
    ###########################################################################################
    

    ################################### УДАЛЕНИЕ СЕРВЕРА #####################################
    elif callback.data == 'servers_delete':
        print()
        # url = BASE_API_URL + f"/api/users/{temp_data[callback.message.chat.id]['user_id']}/servers"
        # response = requests.get(url)
        # list_of_servers = response.json()
        # servers = ''
        # if len(list_of_servers) > 0:
        #     i = 1
        #     for server in list_of_servers:
        #         servers += str(i) + '. ' + server['name'] + '\n'
        #         i += 1
        #     bot.send_message(callback.message.chat.id, servers)
        #     bot.send_message(callback.message.chat.id, 'Введите № сервера для удаления')
        #     users_states[callback.message.chat.id] = states.WAITING_SERVERS_ID
        #     while True:
        #         time.sleep(0.5)
        #         if users_states[callback.message.chat.id] == states.SERVERS_ID_RECIEVED:
        #             break
        #     server_id = list_of_servers[temp_data[callback.message.chat.id]['server_id']]['server_id']
        #     url = BASE_API_URL + f'/api/users/{temp_data[callback.message.chat.id]['user_id']}/' + \
        #                          f'servers/{server_id}/delete'
        #     response = requests.delete(url)
        #     bot.send_message(callback.message.chat.id, response.json()['message'])
        # else:
        #     bot.send_message(callback.message.chat.id, 'У вас пока нет ни одного созданного сервера')    
    ###########################################################################################


    ##################################### ГЛАВНОЕ МЕНЮ ########################################
    elif 'server:' in callback.data:
        str_without_tag = callback.data[((callback.data.find(':')) + 1):]
        current_server_name = str_without_tag[:(str_without_tag.find(':'))]
        current_server_id = int(str_without_tag[(str_without_tag.find(':')) + 1:])
        temp_data[callback.message.chat.id]['current_server_id'] = current_server_id
        modules = [{'name' : 'Smart_plug', 'alis' : 'kitchen', 'id' : 1}, 
                   {'name' : 'Climate control', 'alias' : 'sleeping room', 'id' : 2}]
        bot.send_message(callback.message.chat.id, \
                         "Модули:", \
                         reply_markup=make_modules_menu(modules))
        
    ###########################################################################################
    

    ############################## ВЫВОД СПИСКА ДОСТУПНЫХ МОДУЛЕЙ #############################
    # elif callback.data == 'modules':
    #     url = BASE_API_URL + f'/api/users/{temp_data[callback.message.chat.id]['user_id']}/' + \
    #                          f'servers/{temp_data[callback.message.chat.id]['current_server_id']}/' + \
    #                           'modules'
    #     response = requests.get(url)
    #     bot.send_message(callback.message.chat.id, 
    #                      "Доступные модули:",
    #                      reply_markup=make_modules_menu(response.json()))
    ###########################################################################################


    ############################ ВЫВОД СПИСКА ДОСТУПНЫХ СЦЕНАРИЕВ #############################
    elif callback.data == 'scenarios':
        print('')
        # url = BASE_API_URL + '/api/modules'
        # response = requests.get(url, json={'server_id' : temp_data[callback.message.chat.id]['current_server_id']})
        # bot.send_message(callback.message.chat.id, 
        #                  "Доступные модули:",
        #                  reply_markup=make_modules_menu(response.json()))
    ###########################################################################################


    ########################## ВЫВОД ВОЗМОЖНЫХ ОПЕРАЦИЙ С СЕРВЕРОМ ############################
    elif callback.data == 'modules_actions':
        bot.send_message(callback.message.chat.id, 'Выберите действие:', reply_markup=make_modules_action_menu())
    ###########################################################################################


    ################################### ДОБАВЛЕНИЕ МОДУЛЯ #####################################
    elif callback.data == 'modules_add':

        print()
        
        # # ПРИ ДОБАВЛЕНИИ МОДУЛЯ СРАЗУ ДОБАВЛЯТЬ НЕОБХОДИМЫЕ УСТРОЙСТВА
        # # Идет опрос какого типа устройство пользователь хочет добавить
        # # (relay, MK, camera)
        # # Добавляется запись в твблицу devices и выдается клиенту MQTT topic
        # # Также добавляется запись в таблицу modules_devices

        # url = BASE_API_URL + f'/api/users/{temp_data[callback.message.chat.id]['user_id']}/' + \
        #                      f'servers/{temp_data[callback.message.chat.id]['current_server_id']}/' + \
        #                       'modules/types'
        # response = requests.get(url)
        # list_of_modules = response.json()
        # modules = ''
        # if len(list_of_modules) > 0:
        #     i = 1
        #     for module in list_of_modules:
        #         modules += str(i) + '. ' + module['name'] + '\n'
        #         i += 1
        #     bot.send_message(callback.message.chat.id, modules)
        #     bot.send_message(callback.message.chat.id, 'Выберите № модуля')
        #     users_states[callback.message.chat.id] = states.WAITING_MODULES_ID
        #     while True:
        #         time.sleep(0.5)
        #         if users_states[callback.message.chat.id] == states.MODULES_ID_RECIEVED:
        #             break
        #     module_type_id = list_of_modules[temp_data[callback.message.chat.id]['module_id']]['id']

        #     bot.send_message(callback.message.chat.id, 'Введите псевдоним для модуля')
        #     users_states[callback.message.chat.id] = states.WAITING_ALIAS_FOR_MODULE
        #     while True:
        #         time.sleep(0.5)
        #         if users_states[callback.message.chat.id] == states.ALIAS_FOR_MODULE_RECIEVED:
        #             break
        #     alias = temp_data[callback.message.chat.id]['alias']
        #     url = BASE_API_URL + f'/api/users/{temp_data[callback.message.chat.id]['user_id']}/' + \
        #                          f'servers/{temp_data[callback.message.chat.id]['current_server_id']}/' + \
        #                           'modules/add'
        #     module_data = {'module_type_id' : module_type_id, 'alias' : alias}
        #     response = requests.post(url, json=module_data)
        #     data = response.json()
        #     cur_module_id = data['module_id']
        #     print(cur_module_id)
        #     bot.send_message(callback.message.chat.id, data['message'])
        #     url = BASE_API_URL + f'/api/users/{temp_data[callback.message.chat.id]['user_id']}/' + \
        #                          f'servers/{temp_data[callback.message.chat.id]['current_server_id']}/' + \
        #                          f'modules/types/{module_type_id}/necessary_devices'
        #     response = requests.get(url)
        #     necessary_devices = response.json()
        #     for device in necessary_devices:
        #         for i in range(device['count']):
        #             device_alias = ''
        #             if device['count'] > 1:
        #                 bot.send_message(callback.message.chat.id, \
        #                                 f'Введите псевдоним для устройств типа {device['device_type_id']}')
        #                 users_states[callback.message.chat.id] = states.WAITING_ALIAS_FOR_DEVICE
        #                 while True:
        #                     time.sleep(0.5)
        #                     if users_states[callback.message.chat.id] == states.ALIAS_FOR_DEVICE_RECIEVED:
        #                         break
        #                 device_alias = temp_data[callback.message.chat.id]['device_alias']
        #             url = BASE_API_URL + f'/api/users/{temp_data[callback.message.chat.id]['user_id']}/' + \
        #                                  f'servers/{temp_data[callback.message.chat.id]['current_server_id']}/' + \
        #                                  f'modules/{cur_module_id}/' + \
        #                                   'add_devices'
        #             response = requests.post(url, json={'device_type_id': device['device_type_id'],\
        #                                                 'alias': device_alias})
                    
        # else:
        #     bot.send_message(callback.message.chat.id, 'Еще не добавлено ни одного модуля')
    ###########################################################################################
    
    
    ################################### УДАЛЕНИЕ МОДУЛЯ #######################################
    elif callback.data == 'modules_delete':

        print()

        # url = BASE_API_URL + f'/api/users/{temp_data[callback.message.chat.id]['user_id']}/' + \
        #                      f'servers/{temp_data[callback.message.chat.id]['current_server_id']}/' + \
        #                       'modules'
        # response = requests.get(url)
        # list_of_modules = response.json()
        # modules = ''
        # if len(list_of_modules) > 0:
        #     i = 1
        #     for module in list_of_modules:
        #         modules += str(i) + '. ' + module['name'] + '\n'
        #         i += 1
        #     bot.send_message(callback.message.chat.id, modules)
        #     bot.send_message(callback.message.chat.id, 'Выберите № модуля')
        #     users_states[callback.message.chat.id] = states.WAITING_MODULES_ID
        #     while True:
        #         time.sleep(0.5)
        #         if users_states[callback.message.chat.id] == states.MODULES_ID_RECIEVED:
        #             break
        #     module_id = list_of_modules[temp_data[callback.message.chat.id]['module_id']]['id']
        #     url = BASE_API_URL + f'/api/users/{temp_data[callback.message.chat.id]['user_id']}/' + \
        #                          f'servers/{temp_data[callback.message.chat.id]['current_server_id']}/' + \
        #                          f'modules/{module_id}/delete'
        #     response = requests.delete(url)
        #     data = response.json()
        #     bot.send_message(callback.message.chat.id, data['message'])
        # else:
        #     bot.send_message(callback.message.chat.id, 'Еще не добавлено ни одного модуля')
    ###########################################################################################
    

    ############################### ВЫВОД ФУНКЦИОНАЛА МОДУЛЯ ##################################
    elif 'module:' in callback.data:
        str_without_tag = callback.data[((callback.data.find(':')) + 1):]
        current_module_name = str_without_tag[:(str_without_tag.find(':'))]
        current_module_id = int(str_without_tag[(str_without_tag.find(':')) + 1:])
        bot.send_message(callback.message.chat.id, \
                         f"Выбран модуль: {current_module_name}\n")
        # url = BASE_API_URL + f'/api/users/{temp_data[callback.message.chat.id]['user_id']}/' + \
        #                      f'servers/{temp_data[callback.message.chat.id]['current_server_id']}/' + \
        #                      f'modules/{current_module_id}/capabilities'
        # response = requests.get(url)
        capabilities = [{'capability_name' : 'Показать текущее значение(я)', 'capability_id' : 1},
                        {'capability_name' : 'Вывести график за 24 часа', 'capability_id' : 2}]
        bot.send_message(callback.message.chat.id, 'Функционал:', reply_markup=make_modules_capabilities(current_module_id, capabilities))
        temp_data[callback.message.chat.id]['current_module_id'] = current_module_id
    ###########################################################################################
    
    elif 'capability:' in callback.data:
        str_without_tag = callback.data[((callback.data.find(':')) + 1):]
        current_capability_name = str_without_tag[:(str_without_tag.find(':'))]
        current_capability_id = int(str_without_tag[(str_without_tag.find(':')) + 1:])
        bot.send_message(callback.message.chat.id, \
                         f"Выбрана функция: {current_capability_name}\n")
        # url = BASE_API_URL + f'/api/users/{temp_data[callback.message.chat.id]['user_id']}/' + \
        #                      f'servers/{temp_data[callback.message.chat.id]['current_server_id']}/' + \
        #                      f'modules/{temp_data[callback.message.chat.id]['current_module_id']}/' + \
        #                      f'capabilities/{current_capability_id}'
        # response = requests.post(url)

    elif callback.data == 'back_to_the_servers_list':
        url = BASE_API_URL + f'/api/users/{temp_data[callback.message.chat.id]['user_id']}/servers'
        response = requests.get(url)
        bot.edit_message_text(
            chat_id=callback.message.chat.id,
            message_id=callback.message.message_id,
            text="Доступные сервера:",
            reply_markup=make_servers_menu(response.json())
        )
    # elif callback.data == 'back_to_the_main_menu':
    #     bot.edit_message_text(
    #         chat_id=callback.message.chat.id,
    #         message_id=callback.message.message_id,
    #         text="Главное меню:",
    #         reply_markup=make_main_menu()
    #     )
    elif callback.data == 'back_to_the_modules_list':
        url = BASE_API_URL + f'/api/users/{temp_data[callback.message.chat.id]['user_id']}/' + \
                             f'servers/{temp_data[callback.message.chat.id]['current_server_id']}/' + \
                              'modules'
        response = requests.get(url)
        bot.edit_message_text(
            chat_id=callback.message.chat.id,
            message_id=callback.message.message_id,
            text="Доступные модули:",
            reply_markup=make_modules_menu(response.json())
        )
        

@bot.message_handler(content_types=['text'])
def handle_text(message):
    if users_states[message.chat.id] == states.WAITING_USERS_NAME:
        users_states[message.chat.id] = states.WAITING_USERS_PASSWORD
        temp_data[message.chat.id]['username'] = message.text
    elif users_states[message.chat.id] == states.WAITING_USERS_PASSWORD:
        users_states[message.chat.id] = states.WAITING_AUTHENTIFICATION_RESPONSE
        temp_data[message.chat.id]['password'] = message.text
    elif users_states[message.chat.id] == states.WAITING_SERVERS_NAME:
        users_states[message.chat.id] = states.SERVER_NAME_RECIEVED
        temp_data[message.chat.id]['server_name'] = message.text
    elif users_states[message.chat.id] == states.WAITING_SERVERS_ID:
        users_states[message.chat.id] = states.SERVERS_ID_RECIEVED
        temp_data[message.chat.id]['server_id'] = int(message.text) - 1
    elif users_states[message.chat.id] == states.WAITING_MODULES_ID:
        users_states[message.chat.id] = states.MODULES_ID_RECIEVED
        temp_data[message.chat.id]['module_id'] = int(message.text) - 1
    elif users_states[message.chat.id] == states.WAITING_ALIAS_FOR_MODULE:
        users_states[message.chat.id] = states.ALIAS_FOR_MODULE_RECIEVED
        temp_data[message.chat.id]['alias'] = message.text
    elif users_states[message.chat.id] == states.WAITING_ALIAS_FOR_DEVICE:
        users_states[message.chat.id] = states.ALIAS_FOR_DEVICE_RECIEVED
        temp_data[message.chat.id]['device_alias'] = message.text

bot.polling(none_stop=True)
###############################################################################################