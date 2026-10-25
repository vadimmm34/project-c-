#ifndef LEVELS_H
#define LEVELS_H

#include "raylib.h"

// Структура платформы
struct Platforma {
    Rectangle forma;      
    Color cvet;           
    bool dvizhetsya;      
    float startY;        
    float amplitude;      
    float speed;          
    float phase;         
    float prevY;          
    float deltaY;         
};

// Структура точки сбора
struct Tochka {
    Vector2 poziciya;     
    bool aktivna;         
    bool bonus_time;      
    float pulse_timer;    
};

// Уровень 1
extern Platforma uroven1_platformy[];
extern int koll_plat_1;
extern Tochka tochki_uroven1[];
extern int koll_toch_1;

// Уровень 2
extern Platforma uroven2_platformy[];
extern int koll_plat_2;
extern Tochka tochki_uroven2[];
extern int koll_toch_2;

// Уровень 3
extern Platforma uroven3_platformy[];
extern int koll_plat_3;
extern Tochka tochki_uroven3[];
extern int koll_toch_3;

#endif