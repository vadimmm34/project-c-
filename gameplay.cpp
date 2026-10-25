#define _CRT_SECURE_NO_WARNINGS
#include "gameplay.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Локальные переменные для эффекта тряски камеры
static float shakeDuration = 0.0f;
static float shakeMagnitude = 0.0f;

void ОбновитьИгру(Begun& igrok, float dt, int& tekushchiy_uroven, int& sostoyanie,
    Slozhnost slozhnost, float& igrovoy_taymer, char* prichina_smerti,
    Sound zvuk_uron, Sound zvuk_pobeda, Sound zvuk_bonus)
{
    // --- 1. ОБРАБОТКА ТАЙМЕРОВ И КООЛДАУНОВ ---
    if (igrovoy_taymer > 0.0f) {
        igrovoy_taymer -= dt;
        if (igrovoy_taymer <= 0.0f) {
            igrovoy_taymer = 0.0f;
            strcpy(prichina_smerti, "ВРЕМЯ ИСТЕКЛО!");
            sostoyanie = 4; // EKRAN_PROIGRYSH
            return;
        }
    }

    if (igrok.invincible_timer > 0.0f) igrok.invincible_timer -= dt;
    if (igrok.hit_flash > 0.0f) igrok.hit_flash -= dt;
    if (igrok.jump_cooldown > 0.0f) igrok.jump_cooldown -= dt;

    if (igrok.cooldown1 > 0.0f) igrok.cooldown1 -= dt;
    if (igrok.cooldown2 > 0.0f) igrok.cooldown2 -= dt;
    if (igrok.cooldown3 > 0.0f) igrok.cooldown3 -= dt;
    if (igrok.cooldown4 > 0.0f) igrok.cooldown4 -= dt;

    if (igrok.effect1 > 0.0f) igrok.effect1 -= dt;
    if (igrok.effect2 > 0.0f) igrok.effect2 -= dt;
    if (igrok.effect3 > 0.0f) igrok.effect3 -= dt;
    if (igrok.effect4 > 0.0f) igrok.effect4 -= dt;

    // --- 2. УПРАВЛЕНИЕ САБОТАЖНИКА (Клавиши 1-4) ---
    if (IsKeyPressed(KEY_ONE) && igrok.cooldown1 <= 0.0f) { igrok.effect1 = 3.0f; igrok.cooldown1 = 6.0f; }
    if (IsKeyPressed(KEY_TWO) && igrok.cooldown2 <= 0.0f) { igrok.effect2 = 2.5f; igrok.cooldown2 = 7.0f; }
    if (IsKeyPressed(KEY_THREE) && igrok.cooldown3 <= 0.0f) { igrok.effect3 = 4.0f; igrok.cooldown3 = 9.0f; }
    if (IsKeyPressed(KEY_FOUR) && igrok.cooldown4 <= 0.0f) { igrok.effect4 = 2.0f; igrok.cooldown4 = 8.0f; }

    // --- 3. ФИЗИКА И УПРАВЛЕНИЕ БЕГУНА ---
    float speed_modifier = 1.0f;
    if (igrok.effect1 > 0.0f) speed_modifier *= 0.5f; // Замедление от ловушки 1

    float target_speed = 0.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) target_speed = -300.0f * speed_modifier;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) target_speed = 300.0f * speed_modifier;

    // Плавный разгон и торможение
    float accel = igrok.na_zemle ? 15.0f : 7.0f;
    igrok.skorost.x += (target_speed - igrok.skorost.x) * accel * dt;

    // Гравитация (эффект 3 увеличивает гравитацию)
    float gravity = (igrok.effect3 > 0.0f) ? 1600.0f : 1000.0f;
    igrok.skorost.y += gravity * dt;

    // Прыжок
    if ((IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) && igrok.na_zemle && igrok.jump_cooldown <= 0.0f) {
        float force_modifier = 1.0f;
        if (igrok.effect2 > 0.0f) force_modifier *= 0.6f; // Ослабление прыжка от ловушки 2
        igrok.skorost.y = -500.0f * force_modifier;
        igrok.na_zemle = false;
        igrok.jump_cooldown = 0.2f;
    }

    // Применение скорости к координатам
    igrok.poziciya.x += igrok.skorost.x * dt;
    igrok.poziciya.y += igrok.skorost.y * dt;
    igrok.na_zemle = false;

    // --- 4. КОЛЛИЗИИ С ПЛАТФОРМАМИ ---
    Platforma* platformy = nullptr;
    int koll_plat = 0;
    if (tekushchiy_uroven == 1) { platformy = uroven1_platformy; koll_plat = koll_plat_1; }
    else if (tekushchiy_uroven == 2) { platformy = uroven2_platformy; koll_plat = koll_plat_2; }
    else if (tekushchiy_uroven == 3) { platformy = uroven3_platformy; koll_plat = koll_plat_3; }

    Rectangle rect_игрока = { igrok.poziciya.x, igrok.poziciya.y, igrok.shirina, igrok.vysota };

    for (int i = 0; i < koll_plat; i++) {
        // Обновление движущихся платформ
        if (platformy[i].dvizhetsya) {
            platformy[i].phase += platformy[i].speed * dt;
            float newY = platformy[i].startY + sinf(platformy[i].phase) * platformy[i].amplitude;
            platformy[i].deltaY = newY - platformy[i].forma.y;
            platformy[i].forma.y = newY;
        }
        else {
            platformy[i].deltaY = 0.0f;
        }

        if (CheckCollisionRecs(rect_игрока, platformy[i].forma)) {
            // Если стоим НА платформе
            if (igrok.skorost.y >= 0.0f && (rect_игрока.y + rect_игрока.height - igrok.skorost.y * dt <= platformy[i].forma.y + 4.0f)) {
                igrok.poziciya.y = platformy[i].forma.y - igrok.vysota;
                igrok.skorost.y = 0.0f;
                igrok.na_zemle = true;
                if (platformy[i].dvizhetsya) {
                    igrok.poziciya.y += platformy[i].deltaY;
                }

                // Проверка финиша (золотая платформа)
                if (platformy[i].cvet.r == GOLD.r && platformy[i].cvet.g == GOLD.g && platformy[i].cvet.b == GOLD.b) {
                    PlaySound(zvuk_pobeda);

                    if (tekushchiy_uroven < 3) {
                        // Переключаем на промежуточный экран (экран перехода между уровнями)
                        sostoyanie = 12; // EKRAN_MEZHDUNARODNY
                    }
                    else {
                        sostoyanie = 5; // EKRAN_POBEDA для финала игры
                    }
                    return;
                }
            }
        }
    }

    // --- 5. КОЛЛИЗИИ С ТОЧКАМИ СБОРА (БОНУСЫ ВРЕМЕНИ) ---
    Tochka* tochki = nullptr;
    int koll_toch = 0;
    if (tekushchiy_uroven == 1) { tochki = tochki_uroven1; koll_toch = koll_toch_1; }
    else if (tekushchiy_uroven == 2) { tochki = tochki_uroven2; koll_toch = koll_toch_2; }
    else if (tekushchiy_uroven == 3) { tochki = tochki_uroven3; koll_toch = koll_toch_3; }

    for (int i = 0; i < koll_toch; i++) {
        if (tochki[i].aktivna) {
            tochki[i].pulse_timer += dt * 5.0f;
            if (CheckCollisionCircleRec(tochki[i].poziciya, 10.0f, rect_игрока)) {
                tochki[i].aktivna = false;
                PlaySound(zvuk_bonus);
                igrovoy_taymer += 5.0f; // Добавляем 5 секунд
            }
        }
    }

    // --- 6. ПРОВЕРКА ПАДЕНИЯ И УРОНА ---
    if (igrok.poziciya.y > 700.0f) {
        if (igrok.invincible_timer <= 0.0f) {
            igrok.zhizni--;
            PlaySound(zvuk_uron);
            shakeDuration = 0.4f;
            shakeMagnitude = 15.0f;

            if (igrok.zhizni <= 0) {
                strcpy(prichina_smerti, "ВЫ УПАЛИ В БЕЗДНУ!");
                sostoyanie = 4; // EKRAN_PROIGRYSH
            }
            else {
                // Возрождение на старте
                igrok.poziciya = { 50, 400 };
                igrok.skorost = { 0, 0 };
                igrok.invincible_timer = 1.5f;
                igrok.hit_flash = 0.3f;
            }
        }
    }
}

