import telebot
import asyncio
import requests
from datetime import date
from telebot import types
from telebot.types import ReplyKeyboardMarkup, ReplyKeyboardRemove
import os
import json
from dotenv import load_dotenv

# Загружаем .env
load_dotenv()

class states:
    WAITING_USER_NAME_AND_PASSWORD = 0


bot = telebot.TeleBot(os.getenv("TELEGRAM_BOT_TOKEN", ""))
BASE_API_URL = os.getenv("API_BASE_URL", "")

is_user_authorized = False

def make_main_menu():
    markup = types.InlineKeyboardMarkup()
    btn1 = types.InlineKeyboardButton('Вход', callback_data='log_in')
    markup.row(btn1)
    btn2 = types.InlineKeyboardButton('Регистрация', callback_data='registration')
    markup.row(btn2)
    return markup

@bot.message_handler(commands=['start'])
def start(message):
    bot.send_message(message.chat.id, 'Привет!', reply_markup=make_main_menu())

@bot.callback_query_handler(func=lambda callback: True)
def callback_message(callback):
    if callback.data == 'device_list':
        url = BASE_API_URL + "/api/devices"
        response = requests.get(url)
        if response.status_code == 200:
            data = response.json()
            list_of_devices = ""
            for device in data:
                list_of_devices += device['device_name'] + ' - ' + device['type'] + '\n'
            bot.send_message(callback.message.chat.id, list_of_devices)
        else:
            bot.send_message(callback.message.chat.id, 'Технические неполадки :(')
    elif callback.data == 'log_in':
        url = BASE_API_URL + "/api/users/auth"
        username = "user"
        password = "password"
        auth_data = {'username' : username, 'password' : password}
        response = requests.post(url, json=auth_data)
        data = response.json()
        if data["status"]:
            is_user_authorized = True
        bot.send_message(callback.message.chat.id, data["message"])
    elif callback.data == 'registration':
        print('registration')

bot.polling(none_stop=True)