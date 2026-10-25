#pragma once
#ifndef GAMEPLAY_H
#define GAMEPLAY_H

#include "raylib.h"
#include "levels.h"

// Состояния сложности игры (согласовано с main.cpp)
typedef enum {
    SLOZHNOST_LEGKO,
    SLOZHNOST_NORMAL,
    SLOZHNOST_HARD
} Slozhnost;

// Структура персонажа (Бегуна)
struct Begun {
    Vector2 poziciya;
    Vector2 skorost;
    bool na_zemle;
    float shirina;
    float vysota;
    int zhizni;
    float effect1, effect2, effect3, effect4;
    float cooldown1, cooldown2, cooldown3, cooldown4;
    float jump_cooldown;
    float invincible_timer;
    float hit_flash;
};

// Прототипы функций для управления игровым процессом
void ОбновитьИгру(Begun& igrok, float dt, int& tekushchiy_uroven, int& sostoyanie,
    Slozhnost slozhnost, float& igrovoy_taymer, char* prichina_smerti,
    Sound zvuk_uron, Sound zvuk_pobeda, Sound zvuk_bonus);

void ОтрисоватьИгру(Begun igrok, int tekushchiy_uroven, Font shrift, float ui_scale,
    int okno_shirina, int okno_vysota, float igrovoy_taymer,
    Texture2D fon, float smeshchenie_fona);

#endif // GAMEPLAY_H