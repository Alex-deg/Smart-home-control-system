from telebot import types
from telebot.types import ReplyKeyboardMarkup, ReplyKeyboardRemove

#################################СОЗДАНИЕ КНОПОЧНЫХ МЕНЮ#######################################
def make_button_menu(list_of_titles, list_of_callbacks_names):
    markup = types.InlineKeyboardMarkup()
    if len(list_of_titles) != len(list_of_callbacks_names):
        raise RuntimeError("Размеры списков названий кнопок и callback'ов для них не совпадают")
    for i in range(list_of_titles):
        btn = types.InlineKeyboardButton(list_of_titles[i], callback_data=list_of_callbacks_names[i])
        markup.row(btn)
    return markup

def make_auth():
    return make_button_menu(['Вход', 'Регистрация'],\
                            ['log_in', 'registration'])
    # markup = types.InlineKeyboardMarkup()
    # btn1 = types.InlineKeyboardButton('Вход', callback_data='log_in')
    # markup.row(btn1)
    # btn2 = types.InlineKeyboardButton('Регистрация', callback_data='registration')
    # markup.row(btn2)
    # return markup

def make_server_menu(servers):

    names, callbacks = [], []
    for server in servers:
        names.append(server['name'])
        callbacks.append('server:' + server["name"] + ':' + str(server["serverID"]))

    return make_button_menu(names, callbacks)
    # markup = types.InlineKeyboardMarkup()
    # for server in servers:
    #     btn1 = types.InlineKeyboardButton(server["name"], callback_data='server:' +\
    #                                       server["name"] + ':' + str(server["serverID"]))
    #     markup.row(btn1)
    # btn2 = types.InlineKeyboardButton('Редактирование серверов', callback_data='servers_actions')
    # markup.row(btn2)
    # return markup

def make_servers_action_menu():
    return make_button_menu(['Добавить', 'Изменить', 'Удалить', 'Назад'],\
                            ['servers_add', 'servers_edit', 'servers_delete', 'back'])
    # markup = types.InlineKeyboardMarkup()
    # btn1 = types.InlineKeyboardButton('Добавить', callback_data='servers_add')
    # markup.row(btn1)
    # btn2 = types.InlineKeyboardButton('Изменить', callback_data='servers_edit')
    # markup.row(btn2)
    # btn3 = types.InlineKeyboardButton('Удалить', callback_data='servers_delete')
    # markup.row(btn3)
    # btn4 = types.InlineKeyboardButton('Назад', callback_data='back')
    # markup.row(btn4)
    # return markup

def make_main_menu():
    return make_button_menu(['Модули', 'Сценарии', 'Назад'], \
                            ['modules', 'scenarios', 'back'])
    # markup = types.InlineKeyboardMarkup()
    # btn1 = types.InlineKeyboardButton('Модули', callback_data='modules')
    # markup.row(btn1)
    # btn2 = types.InlineKeyboardButton('Сценарии', callback_data='scenarios')
    # markup.row(btn2)
    # btn3 = types.InlineKeyboardButton('Назад', callback_data='back')
    # markup.row(btn3)
    # return markup

def make_modules_menu(modules):

    names, callbacks = [], []
    for module in modules:
        names.append(module['name'])
        callbacks.append(callback_data='module:' + module['name'] + ':' + str(module["record_id"]))
    return make_button_menu(names, callbacks)

    # markup = types.InlineKeyboardMarkup()
    # for module in modules:
    #     btn1 = types.InlineKeyboardButton(module['name'], callback_data='module:' + \
    #                                       module['name'] + ':' + str(module["record_id"]))
    #     markup.row(btn1)
    # btn2 = types.InlineKeyboardButton('Редактирование модулей', callback_data='modules_actions')
    # markup.row(btn2)
    # btn3 = types.InlineKeyboardButton('Назад', callback_data='back')
    # markup.row(btn3)
    # return markup

def make_modules_action_menu():
    return make_button_menu(['Добавить', 'Удалить', 'Назад'],\
                            ['modules_add', 'modules_delete', 'back'])
    # markup = types.InlineKeyboardMarkup()
    # btn1 = types.InlineKeyboardButton('Добавить', callback_data='modules_add')
    # markup.row(btn1)
    # btn2 = types.InlineKeyboardButton('Удалить', callback_data='modules_delete')
    # markup.row(btn2)
    # btn4 = types.InlineKeyboardButton('Назад', callback_data='back')
    # markup.row(btn4)
    # return markup

def make_modules_necessary_devices(necessary_devices):
    
    names, callbacks = [], []
    for necessary_device in necessary_devices:
        names.append(necessary_device)
        callbacks.append('add:' + necessary_device)
    return make_button_menu(names, callbacks)
    # markup = types.InlineKeyboardMarkup()
    # for necessary_device in necessary_devices:
    #     btn1 = types.InlineKeyboardButton(necessary_device, callback_data='add:' + necessary_device)
    #     markup.row(btn1)
    # return markup

def make_modules_capabilities(record_id, capabilities):

    names, callbacks = [], []
    for capability in capabilities:
        if capability != None:
            for action in capability['modes']:
                names.append(action)
                callbacks.append('action:' + action + ':' + str(record_id))

    names.append('Назад')
    callbacks.append('back')
    return make_button_menu(names, callbacks)

    # markup = types.InlineKeyboardMarkup()
    # for capability in capabilities:
    #     if capability != None:
    #         for action in capability['modes']:
    #             btn1 = types.InlineKeyboardButton(action, callback_data='action:' + \
    #                                               action + ':' + str(record_id))
    #             markup.row(btn1)
    # btn2 = types.InlineKeyboardButton('Назад', callback_data='back')
    # markup.row(btn2)
    # return markup

# def make_device_table_from_json(json_data):
#     s = "```\n"
#     max_len_device_name = len('Устройство')
#     max_len_device_type = len('Тип')
#     for device in json_data:
#         max_len_device_name = max(max_len_device_name, len(device['name']))
#         max_len_device_type = max(max_len_device_type, len(device['type']))
#     s += '-' * (10 + max_len_device_name + max_len_device_type + len(json_data)) + '\n'
#     s += '| ' + '№' + ' ' * len(json_data) + '| ' + 'Устройство' + ' ' * (max_len_device_name - len('Устройство') + 1) + '| ' + \
#                 'Тип' + ' ' * (max_len_device_type - len('Тип') + 1) + '|\n'
#     i = 1
#     for device in json_data:
#         s += '-' * (10 + max_len_device_name + max_len_device_type + len(json_data)) + '\n'
#         s += '| ' + str(i) + ' ' * len(json_data) + '| ' + device['name'] + \
#              ' ' * (max_len_device_name - len(device['name']) + 1) \
#            + '| ' + device['type'] + ' ' * (max_len_device_type - len(device['type']) + 1) + '|\n' 
#         i += 1
#     s += '-' * (10 + max_len_device_name + max_len_device_type + len(json_data)) + '\n'
#     return s + "```\n"
###############################################################################################