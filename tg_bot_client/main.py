import telebot
import asyncio
import requests
from datetime import date
from telebot import types
from telebot.types import ReplyKeyboardMarkup, ReplyKeyboardRemove
import os
import json

bot = telebot.TeleBot('YOUR_TOKEN')

#def registration():


def make_main_menu():
    markup = types.InlineKeyboardMarkup()
    btn1 = types.InlineKeyboardButton('Список устройств', callback_data='device_list')
    markup.row(btn1)
    return markup

@bot.message_handler(commands=['start'])
def start(message):
    bot.send_message(message.chat.id, 'Привет!', reply_markup=make_main_menu())

@bot.callback_query_handler(func=lambda callback: True)
def callback_message(callback):
    if callback.data == 'device_list':
        url = 'YOUR_API_URL'
        response = requests.get(url)
        if response.status_code == 200:
            data = response.json()
            list_of_devices = ""
            for device in data:
                list_of_devices += device['device_name'] + ' - ' + device['type'] + '\n'
            bot.send_message(callback.message.chat.id, list_of_devices)
        else:
            bot.send_message(callback.message.chat.id, 'Технические неполадки :(')

bot.polling(none_stop=True)