void ОтрисоватьИгру(Begun igrok, int tekushchiy_uroven, Font shrift, float ui_scale,
    int okno_shirina, int okno_vysota, float igrovoy_taymer,
    Texture2D fon, float smeshchenie_fona)
{
    // Логика камеры, следующей за игроком
    Camera2D camera = { 0 };
    camera.target = { igrok.poziciya.x - okno_shirina / 4.0f, 0 };
    camera.offset = { 0, 0 };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    // Ограничение камеры, чтобы не уходила в минус
    if (camera.target.x < 0) camera.target.x = 0;

    // Тряска экрана при уроне
    if (shakeDuration > 0.0f) {
        camera.offset.x += GetRandomValue(-shakeMagnitude, shakeMagnitude);
        camera.offset.y += GetRandomValue(-shakeMagnitude, shakeMagnitude);
        shakeDuration -= GetFrameTime();
    }

    BeginMode2D(camera);

    // Отрисовка движущегося заднего фона
    float parallax_x = -camera.target.x * 0.4f;
    DrawTextureEx(fon, { parallax_x, 0 }, 0.0f, (float)okno_vysota / fon.height, WHITE);
    DrawTextureEx(fon, { parallax_x + fon.width * ((float)okno_vysota / fon.height), 0 }, 0.0f, (float)okno_vysota / fon.height, WHITE);

    // Рендеринг платформ
    Platforma* platformy = nullptr;
    int koll_plat = 0;
    if (tekushchiy_uroven == 1) { platformy = uroven1_platformy; koll_plat = koll_plat_1; }
    else if (tekushchiy_uroven == 2) { platformy = uroven2_platformy; koll_plat = koll_plat_2; }
    else if (tekushchiy_uroven == 3) { platformy = uroven3_platformy; koll_plat = koll_plat_3; }

    for (int i = 0; i < koll_plat; i++) {
        DrawRectangleRec(platformy[i].forma, platformy[i].cvet);
        DrawRectangleLinesEx(platformy[i].forma, 2.0f, DARKGRAY);
    }

    // Рендеринг сфер времени (бонусов)
    Tochka* tochki = nullptr;
    int koll_toch = 0;
    if (tekushchiy_uroven == 1) { tochki = tochki_uroven1; koll_toch = koll_toch_1; }
    else if (tekushchiy_uroven == 2) { tochki = tochki_uroven2; koll_toch = koll_toch_2; }
    else if (tekushchiy_uroven == 3) { tochki = tochki_uroven3; koll_toch = koll_toch_3; }

    for (int i = 0; i < koll_toch; i++) {
        if (tochki[i].aktivna) {
            float rad = 10.0f + sinf(tochki[i].pulse_timer) * 2.0f;
            DrawCircleV(tochki[i].poziciya, rad, ORANGE);
            DrawCircleV(tochki[i].poziciya, rad * 0.6f, YELLOW);
        }
    }

    // Отрисовка Бегуна
    Color cvet_игрока = BLUE;
    if (igrok.hit_flash > 0.0f) cvet_игрока = RED;
    else if (igrok.invincible_timer > 0.0f && ((int)(igrok.invincible_timer * 10) % 2 == 0)) cvet_игрока = ColorAlpha(BLUE, 0.3f);

    DrawRectangleV(igrok.poziciya, { igrok.shirina, igrok.vysota }, cvet_игрока);
    DrawRectangleLines((int)igrok.poziciya.x, (int)igrok.poziciya.y, (int)igrok.shirina, (int)igrok.vysota, DARKBLUE); 

    EndMode2D();

    // ИНТЕРФЕЙС И ХУД (СТАТИЧНЫЙ ЭКРАН СВЕРХУ КАМЕРЫ)
    // Жизни
    for (int i = 0; i < igrok.zhizni; i++) {
        DrawCircle(30 + i * 35, 30, 12, RED);
    }

    // Таймер и Уровень
    char buffer[64];
    sprintf(buffer, "ВРЕМЯ: %.1f сек", igrovoy_taymer);
    DrawTextEx(shrift, buffer, { 20.0f, 60.0f }, 24.0f, 1.5f, WHITE);

    sprintf(buffer, "УРОВЕНЬ: %d", tekushchiy_uroven);
    DrawTextEx(shrift, buffer, { (float)okno_shirina - 180.0f, 20.0f }, 26.0f, 1.5f, GOLD);

    // Перезарядки способностей Саботажника
    for (int i = 0; i < 4; i++) {
        float cd = 0.0f;
        if (i == 0) cd = igrok.cooldown1;
        if (i == 1) cd = igrok.cooldown2;
        if (i == 2) cd = igrok.cooldown3;
        if (i == 3) cd = igrok.cooldown4;

        Color statusColor = (cd <= 0.0f) ? LIME : RED;
        sprintf(buffer, "ЛОВУШКА %d: %s", i + 1, (cd <= 0.0f) ? "ГОТОВА" : "ПЕРЕЗАРЯДКА");
        DrawTextEx(shrift, buffer, { (float)okno_shirina - 260.0f, 60.0f + i * 25.0f }, 18.0f, 1.0f, statusColor);
    }
}