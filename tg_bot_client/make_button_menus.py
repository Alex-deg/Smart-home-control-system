from telebot import types
from telebot.types import ReplyKeyboardMarkup, ReplyKeyboardRemove

#################################СОЗДАНИЕ КНОПОЧНЫХ МЕНЮ#######################################
def make_button_menu(list_of_titles, list_of_callbacks_names):
    markup = types.InlineKeyboardMarkup()
    if len(list_of_titles) != len(list_of_callbacks_names):
        raise RuntimeError("Размеры списков названий кнопок и callback'ов для них не совпадают")
    for i in range(len(list_of_titles)):
        btn = types.InlineKeyboardButton(list_of_titles[i], callback_data=list_of_callbacks_names[i])
        markup.row(btn)
    return markup

def make_servers_menu(servers):

    names, callbacks = [], []
    for server in servers:
        names.append(server['name'])
        callbacks.append('server:' + server["name"] + ':' + str(server["server_id"]))
    names.append('Редактирование серверов')
    callbacks.append('servers_actions')
    return make_button_menu(names, callbacks)   

def make_servers_action_menu():
    return make_button_menu(['Добавить', 'Изменить', 'Удалить', 'Назад'],\
                            ['servers_add', 'servers_edit', 'servers_delete', 'back_to_the_servers_list'])

# def make_main_menu():
#     return make_button_menu(['Модули', 'Сценарии', 'Назад'], \
#                             ['modules', 'scenarios', 'back_to_the_servers_list'])

def make_modules_menu(modules):

    names, callbacks = [], []
    for module in modules:
        names.append(module['name'] + f" [ {module['alias']} ]")
        callbacks.append('module:' + module['name'] + ':' + str(module['id']))
    names.append('Редактирование модулей')
    callbacks.append('modules_actions')
    names.append('Назад')
    callbacks.append('back_to_the_servers_list')
    return make_button_menu(names, callbacks)

def make_modules_action_menu():
    return make_button_menu(['Добавить', 'Удалить', 'Назад'],\
                            ['modules_add', 'modules_delete', 'back_to_the_modules_list'])

def make_module_types_menu(module_types):

    names, callbacks = [], []
    for module_type in module_types:
        names.append(module_type['name'])
        callbacks.append('module_type:' + module_type['name'] + ':' + str(module_type['id']))
    names.append('Редактирование модулей')
    callbacks.append('module_types_actions')
    names.append('Назад')
    callbacks.append('back_to_the_modules_menu')
    return make_button_menu(names, callbacks)

def make_module_types_action_menu():
    return make_button_menu(['Добавить', 'Изменить', 'Удалить', 'Назад'],\
                            ['module_types_add', 'module_types_edit', 'module_types_delete', 'back_to_the_modules_list'])

# def make_modules_necessary_devices(necessary_devices):
    
#     names, callbacks = [], []
#     for necessary_device in necessary_devices:
#         names.append(necessary_device)
#         callbacks.append('add:' + necessary_device)
#     return make_button_menu(names, callbacks)

def make_modules_capabilities(module_id, capabilities):

    names, callbacks = [], []
    for capability in capabilities:
        names.append(capability['capability_name'])
        callbacks.append('capability:' + capability['capability_name'] + ':' + str(capability['capability_id']))

    names.append('Назад')
    callbacks.append('back_to_the_modules_list')
    return make_button_menu(names, callbacks)

###############################################################################################