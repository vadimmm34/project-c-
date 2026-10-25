#include "interface.h"

void DrawMainMenu(Font font, float ui_scale, int screenW, int screenH, int selected) {
    // Заголовок
    int titleSize = (int)(40.0f * ui_scale);
    Vector2 titleDim = MeasureTextEx(font, "Зона испытаний", (float)titleSize, 2.0f);
    Vector2 titlePos = { screenW / 2.0f - titleDim.x / 2.0f, 80.0f * ui_scale };
    DrawTextEx(font, "Зона испытаний", { titlePos.x + 3.0f, titlePos.y + 3.0f }, (float)titleSize, 2.0f, ColorAlpha(BLACK, 0.7f));
    DrawTextEx(font, "Зона испытаний", titlePos, (float)titleSize, 2.0f, ORANGE);

    // Кнопки меню
    float btnW = 200.0f * ui_scale;
    float btnH = 45.0f * ui_scale;
    float centerX = screenW / 2.0f - btnW / 2.0f;
    float y = 140.0f * ui_scale;
    float step = 55.0f * ui_scale;

    const char* labels[7] = { "Играть", "Выбор уровня", "Управление", "Об авторе", "Настройки", "Статистика", "Выход" };
    Color baseColors[7] = { DARKBLUE, DARKBLUE, DARKBLUE, DARKBLUE, DARKBLUE, DARKBLUE, RED };
    Color textColors[7] = { WHITE, WHITE, WHITE, WHITE, WHITE, WHITE, WHITE };

    for (int i = 0; i < 7; i++) {
        Rectangle rect = { centerX, y + i * step, btnW, btnH };
        Color col = (selected == i) ? ORANGE : baseColors[i];
        DrawRectangleRounded(rect, 0.2f, 8, col);
        DrawRectangleRoundedLinesEx(rect, 0.2f, 8, 2.0f, DARKGRAY);
        int fontSize = (int)(24.0f * ui_scale);
        Vector2 textDim = MeasureTextEx(font, labels[i], (float)fontSize, 2.0f);
        Vector2 textPos = {
            rect.x + (rect.width - textDim.x) / 2.0f,
            rect.y + (rect.height - textDim.y) / 2.0f
        };
        DrawTextEx(font, labels[i], textPos, (float)fontSize, 2.0f, textColors[i]);
    }
}

