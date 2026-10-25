#ifndef INTERFACE_H
#define INTERFACE_H

#include "raylib.h"

// Отрисовка главного меню (7 кнопок: Играть, Выбор уровня, Управление, Об авторе, Настройки, Статистика, Выход)
void DrawMainMenu(Font font, float ui_scale, int screenW, int screenH, int selected);

// Отрисовка экрана настроек
void DrawSettingsScreen(Font font, float ui_scale, int screenW, int screenH,
    int selectedItem, int currentResolution, int difficulty,
    bool soundOn, Texture2D* backgrounds, int currentBg);

#endif