void DrawSettingsScreen(Font font, float ui_scale, int screenW, int screenH,
    int selectedItem, int currentResolution, int difficulty,
    bool soundOn, Texture2D* backgrounds, int currentBg)
{
    // 1. Фон (анимированный, как в главном меню)
    if (backgrounds != nullptr) {
        DrawTexturePro(
            backgrounds[currentBg],
            Rectangle{ 0.0f, 0.0f, (float)backgrounds[currentBg].width, (float)backgrounds[currentBg].height },
            Rectangle{ 0.0f, 0.0f, (float)screenW, (float)screenH },
            Vector2{ 0.0f, 0.0f },
            0.0f,
            WHITE
        );
    }
    else {
        ClearBackground(BLACK);
    }

    // 2. Затемнение для читаемости текста
    DrawRectangle(0, 0, screenW, screenH, ColorAlpha(BLACK, 0.6f));

    // 3. Заголовок
    int titleSize = (int)(40.0f * ui_scale);
    Vector2 titleDim = MeasureTextEx(font, "НАСТРОЙКИ", (float)titleSize, 2.0f);
    DrawTextEx(font, "НАСТРОЙКИ", { screenW / 2.0f - titleDim.x / 2.0f, 60.0f * ui_scale }, (float)titleSize, 2.0f, SKYBLUE);

    // 4. Параметры
    float start_x = 150.0f * ui_scale;
    float znachenie_x = start_x + 350.0f * ui_scale;
    float start_y = 200.0f * ui_scale;
    float shag_y = 80.0f * ui_scale;
    int fontSizeOpt = (int)(28.0f * ui_scale);
    int fontSizeValue = (int)(28.0f * ui_scale);

    // Разрешение
    const char* resText = (currentResolution == 0) ? "1280x720" : (currentResolution == 1) ? "1600x900" : "1920x1080";
    Color labelCol1 = (selectedItem == 0) ? MAGENTA : SKYBLUE;
    Color valueCol1 = (selectedItem == 0) ? LIME : GRAY;
    DrawTextEx(font, "РАЗРЕШЕНИЕ ЭКРАНА:", { start_x, start_y }, (float)fontSizeOpt, 2.0f, labelCol1);
    DrawTextEx(font, resText, { znachenie_x, start_y }, (float)fontSizeValue, 2.0f, valueCol1);
    if (selectedItem == 0) {
        DrawTextEx(font, "<", { znachenie_x - 35.0f * ui_scale, start_y }, (float)fontSizeValue, 2.0f, WHITE);
        DrawTextEx(font, ">", { znachenie_x + MeasureTextEx(font, resText, (float)fontSizeValue, 2.0f).x + 10.0f * ui_scale, start_y }, (float)fontSizeValue, 2.0f, WHITE);
    }

    // Сложность
    const char* diffText = (difficulty == 0) ? "ЛЁГКАЯ" : (difficulty == 1) ? "НОРМАЛЬНАЯ" : "СЛОЖНАЯ";
    Color labelCol2 = (selectedItem == 1) ? MAGENTA : SKYBLUE;
    Color valueCol2 = (selectedItem == 1) ? LIME : GRAY;
    DrawTextEx(font, "СЛОЖНОСТЬ:", { start_x, start_y + shag_y }, (float)fontSizeOpt, 2.0f, labelCol2);
    DrawTextEx(font, diffText, { znachenie_x, start_y + shag_y }, (float)fontSizeValue, 2.0f, valueCol2);
    if (selectedItem == 1) {
        DrawTextEx(font, "<", { znachenie_x - 35.0f * ui_scale, start_y + shag_y }, (float)fontSizeValue, 2.0f, WHITE);
        DrawTextEx(font, ">", { znachenie_x + MeasureTextEx(font, diffText, (float)fontSizeValue, 2.0f).x + 10.0f * ui_scale, start_y + shag_y }, (float)fontSizeValue, 2.0f, WHITE);
    }

    // Звук
    const char* soundText = soundOn ? "ВКЛ" : "ВЫКЛ";
    Color labelCol3 = (selectedItem == 2) ? MAGENTA : SKYBLUE;
    Color valueCol3 = (selectedItem == 2) ? LIME : GRAY;
    DrawTextEx(font, "ЗВУК ПРИЛОЖЕНИЯ:", { start_x, start_y + shag_y * 2 }, (float)fontSizeOpt, 2.0f, labelCol3);
    Vector2 soundSize = MeasureTextEx(font, soundText, (float)fontSizeValue, 2.0f);
    Rectangle soundBtn = { znachenie_x, start_y + shag_y * 2 - 15.0f * ui_scale, soundSize.x + 30.0f * ui_scale, 40.0f * ui_scale };
    DrawRectangleRounded(soundBtn, 0.2f, 8, DARKBLUE);
    DrawRectangleRoundedLinesEx(soundBtn, 0.2f, 8, 2.0f, DARKGRAY);
    DrawTextEx(font, soundText, { znachenie_x + 15.0f * ui_scale, start_y + shag_y * 2 - 10.0f * ui_scale }, (float)fontSizeValue, 2.0f, WHITE);

    // Кнопка "Назад"
    Rectangle backButton = { screenW / 2.0f - 100.0f * ui_scale, 480.0f * ui_scale, 200.0f * ui_scale, 45.0f * ui_scale };
    Color backColor = (selectedItem == 3) ? MAGENTA : DARKGRAY;
    DrawRectangleRounded(backButton, 0.2f, 8, backColor);
    DrawRectangleRoundedLinesEx(backButton, 0.2f, 8, 2.0f, DARKGRAY);
    DrawTextEx(font, "НАЗАД", { backButton.x + 60.0f * ui_scale, backButton.y + 10.0f * ui_scale }, (float)fontSizeValue, 2.0f, WHITE);

    // Подсказка внизу
    int hintSize = (int)(16.0f * ui_scale);
    const char* hintText = "[СТРЕЛКИ ВВЕРХ/ВНИЗ] - Навигация  |  [СТРЕЛКИ ВЛЕВО/ВПРАВО] - Изменение  |  [ENTER] - Применить";
    Vector2 hintDim = MeasureTextEx(font, hintText, (float)hintSize, 2.0f);
    DrawTextEx(font, hintText, { screenW / 2.0f - hintDim.x / 2.0f, (float)screenH - 40.0f * ui_scale }, (float)hintSize, 2.0f, GRAY);
}