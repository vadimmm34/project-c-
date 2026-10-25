#define _CRT_SECURE_NO_WARNINGS
#include "raylib.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include "levels.h"

#pragma warning(disable: 6262)

// Перечисление состояний экранов
enum Sostoyanie {
    EKRAN_ZAGRUZKA,
    EKRAN_MENU,
    EKRAN_IGRA,
    EKRAN_UPRAVLENIE,
    EKRAN_NASTROYKI,
    EKRAN_STATISTIKA,
    EKRAN_AVTOR,
    EKRAN_PROIGRYSH,
    EKRAN_POBEDA,
    EKRAN_VYBOR_UROVNYA,
    EKRAN_PAUZA,
    EKRAN_DIALOG_EXIT,
    EKRAN_MEZHDUNARODNY,
    EKRAN_POZDRAVLENIE
};

#define MAX_ATTEMPTS 20

// Структура одной попытки — хранит метаданные о прохождении
struct AttemptRecord {
    time_t start_time;          
    int play_time_seconds;     
    int total_points;          
    int levels_passed;          
    int lives_lost;           
};

// Статистика всех попыток — фиксированный буфер
struct Statistika {
    AttemptRecord attempts[MAX_ATTEMPTS];
    int count;                  // текущее количество записей
};

// Структура игрока — позиция, скорость, размеры, эффекты и таймеры
struct Begun {
    Vector2 poziciya, skorost;
    bool na_zemle;
    float shirina, vysota;
    int zhizni;
    float effect1, effect2, effect3, effect4; // Эффекты ловушек (временные)
    float cooldown1, cooldown2, cooldown3, cooldown4; // Перезарядки для эффектов
    float jump_cooldown, invincible_timer, hit_flash;
    float anim_scale_y;        
    float anim_timer;           
    bool was_on_ground;         
};

// Уровни сложности
typedef enum { SLOZHNOST_LEGKO, SLOZHNOST_NORMAL, SLOZHNOST_HARD } Slozhnost;

// Прямоугольники кнопок интерфейса — централизованное хранение UI-элементов
struct UI_Rects {
    Rectangle igrat, vybor, upr, nastroyki, statistika, avtor, exit;
    Rectangle nazad, pauza, p_sound, p_resume, p_restart, p_menu;
    Rectangle ur1, ur2, ur3, yes, no;
    Rectangle set_back, s_txt, s_reset;
    Rectangle save_settings_btn;
};

const int MUSIC_COUNT = 10;
Music musicTracks[MUSIC_COUNT];
const int BG_COUNT = 7;
Texture2D bgTextures[BG_COUNT];
int currentBgIndex = 0;

// Настройки игры (сохраняются/загружаются)
struct GameSettings {
    int resolutionIndex;
    int difficulty;
    bool soundEnabled;
} currentSettings;

Music* currentMusic = nullptr;
float aktivnoe_vremya = 0.0f; // Время, когда игрок активно двигается (для движущихся платформ)

// Функции загрузки/сохранения настроек
void LoadSettings(GameSettings& s) {
    // Пытаемся открыть бинарный файл настроек, если есть — читаем, иначе выставляем значения по умолчанию
    std::ifstream f("settings.bin", std::ios::binary);
    if (f.is_open()) {
        f.read(reinterpret_cast<char*>(&s), sizeof(GameSettings));
        f.close();
    }
    else {
        // Значения по умолчанию, если файл не найден
        s.resolutionIndex = 0;
        s.difficulty = 0;
        s.soundEnabled = true;
    }
}

void SaveSettings(const GameSettings& s) {
    // Сохраняем структуру настроек в бинарный файл
    std::ofstream f("settings.bin", std::ios::binary);
    if (f.is_open()) {
        f.write(reinterpret_cast<const char*>(&s), sizeof(GameSettings));
        f.close();
    }
}

// Функции работы со статистикой
void LoadStats(Statistika& s) {
    // Загружаем статистику из бинарного файла; если файла нет — обнуляем счётчик
    std::ifstream f("stats.bin", std::ios::binary);
    if (f.is_open()) {
        f.read(reinterpret_cast<char*>(&s), sizeof(Statistika));
        f.close();
    }
    else {
        s.count = 0;
    }
}

void SaveStatsBin(Statistika& s) {
    // Сохраняем всю статистику в бинарный файл
    std::ofstream f("stats.bin", std::ios::binary);
    if (f.is_open()) {
        f.write(reinterpret_cast<const char*>(&s), sizeof(Statistika));
        f.close();
    }
}

// Экспорт статистики в текстовый файл — удобен для просмотра вне игры
void SaveStatsTxt(const Statistika& s) {
    std::ofstream f("statistics.txt");
    if (!f.is_open()) return;
    f << " СТАТИСТИКА ИГРОВОГО ПРИЛОЖЕНИЯ «ЗОНА ИСПЫТАНИЙ» \n";
    f << "Вход                   Время       Очки     Уровни   Жизни\n";
    for (int i = 0; i < s.count; ++i) {
        const AttemptRecord& r = s.attempts[i];
        char timeStr[64];
        struct tm* tinfo = localtime(&r.start_time);
        strftime(timeStr, sizeof(timeStr), "%d.%m.%Y %H:%M:%S", tinfo);
        int m = r.play_time_seconds / 60;
        int sec = r.play_time_seconds % 60;
        char durStr[16];
        snprintf(durStr, sizeof(durStr), "%02d:%02d", m, sec);
        f << timeStr << "   " << durStr << "      "
            << r.total_points << "       " << r.levels_passed << "        "
            << r.lives_lost << "\n";
    }
    f.close();
}

// Добавление новой попытки в статистику (FIFO при переполнении)
void SaveAttempt(Statistika& s, time_t real_start, int play_time_sec, int points, int levels, int lives) {
    // Если достигнут лимит записей, сдвигаем массив влево, удаляя самую старую запись
    if (s.count >= MAX_ATTEMPTS) {
        for (int i = 0; i < MAX_ATTEMPTS - 1; i++) s.attempts[i] = s.attempts[i + 1];
        s.count = MAX_ATTEMPTS - 1;
    }
    AttemptRecord& r = s.attempts[s.count];
    r.start_time = real_start;
    r.play_time_seconds = play_time_sec;
    r.total_points = points;
    r.levels_passed = levels;
    r.lives_lost = lives;
    s.count++;
    SaveStatsBin(s);
}

// Обновление позиций кнопок при изменении размера окна
void UpdateButtonPositions(int w, int h, float scale, UI_Rects& btn) {
    float cx = w / 2.0f - 100.0f * scale;
    btn.igrat = { cx, 130.0f * scale, 200.0f * scale, 45.0f * scale };
    btn.vybor = { cx, 185.0f * scale, 200.0f * scale, 45.0f * scale };
    btn.upr = { cx, 240.0f * scale, 200.0f * scale, 45.0f * scale };
    btn.nastroyki = { cx, 295.0f * scale, 200.0f * scale, 45.0f * scale };
    btn.statistika = { cx, 350.0f * scale, 200.0f * scale, 45.0f * scale };
    btn.avtor = { cx, 405.0f * scale, 200.0f * scale, 45.0f * scale };
    btn.exit = { cx, 460.0f * scale, 200.0f * scale, 45.0f * scale };
    btn.nazad = btn.pauza = { 20.0f * scale, 20.0f * scale, 120.0f * scale, 40.0f * scale };
    btn.p_sound = { 20.0f * scale, 70.0f * scale, 120.0f * scale, 40.0f * scale };
    btn.p_resume = { cx, 240.0f * scale, 200.0f * scale, 45.0f * scale };
    btn.p_restart = { cx, 300.0f * scale, 200.0f * scale, 45.0f * scale };
    btn.p_menu = { cx, 360.0f * scale, 200.0f * scale, 45.0f * scale };
    btn.ur1 = { cx, 200.0f * scale, 200.0f * scale, 45.0f * scale };
    btn.ur2 = { cx, 260.0f * scale, 200.0f * scale, 45.0f * scale };
    btn.ur3 = { cx, 320.0f * scale, 200.0f * scale, 45.0f * scale };
    float cy = h / 2.0f + 10.0f * scale;
    btn.yes = { w / 2.0f - 110.0f * scale, cy, 100.0f * scale, 40.0f * scale };
    btn.no = { w / 2.0f + 10.0f * scale, cy, 100.0f * scale, 40.0f * scale };
    btn.save_settings_btn = { cx, 480.0f * scale, 200.0f * scale, 45.0f * scale };
    btn.set_back = { 20.0f * scale, h - 80.0f * scale, 120.0f * scale, 45.0f * scale };
    float btnSaveW = 460.0f * scale;
    float btnResetW = 150.0f * scale;
    float gap = 20.0f * scale;
    float totalW = btnSaveW + gap + btnResetW;
    float startX = (w - totalW) / 2.0f;
    btn.s_txt = { startX, h - 80.0f * scale, btnSaveW, 45.0f * scale };
    btn.s_reset = { startX + btnSaveW + gap, h - 80.0f * scale, btnResetW, 45.0f * scale };
}

// Случайный фон — выбирает индекс текстуры фона
void SetRandomBackground() {
    currentBgIndex = GetRandomValue(0, BG_COUNT - 1);
}

// Запуск случайной музыки — можно принудительно выбрать 5-й трек
void PlayRandomMusic(bool soundEnabled, float baseVolume, bool forceMusic5 = false) {
    if (currentMusic) {
        StopMusicStream(*currentMusic);
    }
    int idx;
    if (forceMusic5) {
        idx = 4; // Всегда пятый трек
    }
    else {
        idx = GetRandomValue(0, MUSIC_COUNT - 1);
    }
    currentMusic = &musicTracks[idx];
    PlayMusicStream(*currentMusic);
    SetMusicVolume(*currentMusic, soundEnabled ? baseVolume : 0.0f);
}

// Сбрасывает состояние игрока, платформ и бонусов для старта уровня
void InitGame(Begun& igrok, int& curLevel, int startLvl, float& timeTimer, float& camX, float& cdTime, bool& inCd, int& score,
    time_t& attemptRealStart, float& attemptGameStart, int& attemptLevels, int& attemptLives)
{
    // Установка начальных значений игрока
    igrok = { {100.0f, 360.0f}, {0.0f, 0.0f}, true, 80.0f, 110.0f, 3, 0,0,0,0, 0,0,0,0, 0,0,0, 1.0f, 0.0f, true };
    igrok.hit_flash = 0.0f;
    igrok.invincible_timer = 0.0f;
    curLevel = startLvl;
    timeTimer = 60.0f; // время уровня
    camX = 400.0f;
    cdTime = 2.0f;
    inCd = true;
    score = 0;
    aktivnoe_vremya = 0.0f;

    // Сброс платформ на стартовые позиции с сохранением фазы (для движущихся платформ)
    for (int i = 0; i < koll_plat_1; i++) {
        if (uroven1_platformy[i].dvizhetsya) {
            uroven1_platformy[i].forma.y = uroven1_platformy[i].startY + sinf(uroven1_platformy[i].phase) * uroven1_platformy[i].amplitude;
            uroven1_platformy[i].deltaY = 0.0f;
        }
    }
    for (int i = 0; i < koll_plat_2; i++) {
        if (uroven2_platformy[i].dvizhetsya) {
            uroven2_platformy[i].forma.y = uroven2_platformy[i].startY + sinf(uroven2_platformy[i].phase) * uroven2_platformy[i].amplitude;
            uroven2_platformy[i].deltaY = 0.0f;
        }
    }
    for (int i = 0; i < koll_plat_3; i++) {
        if (uroven3_platformy[i].dvizhetsya) {
            uroven3_platformy[i].forma.y = uroven3_platformy[i].startY + sinf(uroven3_platformy[i].phase) * uroven3_platformy[i].amplitude;
            uroven3_platformy[i].deltaY = 0.0f;
        }
    }

    // Активация всех бонусов (включая таймеры пульсации)
    for (int i = 0; i < koll_toch_1; i++) { tochki_uroven1[i].aktivna = true; tochki_uroven1[i].pulse_timer = 0; }
    for (int i = 0; i < koll_toch_2; i++) { tochki_uroven2[i].aktivna = true; tochki_uroven2[i].pulse_timer = 0; }
    for (int i = 0; i < koll_toch_3; i++) { tochki_uroven3[i].aktivna = true; tochki_uroven3[i].pulse_timer = 0; }
    attemptRealStart = time(NULL);
    attemptGameStart = (float)GetTime();
    attemptLevels = 0;
    attemptLives = 0;
}

// Установка глобальной громкости для всех треков
static void SetGlobalVolume(bool sound_on, float base_vol) {
    float vol = sound_on ? base_vol : 0.0f;
    for (int i = 0; i < MUSIC_COUNT; i++) {
        SetMusicVolume(musicTracks[i], vol);
    }
}

// Отрисовка стилизованной кнопки (используется для многих UI-элементов)
void DrawVolumeButton(Font font, Rectangle rect, const char* text, Vector2 mouse, float ui_scale, bool forcedHover = false, Color baseColor = GRAY, Color hoverColor = LIGHTGRAY, Color pressColor = DARKGRAY, Color textColor = WHITE) {
    bool hover = CheckCollisionPointRec(mouse, rect) || forcedHover;
    bool press = CheckCollisionPointRec(mouse, rect) && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    Color btnColor = press ? pressColor : (hover ? hoverColor : baseColor);
    DrawRectangleRounded({ rect.x + 4.0f * ui_scale, rect.y + 4.0f * ui_scale, rect.width, rect.height }, 0.2f, 8, ColorAlpha(BLACK, 0.3f));
    DrawRectangleRounded(rect, 0.2f, 8, btnColor);
    DrawRectangleRounded({ rect.x, rect.y, rect.width, rect.height / 2.0f }, 0.2f, 8, ColorAlpha(WHITE, press ? 0.0f : 0.15f));
    DrawRectangleRoundedLinesEx(rect, 0.2f, 8, 2.0f, hover ? ORANGE : DARKGRAY);
    float fs = 24.0f * ui_scale;
    Vector2 ts = MeasureTextEx(font, text, fs, 2.0f);
    DrawTextEx(font, text, { rect.x + (rect.width - ts.x) / 2.0f + (press ? 2.0f * ui_scale : 0.0f), rect.y + (rect.height - ts.y) / 2.0f + (press ? 2.0f * ui_scale : 0.0f) }, fs, 2.0f, press ? DARKGRAY : textColor);
}

// Точка респауна и камера по умолчанию (используются при смерти/рестарте)
Vector2 respawnPoint = { 100.0f, 360.0f };
float respawnCameraX = 400.0f;

int main() {
    srand((unsigned int)time(NULL));
    const int razresheniya[4][2] = { {1920, 1080}, {1600, 900}, {1366, 768}, {1280, 720} };
    int vyb_razr = 0;

    // Оконный режим без рамки
    SetConfigFlags(FLAG_WINDOW_UNDECORATED);
    InitWindow(GetMonitorWidth(0), GetMonitorHeight(0), "Зона Испытаний");
    InitAudioDevice();
    SetExitKey(KEY_NULL);

    // Установка иконки окна (если есть)
    Image icon = LoadImage("assets/Background/icon.png");
    if (icon.data != NULL) { SetWindowIcon(icon); UnloadImage(icon); }

    int okno_shirina = razresheniya[vyb_razr][0], okno_vysota = razresheniya[vyb_razr][1];
    float ui_scale = fminf((float)okno_shirina / 800.0f, (float)okno_vysota / 600.0f);

    BeginDrawing(); ClearBackground(BLACK); DrawText("Loading...", 10, 10, 20, WHITE); EndDrawing();

    // Загрузка фонов
    const char* bgFiles[BG_COUNT] = {
        "assets/Background/background.jpg",
        "assets/Background/background2.jpg",
        "assets/Background/background3.png",
        "assets/Background/background4.jpg",
        "assets/Background/background5.png",
        "assets/Background/background6.png",
        "assets/Background/background7.png"
    };
    for (int i = 0; i < BG_COUNT; i++) bgTextures[i] = LoadTexture(bgFiles[i]);
    SetRandomBackground();

    // Загрузка текстур (платформы, персонаж, сердце и т.д.)
    Texture2D texPlatform = LoadTexture("assets/Textures/platforma.png");
    Texture2D texHeart = LoadTexture("assets/Textures/heart.png");
    Texture2D texPerson = LoadTexture("assets/Textures/pers.png");
    Texture2D texLava = LoadTexture("assets/Textures/lava.jpg");
    Texture2D texPauseBg = LoadTexture("assets/Background/fon.png");
    Texture2D texGameOverBg = LoadTexture("assets/Background/FonGameOver.jpg");
    Texture2D texObAvtoreBg = LoadTexture("assets/Background/FonObAvtore.png");
    Texture2D texPobedaBg = LoadTexture("assets/Background/FonPobeda.jpg");
    Texture2D texZagruzkaBg = LoadTexture("assets/Background/ecranzagryzki.png");
    SetTextureFilter(texZagruzkaBg, TEXTURE_FILTER_BILINEAR);

    // Загрузка музыки
    const char* musicFiles[MUSIC_COUNT] = {
        "assets/music/Music1.mp3",
        "assets/music/Music2.mp3",
        "assets/music/Music3.mp3",
        "assets/music/Music4.mp3",
        "assets/music/Music5.mp3",
        "assets/music/Music6.mp3",
        "assets/music/Music7.mp3",
        "assets/music/Music8.mp3",
        "assets/music/Music9.mp3",
        "assets/music/Music10.mp3"
    };
    for (int i = 0; i < MUSIC_COUNT; i++) {
        musicTracks[i] = LoadMusicStream(musicFiles[i]);
        // Устанавливаем начальную громкость для каждого трека, позже её можно менять глобально
        SetMusicVolume(musicTracks[i], 0.5f);
    }

    // Звуковые эффекты
    Sound gameOverSound = LoadSound("assets/Music/GameOver.mp3"), levelCompletedSound = LoadSound("assets/Music/LevelCompleted.mp3");
    Sound buttonSound = LoadSound("assets/Misic/ZvykKnopok.mp3");
    // Настройка громкости эффектов отдельно от музыки
    SetSoundVolume(gameOverSound, 0.7f); SetSoundVolume(levelCompletedSound, 0.7f); SetSoundVolume(buttonSound, 0.4f);

    float baseVolume = 0.5f;
    bool soundEnabled = true;

    Statistika stats = { 0 };
    LoadStats(stats);

    time_t attempt_start_real = 0;
    float attempt_start_game = 0.0f;
    int attempt_levels = 0;
    int attempt_lives_lost = 0;

    Sostoyanie tekushiy_ekran = EKRAN_ZAGRUZKA;
    Sostoyanie prev_ekran = EKRAN_ZAGRUZKA;
    float fadeAlpha = 1.0f;

    float zagruzka_timer = 3.0f;
    float loadProgress = 0.0f;
    float playerScaleX = 1.0f;
    float playerScaleY = 1.0f;
    float landingTimer = 0.0f;

    float zagruzka_zoom = 0.85f;
    float zagruzka_zoom_speed = 0.01f;
    bool zagruzka_mozhno_vyhodit = false;
    bool pervyy_kadr_menyu = false;

    int tekushiy_uroven = 1, ochki = 0, nextLevelAfterTransition = 0, vybrannyy_punkt_nastroek = 0, vybrannaya_knopka = 0;
    float vremya_taymer = 60.0f, avto_kamera_x = 400.0f, bonus_text_timer = 0.0f, otschet_vremya = 2.0f;
    bool pokazat_prichinu = false, levelCompletedSoundPlayed = false, v_otschete = true;
    char prichina_smerti[100] = "";
    Begun igrok = { 0 };
    Slozhnost tekushaya_slozhnost = SLOZHNOST_LEGKO;

    // Загрузка сохранённых настроек (разрешение, сложность, звук)
    LoadSettings(currentSettings);
    vyb_razr = currentSettings.resolutionIndex;
    tekushaya_slozhnost = (Slozhnost)currentSettings.difficulty;
    soundEnabled = currentSettings.soundEnabled;
    SetGlobalVolume(soundEnabled, baseVolume);

    UI_Rects ui;
    UpdateButtonPositions(okno_shirina, okno_vysota, ui_scale, ui);

    Camera2D kamera = { 0 };
    kamera.offset = { (float)okno_shirina / 2.0f, (float)okno_vysota / 2.0f + 50.0f };
    kamera.zoom = 1.0f;
    SetTargetFPS(60);

    // Загрузка шрифта с поддержкой кириллицы (если не удалось — используем дефолтный)
    Font mainFont = { 0 };
    int kody_simvolov[2048] = { 0 }, count = 0;
    for (int i = 32; i < 127; i++) kody_simvolov[count++] = i;
    for (int i = 0x0410; i <= 0x044F; i++) kody_simvolov[count++] = i;
    kody_simvolov[count++] = 0x0401; kody_simvolov[count++] = 0x0451;
    mainFont = LoadFontEx("assets/Shrift/shrift.ttf", 64, kody_simvolov, count);
    if (mainFont.texture.id == 0) mainFont = GetFontDefault();
    SetTextureFilter(mainFont.texture, TEXTURE_FILTER_POINT);

    // Запуск музыки по умолчанию
    currentMusic = &musicTracks[4];
    PlayMusicStream(*currentMusic);
    SetMusicVolume(*currentMusic, soundEnabled ? baseVolume : 0.0f);

    int statsScroll = 0;
    bool levelSelectedFromMenu = false;

    // Корректировка позиций бонусов на 2 и 3 уровнях (простая правка координат)
    for (int i = 0; i < koll_toch_2; i++) { tochki_uroven2[i].poziciya.y -= 25.0f; }
    for (int i = 0; i < koll_toch_3; i++) { tochki_uroven3[i].poziciya.y -= 25.0f; }

    // Главный цикл игры
    while (!WindowShouldClose()) {
        Vector2 mishka = GetMousePosition();

        if (currentMusic) {
            UpdateMusicStream(*currentMusic);
        }

        // Открытие справки по F1 (внешний вызов)
        if (IsKeyPressed(KEY_F1)) {
            system("cmd /c start /MIN \"\" /B Sprauka.chm");
        }

        // Плавное затухание при смене экрана — визуальный эффект
        if (tekushiy_ekran != prev_ekran) {
            fadeAlpha = 1.0f;
            prev_ekran = tekushiy_ekran;
        }

        // Обработка ESC — универсальная логика возврата/паузы/диалогов
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (tekushiy_ekran == EKRAN_IGRA) {
                tekushiy_ekran = EKRAN_PAUZA;
                if (soundEnabled) PlaySound(buttonSound);
            }
            else if (tekushiy_ekran == EKRAN_PAUZA) {
                tekushiy_ekran = EKRAN_IGRA;
                if (soundEnabled) PlaySound(buttonSound);
            }
            else if (tekushiy_ekran == EKRAN_MENU) {
                tekushiy_ekran = EKRAN_DIALOG_EXIT;
                if (soundEnabled) PlaySound(buttonSound);
            }
            else if (tekushiy_ekran == EKRAN_DIALOG_EXIT) {
                tekushiy_ekran = EKRAN_MENU;
                if (soundEnabled) PlaySound(buttonSound);
            }
            else if (tekushiy_ekran == EKRAN_NASTROYKI ||
                tekushiy_ekran == EKRAN_STATISTIKA ||
                tekushiy_ekran == EKRAN_AVTOR ||
                tekushiy_ekran == EKRAN_VYBOR_UROVNYA ||
                tekushiy_ekran == EKRAN_UPRAVLENIE) {
                tekushiy_ekran = EKRAN_MENU;
                if (soundEnabled) PlaySound(buttonSound);
            }
            else if (tekushiy_ekran != EKRAN_PROIGRYSH && tekushiy_ekran != EKRAN_POBEDA && tekushiy_ekran != EKRAN_ZAGRUZKA) {
                tekushiy_ekran = EKRAN_MENU;
                if (soundEnabled) PlaySound(buttonSound);
            }
        }

        // Указатели на текущий уровень — упрощают доступ к массивам уровней
        Platforma* curPlats[] = { uroven1_platformy, uroven2_platformy, uroven3_platformy };
        int curPlatsCount[] = { koll_plat_1, koll_plat_2, koll_plat_3 };
        Tochka* curPoints[] = { tochki_uroven1, tochki_uroven2, tochki_uroven3 };
        int curPointsCount[] = { koll_toch_1, koll_toch_2, koll_toch_3 };

        int u_idx = tekushiy_uroven - 1;
        Platforma* tek_plat = curPlats[u_idx];
        int tek_koll_plat = curPlatsCount[u_idx];
        Tochka* tek_tochki = curPoints[u_idx];
        int tek_koll_toch = curPointsCount[u_idx];

        // Обработчик действий из главного меню — вынесен в лямбду для компактности
        auto doMenuAction = [&](int act) {
            if (soundEnabled) PlaySound(buttonSound);
            switch (act) {
            case 0:
                levelSelectedFromMenu = false;
                respawnPoint = { 100.0f, 360.0f };
                respawnCameraX = 400.0f;
                InitGame(igrok, tekushiy_uroven, 1, vremya_taymer, avto_kamera_x, otschet_vremya, v_otschete, ochki,
                    attempt_start_real, attempt_start_game, attempt_levels, attempt_lives_lost);
                PlayRandomMusic(soundEnabled, baseVolume);
                pokazat_prichinu = false; levelCompletedSoundPlayed = false;
                SetRandomBackground();
                tekushiy_ekran = EKRAN_IGRA;
                break;
            case 1: tekushiy_ekran = EKRAN_VYBOR_UROVNYA; break;
            case 2: tekushiy_ekran = EKRAN_UPRAVLENIE; break;
            case 3: tekushiy_ekran = EKRAN_NASTROYKI; break;
            case 4: tekushiy_ekran = EKRAN_STATISTIKA; break;
            case 5: tekushiy_ekran = EKRAN_AVTOR; break;
            case 6: tekushiy_ekran = EKRAN_DIALOG_EXIT; break;
            }
            };

        // Применение разрешения — обновляет окно и UI
        auto applyRes = [&]() {
            SetWindowSize(razresheniya[vyb_razr][0], razresheniya[vyb_razr][1]);
            okno_shirina = razresheniya[vyb_razr][0]; okno_vysota = razresheniya[vyb_razr][1];
            ui_scale = fminf((float)okno_shirina / 800.0f, (float)okno_vysota / 600.0f);
            kamera.offset = { (float)okno_shirina / 2.0f, (float)okno_vysota / 2.0f + 50.0f };
            UpdateButtonPositions(okno_shirina, okno_vysota, ui_scale, ui);
            if (soundEnabled) PlaySound(buttonSound);
            };

        // --- Блоки обработки разных экранов (заставка, меню, статистика, выбор уровня, пауза и т.д.) ---

        // Заставка
        if (tekushiy_ekran == EKRAN_ZAGRUZKA) {
            zagruzka_timer -= GetFrameTime();
            if (zagruzka_timer > 0.0f) {
                loadProgress = (3.0f - zagruzka_timer) / 3.0f;
                if (loadProgress > 1.0f) loadProgress = 1.0f;
            }
            else {
                loadProgress = 1.0f;
                zagruzka_mozhno_vyhodit = true;
            }
            if (zagruzka_zoom < 1.8f) zagruzka_zoom += zagruzka_zoom_speed * GetFrameTime();

            if (zagruzka_timer <= 0.0f || IsKeyPressed(KEY_ENTER)) {
                tekushiy_ekran = EKRAN_MENU;
                vybrannaya_knopka = 0;
                pervyy_kadr_menyu = true;
                SetRandomBackground();
                if (soundEnabled) PlaySound(buttonSound);
            }
        }

        // Главное меню
        if (tekushiy_ekran == EKRAN_MENU) {
            if (pervyy_kadr_menyu) pervyy_kadr_menyu = false;
            else {
                // Навигация по кнопкам меню с помощью клавиатуры
                if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) vybrannaya_knopka = (vybrannaya_knopka + 1) % 7;
                if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) vybrannaya_knopka = (vybrannaya_knopka + 6) % 7;
                if (IsKeyPressed(KEY_ENTER)) doMenuAction(vybrannaya_knopka);
                Rectangle btns[] = { ui.igrat, ui.vybor, ui.upr, ui.nastroyki, ui.statistika, ui.avtor, ui.exit };
                // Клик мышкой по кнопкам меню
                for (int i = 0; i < 7; ++i) if (CheckCollisionPointRec(mishka, btns[i]) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) doMenuAction(i);
            }
        }
        // Статистика
        else if (tekushiy_ekran == EKRAN_STATISTIKA) {
            int maxVisible = (int)((okno_vysota - 200.0f * ui_scale) / (40.0f * ui_scale));
            if (maxVisible < 1) maxVisible = 1;
            int maxScroll = stats.count - maxVisible;
            if (maxScroll < 0) maxScroll = 0;

            // Прокрутка статистики колёсиком мыши или клавишами вверх/вниз
            int wheel = GetMouseWheelMove();
            if (wheel != 0) {
                statsScroll -= wheel;
                if (statsScroll < 0) statsScroll = 0;
                if (statsScroll > maxScroll) statsScroll = maxScroll;
            }
            if (IsKeyPressed(KEY_UP)) { statsScroll--; if (statsScroll < 0) statsScroll = 0; }
            if (IsKeyPressed(KEY_DOWN)) {
                statsScroll++;
                if (statsScroll > maxScroll) statsScroll = maxScroll;
            }

            // Кнопки экспорта и сброса статистики
            if (CheckCollisionPointRec(mishka, ui.s_txt) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                SaveStatsTxt(stats);
                if (soundEnabled) PlaySound(buttonSound);
            }
            if (CheckCollisionPointRec(mishka, ui.s_reset) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                stats.count = 0;
                SaveStatsBin(stats);
                statsScroll = 0;
                if (soundEnabled) PlaySound(buttonSound);
            }
            if (CheckCollisionPointRec(mishka, ui.nazad) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                tekushiy_ekran = EKRAN_MENU;
                if (soundEnabled) PlaySound(buttonSound);
            }
        }
        // Выбор уровня
        else if (tekushiy_ekran == EKRAN_VYBOR_UROVNYA) {
            if (CheckCollisionPointRec(mishka, ui.nazad) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                tekushiy_ekran = EKRAN_MENU;
                if (soundEnabled) PlaySound(buttonSound);
            }
            Rectangle u_btns[] = { ui.ur1, ui.ur2, ui.ur3 };
            for (int i = 0; i < 3; ++i) {
                if (CheckCollisionPointRec(mishka, u_btns[i]) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    levelSelectedFromMenu = true;
                    respawnPoint = { 100.0f, 360.0f };
                    respawnCameraX = 400.0f;
                    InitGame(igrok, tekushiy_uroven, i + 1, vremya_taymer, avto_kamera_x, otschet_vremya, v_otschete, ochki,
                        attempt_start_real, attempt_start_game, attempt_levels, attempt_lives_lost);
                    PlayRandomMusic(soundEnabled, baseVolume);
                    pokazat_prichinu = false; levelCompletedSoundPlayed = false;
                    SetRandomBackground();
                    tekushiy_ekran = EKRAN_IGRA;
                    if (soundEnabled) PlaySound(buttonSound);
                }
            }
        }
        // Пауза
        else if (tekushiy_ekran == EKRAN_PAUZA) {
            if (CheckCollisionPointRec(mishka, ui.p_resume) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                tekushiy_ekran = EKRAN_IGRA;
                if (soundEnabled) PlaySound(buttonSound);
            }
            if (CheckCollisionPointRec(mishka, ui.p_restart) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                respawnPoint = { 100.0f, 360.0f };
                respawnCameraX = 400.0f;
                InitGame(igrok, tekushiy_uroven, tekushiy_uroven, vremya_taymer, avto_kamera_x, otschet_vremya, v_otschete, ochki,
                    attempt_start_real, attempt_start_game, attempt_levels, attempt_lives_lost);
                PlayRandomMusic(soundEnabled, baseVolume);
                SetRandomBackground();
                tekushiy_ekran = EKRAN_IGRA;
                if (soundEnabled) PlaySound(buttonSound);
            }
            if (CheckCollisionPointRec(mishka, ui.p_menu) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                tekushiy_ekran = EKRAN_MENU;
                if (soundEnabled) PlaySound(buttonSound);
            }
            if (CheckCollisionPointRec(mishka, ui.p_sound) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                soundEnabled = !soundEnabled;
                SetGlobalVolume(soundEnabled, baseVolume);
                if (soundEnabled) PlaySound(buttonSound);
            }
        }
        // Переход между уровнями
        else if (tekushiy_ekran == EKRAN_MEZHDUNARODNY) {
            if (IsKeyPressed(KEY_ENTER)) {
                tekushiy_uroven++;
                respawnPoint = { 100.0f, 360.0f };
                respawnCameraX = 400.0f;
                InitGame(igrok, tekushiy_uroven, nextLevelAfterTransition, vremya_taymer, avto_kamera_x, otschet_vremya, v_otschete, ochki,
                    attempt_start_real, attempt_start_game, attempt_levels, attempt_lives_lost);
                PlayRandomMusic(soundEnabled, baseVolume);
                levelCompletedSoundPlayed = false;
                SetRandomBackground();
                tekushiy_ekran = EKRAN_IGRA;
                if (soundEnabled) PlaySound(buttonSound);
            }
        }
        // Настройки
        else if (tekushiy_ekran == EKRAN_NASTROYKI) {
            // Переключение между пунктами настроек с помощью клавиш вверх/вниз
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { vybrannyy_punkt_nastroek = (vybrannyy_punkt_nastroek + 2) % 3; if (soundEnabled) PlaySound(buttonSound); }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { vybrannyy_punkt_nastroek = (vybrannyy_punkt_nastroek + 1) % 3; if (soundEnabled) PlaySound(buttonSound); }

            // Изменение значения выбранного пункта
            if (vybrannyy_punkt_nastroek == 0 && (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))) {
                vyb_razr = (vyb_razr + (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) ? 1 : 3)) % 4;
            }
            else if (vybrannyy_punkt_nastroek == 1 && (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))) {
                tekushaya_slozhnost = (Slozhnost)(((int)tekushaya_slozhnost + (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) ? 1 : 2)) % 3);
                if (soundEnabled) PlaySound(buttonSound);
            }
            else if (vybrannyy_punkt_nastroek == 2 && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D))) {
            }

            // Вычисление позиций для отображения текущих значений
            float sx = 150.0f * ui_scale, zx = sx + 350.0f * ui_scale, sy = 200.0f * ui_scale, shy = 80.0f * ui_scale, fs = 28.0f * ui_scale;
            char r_txt[64]; snprintf(r_txt, sizeof(r_txt), "%dx%d", razresheniya[vyb_razr][0], razresheniya[vyb_razr][1]);
            Vector2 resSize = MeasureTextEx(mainFont, r_txt, fs, 2.0f);
            Rectangle rectResValue = { zx, sy, resSize.x, resSize.y };
            const char* diffTxt = (tekushaya_slozhnost == SLOZHNOST_LEGKO) ? "ЛЁГКАЯ" : (tekushaya_slozhnost == SLOZHNOST_NORMAL) ? "НОРМАЛЬНАЯ" : "СЛОЖНАЯ";
            Vector2 diffSize = MeasureTextEx(mainFont, diffTxt, fs, 2.0f);
            Rectangle rectDiffValue = { zx, sy + shy, diffSize.x, diffSize.y };
            Vector2 ss = MeasureTextEx(mainFont, soundEnabled ? "ВКЛ" : "ВЫКЛ", fs, 2.0f);
            Rectangle sBtn = { zx, sy + shy * 2 - 15.0f * ui_scale, ss.x + 30.0f * ui_scale, 40.0f * ui_scale };

            // Обработка кликов мышью по значениям настроек
            if (CheckCollisionPointRec(mishka, rectResValue) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                vyb_razr = (vyb_razr + 1) % 4;
                if (soundEnabled) PlaySound(buttonSound);
            }
            if (CheckCollisionPointRec(mishka, rectDiffValue) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                tekushaya_slozhnost = (Slozhnost)(((int)tekushaya_slozhnost + 1) % 3);
                if (soundEnabled) PlaySound(buttonSound);
            }
            if (CheckCollisionPointRec(mishka, sBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                soundEnabled = !soundEnabled;
                SetGlobalVolume(soundEnabled, baseVolume);
                if (soundEnabled) PlaySound(buttonSound);
            }

            // Кнопки-стрелки для изменения значений
            Rectangle resLeft = { zx - 35.0f * ui_scale, sy - 5.0f * ui_scale, 30.0f * ui_scale, 30.0f * ui_scale };
            Rectangle resRight = { zx + resSize.x + 10.0f * ui_scale, sy - 5.0f * ui_scale, 30.0f * ui_scale, 30.0f * ui_scale };
            Rectangle diffLeft = { zx - 35.0f * ui_scale, sy + shy - 5.0f * ui_scale, 30.0f * ui_scale, 30.0f * ui_scale };
            Rectangle diffRight = { zx + diffSize.x + 10.0f * ui_scale, sy + shy - 5.0f * ui_scale, 30.0f * ui_scale, 30.0f * ui_scale };

            if (CheckCollisionPointRec(mishka, resLeft) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { vyb_razr = (vyb_razr + 3) % 4; }
            if (CheckCollisionPointRec(mishka, resRight) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { vyb_razr = (vyb_razr + 1) % 4; }
            if (CheckCollisionPointRec(mishka, diffLeft) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { tekushaya_slozhnost = (Slozhnost)(((int)tekushaya_slozhnost + 2) % 3); if (soundEnabled) PlaySound(buttonSound); }
            if (CheckCollisionPointRec(mishka, diffRight) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { tekushaya_slozhnost = (Slozhnost)(((int)tekushaya_slozhnost + 1) % 3); if (soundEnabled) PlaySound(buttonSound); }

            // Области для подсветки выбранного пункта
            Rectangle rectRes = { sx, sy - 5.0f * ui_scale, 400.0f * ui_scale, 30.0f * ui_scale };
            Rectangle rectDiff = { sx, sy + shy - 5.0f * ui_scale, 400.0f * ui_scale, 30.0f * ui_scale };
            Rectangle rectSound = { sx, sy + shy * 2 - 5.0f * ui_scale, 400.0f * ui_scale, 30.0f * ui_scale };

            if (CheckCollisionPointRec(mishka, rectRes)) vybrannyy_punkt_nastroek = 0;
            else if (CheckCollisionPointRec(mishka, rectDiff)) vybrannyy_punkt_nastroek = 1;
            else if (CheckCollisionPointRec(mishka, rectSound)) vybrannyy_punkt_nastroek = 2;

            // Сохранение настроек и применение разрешения
            if (CheckCollisionPointRec(mishka, ui.save_settings_btn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                currentSettings.resolutionIndex = vyb_razr;
                currentSettings.difficulty = (int)tekushaya_slozhnost;
                currentSettings.soundEnabled = soundEnabled;
                SaveSettings(currentSettings);
                if (soundEnabled) PlaySound(buttonSound);
                applyRes();
                SetGlobalVolume(soundEnabled, baseVolume);
            }
            if (CheckCollisionPointRec(mishka, ui.set_back) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                tekushiy_ekran = EKRAN_MENU;
                if (soundEnabled) PlaySound(buttonSound);
            }
        }

        // Диалог выхода
        else if (tekushiy_ekran == EKRAN_DIALOG_EXIT) {
            if (CheckCollisionPointRec(mishka, ui.yes) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (soundEnabled) PlaySound(buttonSound);
                if (attempt_start_game > 0) {
                    int duration = (int)(GetTime() - attempt_start_game);
                    SaveAttempt(stats, attempt_start_real, duration, ochki, attempt_levels, attempt_lives_lost);
                }
                break; // Выход из главного цикла
            }
            if (CheckCollisionPointRec(mishka, ui.no) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                tekushiy_ekran = EKRAN_MENU;
                if (soundEnabled) PlaySound(buttonSound);
            }
        }

        // Экран поздравления
        else if (tekushiy_ekran == EKRAN_POZDRAVLENIE) {
            if (IsKeyPressed(KEY_ENTER)) {
                if (levelSelectedFromMenu) {
                    levelSelectedFromMenu = false;
                    SetRandomBackground();
                }
                else {
                    SetRandomBackground();
                }
                PlayRandomMusic(soundEnabled, baseVolume);
                tekushiy_ekran = EKRAN_MENU;
                pokazat_prichinu = false;
                if (soundEnabled) PlaySound(buttonSound);
            }
        }
        else if (tekushiy_ekran == EKRAN_PROIGRYSH || tekushiy_ekran == EKRAN_POBEDA) {
            if (IsKeyPressed(KEY_ENTER)) {
                SetRandomBackground();
                PlayRandomMusic(soundEnabled, baseVolume);
                tekushiy_ekran = EKRAN_MENU;
                pokazat_prichinu = false;
                if (soundEnabled) PlaySound(buttonSound);
            }
        }

        // Игровой процесс — основной блок логики (ввод, физика, коллизии, эффекты)
        else if (tekushiy_ekran == EKRAN_IGRA) {
            // Переключение на паузу по клавише P или клику по кнопке паузы
            if (IsKeyPressed(KEY_P) || (CheckCollisionPointRec(mishka, ui.pauza) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
                tekushiy_ekran = EKRAN_PAUZA;
                if (soundEnabled) PlaySound(buttonSound);
            }

            bool anyActiveEffect = (igrok.effect1 > 0.0f || igrok.effect2 > 0.0f || igrok.effect3 > 0.0f || igrok.effect4 > 0.0f);

            // Активация ловушек (клавиши 1-4) — если нет активных эффектов
            if (!anyActiveEffect) {
                int keys[] = { KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR };
                int keysNumpad[] = { KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4 };
                for (int i = 0; i < 4; i++) {
                    if ((IsKeyPressed(keys[i]) || IsKeyPressed(keysNumpad[i])) && (&igrok.cooldown1)[i] <= 0.0f) {
                        (&igrok.effect1)[i] = (i < 3) ? 4.0f : 3.0f;
                        (&igrok.cooldown1)[i] = (&igrok.effect1)[i] + ((i == 2 || i == 3) ? 15.0f : 10.0f);
                        if (soundEnabled) PlaySound(buttonSound);
                        break;
                    }
                }
            }

            // Обработка F1 — открытие справки (повторно, в игре)
            if (IsKeyPressed(KEY_F1)) {
                system("cmd /c start \"\" /B Sprauka.chm");
            }

            // Панель саботажника (кликабельная) — альтернативный ввод для эффектов
            float sp = 160.0f * ui_scale, vp = 280.0f * ui_scale, xp = 5.0f * ui_scale, yp = okno_vysota / 2.0f - vp / 2.0f;
            float sks = 130.0f * ui_scale, vks = 65.0f * ui_scale, ox = (sp - sks) / 2.0f, pv = (vp - (vks * 4)) / 5.0f;
            Rectangle r_sab[4];
            for (int i = 0; i < 4; i++) {
                r_sab[i] = { xp + ox, yp + pv * (i + 1) + vks * i, sks, vks };
                if (!anyActiveEffect && CheckCollisionPointRec(mishka, r_sab[i]) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && (&igrok.cooldown1)[i] <= 0.0f) {
                    (&igrok.effect1)[i] = (i < 3) ? 4.0f : 3.0f;
                    (&igrok.cooldown1)[i] = (&igrok.effect1)[i] + ((i == 2 || i == 3) ? 15.0f : 10.0f);
                    if (soundEnabled) PlaySound(buttonSound);
                    break;
                }
            }

            // Стартовый отсчёт — фиксирует игрока на месте до конца отсчёта
            if (v_otschete) {
                igrok.poziciya = { 100.0f, 360.0f };
                igrok.skorost = { 0.0f, 0.0f };
                igrok.na_zemle = true;
                if ((otschet_vremya -= GetFrameTime()) <= 0) v_otschete = false;
            }
            else {
                // Активное время — увеличиваем только когда игрок бежит (используется для движения платформ)
                aktivnoe_vremya += GetFrameTime();

                // Обновление таймера уровня — если время вышло, переход в экран поражения
                if ((vremya_taymer -= GetFrameTime()) <= 0) {
                    PlaySound(gameOverSound);
                    if (currentMusic) StopMusicStream(*currentMusic);
                    strcpy(prichina_smerti, "Время вышло!");
                    pokazat_prichinu = true;
                    int duration = (int)(GetTime() - attempt_start_game);
                    SaveAttempt(stats, attempt_start_real, duration, ochki, attempt_levels, attempt_lives_lost);
                    tekushiy_ekran = EKRAN_PROIGRYSH;
                }

                // Обновление неуязвимости — визуальный эффект мигания
                if (igrok.invincible_timer > 0) {
                    igrok.invincible_timer -= GetFrameTime();
                    igrok.hit_flash = sinf(GetTime() * 30.0f) > 0 ? 1.0f : 0.0f;
                }
                else igrok.hit_flash = 0.0f;

                // Таймеры эффектов и перезарядок — уменьшаем по времени
                float* effs[] = { &igrok.effect1, &igrok.effect2, &igrok.effect3, &igrok.effect4 };
                float* cds[] = { &igrok.cooldown1, &igrok.cooldown2, &igrok.cooldown3, &igrok.cooldown4 };
                for (int i = 0; i < 4; i++) { if (*effs[i] > 0) *effs[i] -= GetFrameTime(); if (*cds[i] > 0) *cds[i] -= GetFrameTime(); }

                // Анимация приземления/прыжка — небольшие визуальные изменения размера персонажа
                if (!igrok.was_on_ground && igrok.na_zemle) {
                    igrok.anim_scale_y = 0.7f;
                    igrok.anim_timer = 0.2f;
                }
                else if (igrok.was_on_ground && !igrok.na_zemle && igrok.skorost.y < -5.0f) {
                    igrok.anim_scale_y = 1.3f;
                    igrok.anim_timer = 0.15f;
                }
                igrok.was_on_ground = igrok.na_zemle;
                if (igrok.anim_timer > 0.0f) {
                    igrok.anim_timer -= GetFrameTime();
                    if (igrok.anim_timer <= 0.0f) igrok.anim_scale_y = 1.0f;
                    else igrok.anim_scale_y = 1.0f + (igrok.anim_scale_y - 1.0f) * (igrok.anim_timer / 0.2f);
                }
                else igrok.anim_scale_y = 1.0f;

                // Скорость камеры в зависимости от сложности — влияет на "догоняющую" камеру
                float spds2[] = { 3.8f, 4.5f, 5.3f }, spds3[] = { 4.4f, 5.3f, 5.4f };
                float c_sp2 = spds2[tekushaya_slozhnost], c_sp3 = spds3[tekushaya_slozhnost];

                // Ввод игрока — направление и инверсия (эффект4)
                int input_dir = (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) ? 1 : ((IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) ? -1 : 0);
                if (igrok.effect4 > 0) input_dir = -input_dir;

                igrok.skorost.x = input_dir * (6.0f * (igrok.effect1 > 0 ? 0.5f : 1.0f));

                // Прыжок — короткий кулдаун, чтобы избежать многократных прыжков
                if (igrok.jump_cooldown <= 0 && (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) && igrok.na_zemle) {
                    igrok.skorost.y = -14.0f;
                    igrok.na_zemle = false;
                    igrok.jump_cooldown = 0.1f;
                }
                if (igrok.jump_cooldown > 0) igrok.jump_cooldown -= GetFrameTime();

                // Гравитация и перемещение
                igrok.skorost.y += 0.8f * (igrok.effect2 > 0 ? 1.5f : 1.0f);
                igrok.poziciya.x += igrok.skorost.x;
                igrok.poziciya.y += igrok.skorost.y;
                igrok.na_zemle = false;
                Rectangle p_rect = { igrok.poziciya.x, igrok.poziciya.y, igrok.shirina, igrok.vysota };

                // Пересчёт движущихся платформ с использованием активного времени
                for (int i = 0; i < tek_koll_plat; i++) {
                    if (tek_plat[i].dvizhetsya) {
                        float oldY = tek_plat[i].forma.y;
                        tek_plat[i].forma.y = tek_plat[i].startY + sinf(aktivnoe_vremya * tek_plat[i].speed + tek_plat[i].phase) * tek_plat[i].amplitude;
                        float dy = tek_plat[i].forma.y - oldY;

                        // Если игрок стоит на платформе — переносим его вместе с платформой
                        if (igrok.na_zemle && igrok.poziciya.x + igrok.shirina > tek_plat[i].forma.x && igrok.poziciya.x < tek_plat[i].forma.x + tek_plat[i].forma.width && fabsf((igrok.poziciya.y + igrok.vysota) - oldY) <= 6.0f) {
                            igrok.poziciya.y += dy;
                        }
                        tek_plat[i].deltaY = dy;
                    }

                    // Коллизии игрока с платформой — обработка сверху, снизу и по бокам
                    if (CheckCollisionRecs(p_rect, tek_plat[i].forma)) {
                        if (igrok.skorost.y >= 0 && igrok.poziciya.y + igrok.vysota <= tek_plat[i].forma.y + igrok.skorost.y + 8.0f) {
                            // Приземление сверху
                            igrok.skorost.y = 0;
                            igrok.poziciya.y = tek_plat[i].forma.y - igrok.vysota;
                            igrok.na_zemle = true;

                            // В режиме HARD платформа (не золотая) становится точкой респауна
                            if (tekushaya_slozhnost == SLOZHNOST_HARD && tekushiy_uroven > 1 && tek_plat[i].cvet.r != GOLD.r && tek_plat[i].cvet.g != GOLD.g && tek_plat[i].cvet.b != GOLD.b) {
                                respawnPoint = igrok.poziciya;
                                respawnCameraX = avto_kamera_x;
                            }

                            // Проверка финиша (золотая платформа)
                            if (tek_plat[i].cvet.r == GOLD.r && tek_plat[i].cvet.g == GOLD.g && tek_plat[i].cvet.b == GOLD.b) {
                                attempt_levels++;
                                if (!levelCompletedSoundPlayed) {
                                    PlaySound(levelCompletedSound);
                                    levelCompletedSoundPlayed = true;
                                }
                                if (currentMusic) StopMusicStream(*currentMusic);
                                int duration = (int)(GetTime() - attempt_start_game);
                                SaveAttempt(stats, attempt_start_real, duration, ochki, attempt_levels, attempt_lives_lost);

                                // Если уровень выбран из меню или это 3-й уровень — показываем финал
                                if (levelSelectedFromMenu || tekushiy_uroven == 3) {
                                    tekushiy_ekran = EKRAN_POZDRAVLENIE;
                                }
                                else {
                                    // Иначе — переход на следующий уровень
                                    nextLevelAfterTransition = tekushiy_uroven + 1;
                                    tekushiy_ekran = EKRAN_MEZHDUNARODNY;
                                }
                                break;
                            }
                        }
                        else if (igrok.skorost.y < 0 && igrok.poziciya.y >= tek_plat[i].forma.y + tek_plat[i].forma.height + igrok.skorost.y - 4.0f) {
                            // Столкновение снизу — останавливаем вертикальную скорость
                            igrok.skorost.y = 0;
                            igrok.poziciya.y = tek_plat[i].forma.y + tek_plat[i].forma.height;
                        }
                        else if (igrok.skorost.x > 0 && igrok.poziciya.x + igrok.shirina <= tek_plat[i].forma.x + igrok.skorost.x + 2.0f) {
                            // Столкновение справа — корректируем X
                            igrok.poziciya.x = tek_plat[i].forma.x - igrok.shirina;
                        }
                        else if (igrok.skorost.x < 0 && igrok.poziciya.x >= tek_plat[i].forma.x + tek_plat[i].forma.width + igrok.skorost.x - 2.0f) {
                            // Столкновение слева
                            igrok.poziciya.x = tek_plat[i].forma.x + tek_plat[i].forma.width;
                        }
                    }
                }

                // Сбор бонусов — проверка пересечения с кругом бонуса
                for (int i = 0; i < tek_koll_toch; i++) {
                    if (tek_tochki[i].aktivna && CheckCollisionCircleRec(tek_tochki[i].poziciya, 10.0f, p_rect)) {
                        if (tek_tochki[i].bonus_time) { vremya_taymer += 10.0f; bonus_text_timer = 1.0f; }
                        else ochki += 10;
                        tek_tochki[i].aktivna = false;
                    }
                }

                // Смерть игрока — вынесено в лямбду die для повторного использования
                auto die = [&](const char* reason) {
                    attempt_lives_lost++;
                    igrok.zhizni--;
                    igrok.invincible_timer = 1.5f;
                    if (igrok.zhizni <= 0) {
                        PlaySound(gameOverSound);
                        if (currentMusic) StopMusicStream(*currentMusic);
                        strcpy(prichina_smerti, reason);
                        pokazat_prichinu = true;
                        int duration = (int)(GetTime() - attempt_start_game);
                        SaveAttempt(stats, attempt_start_real, duration, ochki, attempt_levels, attempt_lives_lost);
                        tekushiy_ekran = EKRAN_PROIGRYSH;
                    }
                    else {
                        // Если остались жизни — респаун в зависимости от сложности и уровня
                        if (tekushaya_slozhnost == SLOZHNOST_HARD && tekushiy_uroven > 1) {
                            igrok.poziciya = respawnPoint;
                            igrok.skorost = { 0.0f, 0.0f };
                            igrok.na_zemle = true;
                            avto_kamera_x = respawnCameraX;
                            kamera.target.x = avto_kamera_x;
                        }
                        else {
                            igrok.poziciya = { 100.0f, 360.0f };
                            igrok.skorost = { 0.0f, 0.0f };
                            igrok.na_zemle = true;
                            if (tekushiy_uroven > 1) {
                                avto_kamera_x = 400.0f;
                                kamera.target.x = avto_kamera_x;
                            }
                        }
                    }
                    };

                // Падение в лаву — проверка по Y
                if (igrok.poziciya.y + igrok.vysota > 660.0f) {
                    if (igrok.invincible_timer <= 0) {
                        die("Вы утонули в лаве!");
                    }
                    else {
                        // Если неуязвимость — просто перемещаем игрока на респаун
                        if (tekushaya_slozhnost == SLOZHNOST_HARD && tekushiy_uroven > 1) {
                            igrok.poziciya = respawnPoint;
                            avto_kamera_x = respawnCameraX;
                            kamera.target.x = avto_kamera_x;
                        }
                        else {
                            igrok.poziciya = { 100.0f, 365.0f };
                            if (tekushiy_uroven > 1) avto_kamera_x = 400.0f;
                        }
                        igrok.skorost = { 0.0f, 0.0f };
                        igrok.na_zemle = true;
                    }
                }

                // Управление камерой — разные режимы для уровня 1 и остальных
                if (tekushiy_uroven == 1) {
                    // На первом уровне камера следует за игроком
                    kamera.target.x = igrok.poziciya.x;
                    kamera.target.y = igrok.poziciya.y;
                    if (kamera.target.x < 400.0f) kamera.target.x = 400.0f;
                    if (kamera.target.x > 12650.0f) kamera.target.x = 12650.0f;
                }
                else {
                    // На остальных уровнях камера движется автоматически с фиксированной скоростью
                    avto_kamera_x += (tekushiy_uroven == 2) ? c_sp2 : c_sp3;
                    kamera.target.x = avto_kamera_x;
                    kamera.target.y = igrok.poziciya.y;
                    // Если игрок отстаёт от камеры — смерть
                    if (igrok.poziciya.x < avto_kamera_x - okno_shirina / 2.0f) {
                        if (igrok.invincible_timer <= 0) die("Камера догнала!");
                        else {
                            if (tekushaya_slozhnost == SLOZHNOST_HARD && tekushiy_uroven > 1) {
                                igrok.poziciya = respawnPoint;
                                avto_kamera_x = respawnCameraX;
                                kamera.target.x = avto_kamera_x;
                            }
                            else {
                                igrok.poziciya = { 100.0f, 365.0f };
                                avto_kamera_x = 400.0f;
                            }
                            igrok.skorost = { 0.0f, 0.0f };
                            igrok.na_zemle = true;
                        }
                    }
                }
                if (kamera.target.y > 430.0f) kamera.target.y = 430.0f;
            }
        }

        // Обновление указателей перед отрисовкой (повторно)
        u_idx = tekushiy_uroven - 1;
        tek_plat = curPlats[u_idx];
        tek_koll_plat = curPlatsCount[u_idx];
        tek_tochki = curPoints[u_idx];
        tek_koll_toch = curPointsCount[u_idx];

        // --- Отрисовка всех экранов ---
        BeginDrawing();
        ClearBackground(BLACK);

        // Заставка
        if (tekushiy_ekran == EKRAN_ZAGRUZKA) {
            if (texZagruzkaBg.id) {
                Rectangle srcRect = { 0.0f, 0.0f, (float)texZagruzkaBg.width, (float)texZagruzkaBg.height };
                Rectangle dstRect = {
                    (float)okno_shirina / 2.0f,
                    (float)okno_vysota / 2.0f,
                    (float)okno_shirina * zagruzka_zoom,
                    (float)okno_vysota * zagruzka_zoom
                };
                Vector2 origin = { dstRect.width / 2.0f, dstRect.height / 2.0f };
                DrawTexturePro(texZagruzkaBg, srcRect, dstRect, origin, 0.0f, WHITE);
            }
            else DrawRectangle(0, 0, okno_shirina, okno_vysota, DARKGRAY);

            // Прогресс-бар загрузки
            float barWidth = okno_shirina * 0.6f;
            float barHeight = 25.0f * ui_scale;
            float barX = (okno_shirina - barWidth) / 2.0f;
            float barY = okno_vysota - 120.0f * ui_scale;
            DrawRectangleRounded({ barX, barY, barWidth, barHeight }, 0.3f, 8, DARKGRAY);
            DrawRectangleRounded({ barX, barY, barWidth * loadProgress, barHeight }, 0.3f, 8, LIME);
            char percentText[16];
            snprintf(percentText, sizeof(percentText), "%d%%", (int)(loadProgress * 100));
            float fsPercent = 28.0f * ui_scale;
            Vector2 tsPercent = MeasureTextEx(mainFont, percentText, fsPercent, 2.0f);
            DrawTextEx(mainFont, percentText, { okno_shirina / 2.0f - tsPercent.x / 2.0f, barY - 35.0f * ui_scale }, fsPercent, 2.0f, WHITE);

            if (zagruzka_mozhno_vyhodit) {
                const char* txt = "Нажмите ENTER для продолжения";
                float fs = 20.0f * ui_scale;
                Vector2 ts = MeasureTextEx(mainFont, txt, fs, 2.0f);
                Rectangle podlozhka = { okno_shirina / 2.0f - ts.x / 2.0f - 10.0f, okno_vysota - 60.0f * ui_scale - 5.0f, ts.x + 20.0f, ts.y + 10.0f };
                DrawRectangleRec(podlozhka, ColorAlpha(WHITE, 0.7f));
                DrawTextEx(mainFont, txt, { okno_shirina / 2.0f - ts.x / 2.0f, okno_vysota - 60.0f * ui_scale }, fs, 2.0f, BLACK);
            }
        }

        // Диалог выхода
        if (tekushiy_ekran == EKRAN_DIALOG_EXIT) {
            DrawRectangle(0, 0, okno_shirina, okno_vysota, ColorAlpha(BLACK, 0.8f));
            Rectangle okno_d = { okno_shirina / 2.0f - 160.0f * ui_scale, okno_vysota / 2.0f - 60.0f * ui_scale, 320.0f * ui_scale, 140.0f * ui_scale };
            DrawRectangleRec(okno_d, DARKGRAY);
            DrawRectangleLinesEx(okno_d, 3.0f, WHITE);
            Vector2 ts = MeasureTextEx(mainFont, "Выйти из игры?", 20.0f * ui_scale, 2.0f);
            DrawTextEx(mainFont, "Выйти из игры?", { okno_shirina / 2.0f - ts.x / 2.0f, okno_d.y + 20.0f * ui_scale }, 20.0f * ui_scale, 2.0f, WHITE);
            DrawVolumeButton(mainFont, ui.yes, "ДА", mishka, ui_scale, true, Color{ 150, 0, 0, 255 }, Color{ 255, 50, 50, 255 }, Color{ 100, 0, 0, 255 });
            DrawVolumeButton(mainFont, ui.no, "НЕТ", mishka, ui_scale, true, GRAY, LIGHTGRAY, DARKGRAY);
        }

        // Экраны завершения (поражение/победа)
        else if (tekushiy_ekran == EKRAN_PROIGRYSH || tekushiy_ekran == EKRAN_POBEDA) {
            Texture2D* bg = (tekushiy_ekran == EKRAN_PROIGRYSH) ? &texGameOverBg : &texPobedaBg;
            if (bg->id) DrawTexturePro(*bg, { 0,0,(float)bg->width,(float)bg->height }, { 0,0,(float)okno_shirina,(float)okno_vysota }, { 0,0 }, 0, WHITE);
            else DrawRectangle(0, 0, okno_shirina, okno_vysota, DARKGRAY);
            float cx = okno_shirina / 2.0f;
            if (tekushiy_ekran == EKRAN_PROIGRYSH) {
                Vector2 ts = MeasureTextEx(mainFont, "Игра окончена!", 60.0f, 2.0f);
                DrawTextEx(mainFont, "Игра окончена!", { cx - ts.x / 2.0f, 150.0f * ui_scale }, 60.0f, 2.0f, RED);
                if (pokazat_prichinu) {
                    ts = MeasureTextEx(mainFont, prichina_smerti, 30.0f, 2.0f);
                    DrawTextEx(mainFont, prichina_smerti, { cx - ts.x / 2.0f, 280.0f * ui_scale }, 30.0f, 2.0f, YELLOW);
                }
                ts = MeasureTextEx(mainFont, "Нажмите Enter для возврата в меню", 30.0f, 2.0f);
                DrawTextEx(mainFont, "Нажмите Enter для возврата в меню", { cx - ts.x / 2.0f, 400.0f * ui_scale }, 30.0f, 2.0f, LIGHTGRAY);
            }
            else {
                Vector2 ts = MeasureTextEx(mainFont, "Победа!", 60.0f, 2.0f);
                DrawTextEx(mainFont, "Победа!", { cx - ts.x / 2.0f, 150.0f * ui_scale }, 60.0f, 2.0f, GREEN);
                ts = MeasureTextEx(mainFont, "Нажмите Enter для возврата в меню", 30.0f, 2.0f);
                DrawTextEx(mainFont, "Нажмите Enter для возврата в меню", { cx - ts.x / 2.0f, 400.0f * ui_scale }, 30.0f, 2.0f, LIGHTGRAY);
            }
        }

        // Экран перехода между уровнями
        else if (tekushiy_ekran == EKRAN_MEZHDUNARODNY) {
            // ОТРИСОВКА ФОНА ПОБЕДЫ НА ЭКРАНЕ ПЕРЕХОДА
            if (texPobedaBg.id != 0 && texPobedaBg.width > 0 && texPobedaBg.height > 0) {
                DrawTexturePro(texPobedaBg,
                    { 0, 0, (float)texPobedaBg.width, (float)texPobedaBg.height },
                    { 0, 0, (float)okno_shirina, (float)okno_vysota },
                    { 0, 0 }, 0, WHITE);
            }
            else {
                DrawRectangle(0, 0, okno_shirina, okno_vysota, Color{ 20, 20, 80, 255 });
            }

            char text[64];
            snprintf(text, sizeof(text), "Уровень %d пройден!", nextLevelAfterTransition - 1);
            Vector2 ts = MeasureTextEx(mainFont, text, 40.0f * ui_scale, 2.0f);
            float cx = okno_shirina / 2.0f;
            DrawTextEx(mainFont, text, { cx - ts.x / 2.0f, okno_vysota / 2.0f - 40.0f * ui_scale - ts.y }, 40.0f * ui_scale, 2.0f, GREEN);
            ts = MeasureTextEx(mainFont, "Нажмите Enter для продолжения", 24.0f * ui_scale, 2.0f);
            DrawTextEx(mainFont, "Нажмите Enter для продолжения", { cx - ts.x / 2.0f, okno_vysota / 2.0f - 20.0f * ui_scale }, 24.0f * ui_scale, 2.0f, YELLOW);
        }

        // Экран поздравления (финал или выбранный уровень)
        else if (tekushiy_ekran == EKRAN_POZDRAVLENIE) {
            if (texPobedaBg.id != 0 && texPobedaBg.width > 0 && texPobedaBg.height > 0) {
                DrawTexturePro(texPobedaBg,
                    { 0, 0, (float)texPobedaBg.width, (float)texPobedaBg.height },
                    { 0, 0, (float)okno_shirina, (float)okno_vysota },
                    { 0, 0 }, 0, WHITE);
            }
            else {
                DrawRectangle(0, 0, okno_shirina, okno_vysota, Color{ 20, 20, 80, 255 });
            }

            float cx = okno_shirina / 2.0f;

            if (levelSelectedFromMenu) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Уровень %d пройден!", tekushiy_uroven);
                Vector2 ts = MeasureTextEx(mainFont, msg, 50.0f * ui_scale, 2.0f);
                DrawTextEx(mainFont, msg, { cx - ts.x / 2.0f, 150.0f * ui_scale }, 50.0f * ui_scale, 2.0f, GREEN);
                ts = MeasureTextEx(mainFont, "Нажмите Enter для возврата в меню", 30.0f * ui_scale, 2.0f);
                DrawTextEx(mainFont, "Нажмите Enter для возврата в меню", { cx - ts.x / 2.0f, 400.0f * ui_scale }, 30.0f * ui_scale, 2.0f, LIGHTGRAY);
            }
            else {
                Vector2 ts = MeasureTextEx(mainFont, "Вы прошли все 3 уровня!", 50.0f * ui_scale, 2.0f);
                DrawTextEx(mainFont, "Вы прошли все 3 уровня!", { cx - ts.x / 2.0f, 120.0f * ui_scale }, 50.0f * ui_scale, 2.0f, GREEN);
                Vector2 ts2 = MeasureTextEx(mainFont, "Поздравляем!", 40.0f * ui_scale, 2.0f);
                DrawTextEx(mainFont, "Поздравляем!", { cx - ts2.x / 2.0f, 180.0f * ui_scale }, 40.0f * ui_scale, 2.0f, YELLOW);
                ts = MeasureTextEx(mainFont, "Нажмите Enter для возврата в меню", 25.0f * ui_scale, 2.0f);
                DrawTextEx(mainFont, "Нажмите Enter для возврата в меню", { cx - ts.x / 2.0f, 400.0f * ui_scale }, 25.0f * ui_scale, 2.0f, LIGHTGRAY);
            }
        }

        // Статистика
        else if (tekushiy_ekran == EKRAN_STATISTIKA) {
            DrawRectangle(0, 0, okno_shirina, okno_vysota, Color{ 60, 60, 70, 255 });
            Vector2 ts = MeasureTextEx(mainFont, "СТАТИСТИКА", 50.0f * ui_scale, 2.0f);
            DrawTextEx(mainFont, "СТАТИСТИКА", { okno_shirina / 2.0f - ts.x / 2.0f, 40.0f * ui_scale }, 50.0f * ui_scale, 2.0f, SKYBLUE);
            float sy = 120.0f * ui_scale, rowH = 40.0f * ui_scale, fs = 20.0f * ui_scale;
            float dateX = 100.0f * ui_scale;
            float maxDateWidth = MeasureTextEx(mainFont, "07.06.2026 20:08:18", fs, 2.0f).x;
            DrawTextEx(mainFont, "Вход", { dateX + (maxDateWidth - MeasureTextEx(mainFont, "Вход", fs, 2.0f).x) / 2.0f, sy }, fs, 2.0f, ORANGE);
            DrawTextEx(mainFont, "Наиграно", { 350.0f * ui_scale, sy }, fs, 2.0f, ORANGE);
            DrawTextEx(mainFont, "Очки", { 500.0f * ui_scale, sy }, fs, 2.0f, ORANGE);
            DrawTextEx(mainFont, "Уровни", { 600.0f * ui_scale, sy }, fs, 2.0f, ORANGE);
            DrawTextEx(mainFont, "Жизни", { 720.0f * ui_scale, sy }, fs, 2.0f, ORANGE);
            DrawLine(80.0f * ui_scale, sy + 30.0f * ui_scale, okno_shirina - 80.0f * ui_scale, sy + 30.0f * ui_scale, LIGHTGRAY);
            float listStartY = sy + 40.0f * ui_scale;
            float listHeight = okno_vysota - listStartY - 100.0f * ui_scale;
            int maxVisible = (int)(listHeight / rowH);
            if (maxVisible < 1) maxVisible = 1;
            int maxScroll = stats.count - maxVisible;
            if (maxScroll < 0) maxScroll = 0;
            if (statsScroll > maxScroll) statsScroll = maxScroll;

            int wheel = GetMouseWheelMove();
            if (wheel != 0) {
                statsScroll -= wheel;
                if (statsScroll < 0) statsScroll = 0;
                if (statsScroll > maxScroll) statsScroll = maxScroll;
            }
            if (IsKeyPressed(KEY_UP)) { statsScroll--; if (statsScroll < 0) statsScroll = 0; }
            if (IsKeyPressed(KEY_DOWN)) { statsScroll++; if (statsScroll > maxScroll) statsScroll = maxScroll; }

            // Вывод записей статистики
            for (int i = statsScroll; i < stats.count && i < statsScroll + maxVisible; i++) {
                float ry = listStartY + (i - statsScroll) * rowH;
                char timeStr[64];
                struct tm* tinfo = localtime(&stats.attempts[i].start_time);
                strftime(timeStr, sizeof(timeStr), "%d.%m.%Y %H:%M:%S", tinfo);
                DrawTextEx(mainFont, timeStr, { dateX, ry }, fs, 2.0f, WHITE);
                int m = stats.attempts[i].play_time_seconds / 60;
                int sec = stats.attempts[i].play_time_seconds % 60;
                char durStr[16]; snprintf(durStr, sizeof(durStr), "%02d:%02d", m, sec);
                DrawTextEx(mainFont, durStr, { 350.0f * ui_scale, ry }, fs, 2.0f, WHITE);
                char b[16];
                snprintf(b, sizeof(b), "%d", stats.attempts[i].total_points); DrawTextEx(mainFont, b, { 500.0f * ui_scale, ry }, fs, 2.0f, WHITE);
                snprintf(b, sizeof(b), "%d", stats.attempts[i].levels_passed); DrawTextEx(mainFont, b, { 600.0f * ui_scale, ry }, fs, 2.0f, WHITE);
                snprintf(b, sizeof(b), "%d", stats.attempts[i].lives_lost); DrawTextEx(mainFont, b, { 720.0f * ui_scale, ry }, fs, 2.0f, WHITE);
            }

            // Полоса прокрутки
            if (stats.count > maxVisible) {
                float trackX = okno_shirina - 30.0f * ui_scale;
                float trackY = listStartY;
                float trackH = listHeight;
                float thumbH = trackH * ((float)maxVisible / stats.count);
                if (thumbH < 15.0f * ui_scale) thumbH = 15.0f * ui_scale;
                float thumbY = trackY;
                if (maxScroll > 0) {
                    thumbY = trackY + ((float)statsScroll / maxScroll) * (trackH - thumbH);
                    if (thumbY > trackY + trackH - thumbH) thumbY = trackY + trackH - thumbH;
                    if (thumbY < trackY) thumbY = trackY;
                }
                DrawRectangleRounded({ trackX, trackY, 15.0f * ui_scale, trackH }, 0.2f, 8, DARKGRAY);
                DrawRectangleRounded({ trackX, thumbY, 15.0f * ui_scale, thumbH }, 0.2f, 8, GRAY);
            }

            DrawVolumeButton(mainFont, ui.s_txt, "Сохранить статистику в файл", mishka, ui_scale, CheckCollisionPointRec(mishka, ui.s_txt), DARKGREEN, GREEN, Color{ 0, 100, 0, 255 });
            DrawVolumeButton(mainFont, ui.s_reset, "Сброс", mishka, ui_scale, CheckCollisionPointRec(mishka, ui.s_reset), Color{ 150, 0, 0, 255 }, Color{ 255, 50, 50, 255 }, Color{ 100, 0, 0, 255 });
            DrawVolumeButton(mainFont, ui.nazad, "Назад", mishka, ui_scale, CheckCollisionPointRec(mishka, ui.nazad));
        }

        // Об авторе
        else if (tekushiy_ekran == EKRAN_AVTOR) {
            if (texObAvtoreBg.id) DrawTexturePro(texObAvtoreBg, { 0,0,(float)texObAvtoreBg.width,(float)texObAvtoreBg.height }, { 0,0,(float)okno_shirina,(float)okno_vysota }, { 0,0 }, 0, WHITE);
            else DrawRectangle(0, 0, okno_shirina, okno_vysota, DARKGRAY);
            Color semiBlack = { 0, 0, 0, 200 };
            float cx = okno_shirina / 2.0f;

            Rectangle block1 = { cx - 450.0f * ui_scale, 40.0f * ui_scale, 900.0f * ui_scale, 120.0f * ui_scale };
            DrawRectangleRounded(block1, 0.2f, 8, semiBlack);
            DrawRectangleRoundedLinesEx(block1, 0.2f, 8, 2.0f, ORANGE);
            DrawTextEx(mainFont, "Об авторе", { cx - MeasureTextEx(mainFont, "Об авторе", 32.0f * ui_scale, 2.0f).x / 2.0f, 55.0f * ui_scale }, 32.0f * ui_scale, 2.0f, ORANGE);
            DrawTextEx(mainFont, "Автором этой прекрасной игры является Карп Вадим Дмитриевич", { cx - MeasureTextEx(mainFont, "Автором этой прекрасной игры является Карп Вадим Дмитриевич", 18.0f * ui_scale, 2.0f).x / 2.0f, 100.0f * ui_scale }, 18.0f * ui_scale, 2.0f, SKYBLUE);
            DrawTextEx(mainFont, "из Гродненского государственного политехнического колледжа и группы ПЗТ - 45", { cx - MeasureTextEx(mainFont, "из Гродненского государственного политехнического колледжа и группы ПЗТ - 45", 16.0f * ui_scale, 2.0f).x / 2.0f, 130.0f * ui_scale }, 16.0f * ui_scale, 2.0f, WHITE);

            Rectangle block2 = { cx - 450.0f * ui_scale, 180.0f * ui_scale, 900.0f * ui_scale, 220.0f * ui_scale };
            DrawRectangleRounded(block2, 0.2f, 8, semiBlack);
            DrawRectangleRoundedLinesEx(block2, 0.2f, 8, 2.0f, GREEN);
            DrawTextEx(mainFont, "Об игре", { cx - MeasureTextEx(mainFont, "Об игре", 32.0f * ui_scale, 2.0f).x / 2.0f, 195.0f * ui_scale }, 32.0f * ui_scale, 2.0f, GREEN);
            DrawTextEx(mainFont, "Моя игра называется \"Зона испытаний\". Это динамичный 2D-платформер,", { cx - MeasureTextEx(mainFont, "Моя игра называется \"Зона испытаний\". Это динамичный 2D-платформер,", 15.0f * ui_scale, 2.0f).x / 2.0f, 240.0f * ui_scale }, 15.0f * ui_scale, 2.0f, WHITE);
            DrawTextEx(mainFont, "где нужно прыгать по платформам и выживать. Я сам прописывал физику", { cx - MeasureTextEx(mainFont, "где нужно прыгать по платформам и выживать. Я сам прописывал физику", 15.0f * ui_scale, 2.0f).x / 2.0f, 265.0f * ui_scale }, 15.0f * ui_scale, 2.0f, WHITE);
            DrawTextEx(mainFont, "движений, настраивал логику игры и собирал уровни так, чтобы играть", { cx - MeasureTextEx(mainFont, "движений, настраивал логику игры и собирал уровни так, чтобы играть", 15.0f * ui_scale, 2.0f).x / 2.0f, 290.0f * ui_scale }, 15.0f * ui_scale, 2.0f, WHITE);
            DrawTextEx(mainFont, "было действительно интересно и непросто. Внутри тебя ждут двигающиеся", { cx - MeasureTextEx(mainFont, "было действительно интересно и непросто. Внутри тебя ждут двигающиеся", 15.0f * ui_scale, 2.0f).x / 2.0f, 315.0f * ui_scale }, 15.0f * ui_scale, 2.0f, WHITE);
            DrawTextEx(mainFont, "платформы, опасная лава, сбор бонусов и детальная статистика", { cx - MeasureTextEx(mainFont, "платформы, опасная лава, сбор бонусов и детальная статистика", 15.0f * ui_scale, 2.0f).x / 2.0f, 340.0f * ui_scale }, 15.0f * ui_scale, 2.0f, WHITE);
            DrawTextEx(mainFont, "твоих игровых попыток.", { cx - MeasureTextEx(mainFont, "твоих игровых попыток.", 15.0f * ui_scale, 2.0f).x / 2.0f, 365.0f * ui_scale }, 15.0f * ui_scale, 2.0f, WHITE);

            Rectangle block3 = { cx - 450.0f * ui_scale, 420.0f * ui_scale, 900.0f * ui_scale, 100.0f * ui_scale };
            DrawRectangleRounded(block3, 0.2f, 8, semiBlack);
            DrawRectangleRoundedLinesEx(block3, 0.2f, 8, 2.0f, LIME);
            DrawTextEx(mainFont, "На чём сделано?", { cx - MeasureTextEx(mainFont, "На чём сделано?", 32.0f * ui_scale, 2.0f).x / 2.0f, 435.0f * ui_scale }, 32.0f * ui_scale, 2.0f, LIME);
            DrawTextEx(mainFont, "Игру я написал на языке C++, а для отрисовки графики и работы со звуком", { cx - MeasureTextEx(mainFont, "Игру я написал на языке C++, а для отрисовки графики и работы со звуком", 16.0f * ui_scale, 2.0f).x / 2.0f, 480.0f * ui_scale }, 16.0f * ui_scale, 2.0f, WHITE);
            DrawTextEx(mainFont, "использовал библиотеку Raylib. Весь код собран в Microsoft Visual Studio.", { cx - MeasureTextEx(mainFont, "использовал библиотеку Raylib. Весь код собран в Microsoft Visual Studio.", 16.0f * ui_scale, 2.0f).x / 2.0f, 505.0f * ui_scale }, 16.0f * ui_scale, 2.0f, WHITE);

            Rectangle block4 = { cx - 450.0f * ui_scale, 540.0f * ui_scale, 900.0f * ui_scale, 50.0f * ui_scale };
            DrawRectangleRounded(block4, 0.2f, 8, semiBlack);
            DrawTextEx(mainFont, "Гродно, 2026 г.", { cx - MeasureTextEx(mainFont, "Гродно, 2026 г.", 20.0f * ui_scale, 2.0f).x / 2.0f, 555.0f * ui_scale }, 20.0f * ui_scale, 2.0f, GRAY);

            DrawVolumeButton(mainFont, ui.nazad, "Назад", mishka, ui_scale, CheckCollisionPointRec(mishka, ui.nazad));
            if (CheckCollisionPointRec(mishka, ui.nazad) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                tekushiy_ekran = EKRAN_MENU;
                if (soundEnabled) PlaySound(buttonSound);
            }
        }

        // Настройки (отрисовка)
        else if (tekushiy_ekran == EKRAN_NASTROYKI) {
            if (bgTextures[currentBgIndex].id) DrawTexturePro(bgTextures[currentBgIndex], { 0,0,(float)bgTextures[currentBgIndex].width,(float)bgTextures[currentBgIndex].height }, { 0,0,(float)okno_shirina,(float)okno_vysota }, { 0,0 }, 0, WHITE);
            else DrawRectangle(0, 0, okno_shirina, okno_vysota, DARKGRAY);
            DrawRectangle(0, 0, okno_shirina, okno_vysota, ColorAlpha(BLACK, 0.6f));
            Vector2 ts = MeasureTextEx(mainFont, "НАСТРОЙКИ", 50.0f * ui_scale, 2.0f);
            DrawTextEx(mainFont, "НАСТРОЙКИ", { okno_shirina / 2.0f - ts.x / 2.0f, 60.0f * ui_scale }, 50.0f * ui_scale, 2.0f, SKYBLUE);
            float sx = 150.0f * ui_scale, zx = sx + 350.0f * ui_scale, sy = 200.0f * ui_scale, shy = 80.0f * ui_scale, fs = 28.0f * ui_scale;
            DrawTextEx(mainFont, "РАЗРЕШЕНИЕ:", { sx, sy }, fs, 2.0f, vybrannyy_punkt_nastroek == 0 ? MAGENTA : SKYBLUE);
            char r_txt[64]; snprintf(r_txt, sizeof(r_txt), "%dx%d", razresheniya[vyb_razr][0], razresheniya[vyb_razr][1]);
            DrawTextEx(mainFont, r_txt, { zx, sy }, fs, 2.0f, vybrannyy_punkt_nastroek == 0 ? LIME : GRAY);
            if (vybrannyy_punkt_nastroek == 0) {
                DrawTextEx(mainFont, "<", { zx - 35.0f * ui_scale, sy }, fs, 2.0f, WHITE);
                DrawTextEx(mainFont, ">", { zx + MeasureTextEx(mainFont, r_txt, fs, 2.0f).x + 10.0f * ui_scale, sy }, fs, 2.0f, WHITE);
            }
            DrawTextEx(mainFont, "СЛОЖНОСТЬ:", { sx, sy + shy }, fs, 2.0f, vybrannyy_punkt_nastroek == 1 ? MAGENTA : SKYBLUE);
            const char* slozhN[] = { "ЛЁГКАЯ", "НОРМАЛЬНАЯ", "СЛОЖНАЯ" };
            DrawTextEx(mainFont, slozhN[tekushaya_slozhnost], { zx, sy + shy }, fs, 2.0f, vybrannyy_punkt_nastroek == 1 ? LIME : GRAY);
            if (vybrannyy_punkt_nastroek == 1) {
                DrawTextEx(mainFont, "<", { zx - 35.0f * ui_scale, sy + shy }, fs, 2.0f, WHITE);
                DrawTextEx(mainFont, ">", { zx + MeasureTextEx(mainFont, slozhN[tekushaya_slozhnost], fs, 2.0f).x + 10.0f * ui_scale, sy + shy }, fs, 2.0f, WHITE);
            }
            DrawTextEx(mainFont, "ЗВУК:", { sx, sy + shy * 2 }, fs, 2.0f, vybrannyy_punkt_nastroek == 2 ? MAGENTA : SKYBLUE);
            Vector2 ss = MeasureTextEx(mainFont, soundEnabled ? "ВКЛ" : "ВЫКЛ", fs, 2.0f);
            Rectangle sBtn = { zx, sy + shy * 2 - 15.0f * ui_scale, ss.x + 30.0f * ui_scale, 40.0f * ui_scale };
            DrawVolumeButton(mainFont, sBtn, soundEnabled ? "ВКЛ" : "ВЫКЛ", mishka, ui_scale, CheckCollisionPointRec(mishka, sBtn), DARKBLUE, BLUE, Color{ 0,0,100,255 });
            DrawVolumeButton(mainFont, ui.save_settings_btn, "СОХРАНИТЬ", mishka, ui_scale, CheckCollisionPointRec(mishka, ui.save_settings_btn), DARKBLUE, BLUE, Color{ 0,0,100,255 }, WHITE);
            DrawVolumeButton(mainFont, ui.set_back, "НАЗАД", mishka, ui_scale, CheckCollisionPointRec(mishka, ui.set_back), DARKGRAY, LIGHTGRAY, GRAY);
        }

        // Все остальные экраны (меню, выбор уровня, управление, игра)
        else if (tekushiy_ekran != EKRAN_ZAGRUZKA) {
            if (bgTextures[currentBgIndex].id) DrawTexturePro(bgTextures[currentBgIndex], { 0,0,(float)bgTextures[currentBgIndex].width,(float)bgTextures[currentBgIndex].height }, { 0,0,(float)okno_shirina,(float)okno_vysota }, { 0,0 }, 0, WHITE);
            else DrawRectangle(0, 0, okno_shirina, okno_vysota, DARKGRAY);

            // Главное меню
            if (tekushiy_ekran == EKRAN_MENU) {
                Vector2 ts = MeasureTextEx(mainFont, "Зона испытаний", 40.0f * ui_scale, 2.0f);
                DrawTextEx(mainFont, "Зона испытаний", { okno_shirina / 2.0f - ts.x / 2.0f + 3.0f, 50.0f * ui_scale + 3.0f }, 40.0f * ui_scale, 2.0f, ColorAlpha(BLACK, 0.7f));
                DrawTextEx(mainFont, "Зона испытаний", { okno_shirina / 2.0f - ts.x / 2.0f, 50.0f * ui_scale }, 40.0f * ui_scale, 2.0f, ORANGE);
                const char* lbls[] = { "Играть", "Выбор уровня", "Управление", "Настройки", "Статистика", "Об авторе", "Выход" };
                Rectangle btns[] = { ui.igrat, ui.vybor, ui.upr, ui.nastroyki, ui.statistika, ui.avtor, ui.exit };
                for (int i = 0; i < 7; i++) {
                    bool isHovered = CheckCollisionPointRec(mishka, btns[i]);
                    if (isHovered) vybrannaya_knopka = i;
                    bool forcedHover = (vybrannaya_knopka == i) || isHovered;
                    Color baseColor = (i == 6) ? Color{ 150, 0, 0, 255 } : Color{ 0, 0, 150, 255 };
                    Color hoverColor = (i == 6) ? Color{ 255, 50, 50, 255 } : Color{ 200, 0, 0, 255 };
                    Color pressColor = Color{ 100, 0, 0, 255 };
                    DrawVolumeButton(mainFont, btns[i], lbls[i], mishka, ui_scale, forcedHover, baseColor, hoverColor, pressColor);
                }
            }
            // Выбор уровня
            else if (tekushiy_ekran == EKRAN_VYBOR_UROVNYA) {
                Vector2 ts = MeasureTextEx(mainFont, "Выбор уровня", 40.0f * ui_scale, 2.0f);
                DrawTextEx(mainFont, "Выбор уровня", { okno_shirina / 2.0f - ts.x / 2.0f, 100.0f * ui_scale }, 40.0f * ui_scale, 2.0f, ORANGE);
                Rectangle u_btns[] = { ui.ur1, ui.ur2, ui.ur3 };
                const char* lbls[] = { "Уровень 1", "Уровень 2", "Уровень 3" };
                for (int i = 0; i < 3; i++) DrawVolumeButton(mainFont, u_btns[i], lbls[i], mishka, ui_scale, CheckCollisionPointRec(mishka, u_btns[i]), DARKGREEN, GREEN, Color{ 0,100,0,255 });
                DrawVolumeButton(mainFont, ui.nazad, "Назад", mishka, ui_scale, CheckCollisionPointRec(mishka, ui.nazad));
            }
            // Управление
            else if (tekushiy_ekran == EKRAN_UPRAVLENIE) {
                Vector2 ts = MeasureTextEx(mainFont, "Управление", 30.0f * ui_scale, 2.0f);
                DrawTextEx(mainFont, "Управление", { okno_shirina / 2.0f - ts.x / 2.0f, 70.0f * ui_scale }, 30.0f * ui_scale, 2.0f, WHITE);
                DrawRectangle(100.0f * ui_scale, 120.0f * ui_scale, okno_shirina - 200.0f * ui_scale, 400.0f * ui_scale, ColorAlpha(BLACK, 0.7f));
                DrawTextEx(mainFont, "Игрок:", { 140.0f * ui_scale, 140.0f * ui_scale }, 34.0f, 2.0f, GREEN);
                DrawTextEx(mainFont, "W A S D   или   Стрелки для управления\n\nВверх / W - прыжок", { 160.0f * ui_scale, 180.0f * ui_scale }, 20.0f * ui_scale, 2.0f, WHITE);
                DrawTextEx(mainFont, "Саботажник:", { 140.0f * ui_scale, 260.0f * ui_scale }, 34.0f, 2.0f, RED);
                DrawTextEx(mainFont, "Кнопки для управления\n\n1 - скорость движения игрока 0.5x\n\n2 - увеличение силы гравитации 1.5x\n\n3 - периодическое мигание экрана\n\n4 - инверсия клавиш управления", { 160.0f * ui_scale, 300.0f * ui_scale }, 20.0f * ui_scale, 2.0f, WHITE);
                DrawVolumeButton(mainFont, ui.nazad, "Назад", mishka, ui_scale, CheckCollisionPointRec(mishka, ui.nazad));
                if (CheckCollisionPointRec(mishka, ui.nazad) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    tekushiy_ekran = EKRAN_MENU;
                    if (soundEnabled) PlaySound(buttonSound);
                }
            }
            // Игра/пауза (отрисовка игрового мира)
            else if (tekushiy_ekran == EKRAN_IGRA || tekushiy_ekran == EKRAN_PAUZA) {
                BeginMode2D(kamera);
                if (texLava.id) DrawTexturePro(texLava, { 0, 0, (float)texLava.width, (float)texLava.height }, { -5000, 640, 25000, 300 }, { 0,0 }, 0, WHITE);
                else DrawRectangle(-5000, 640, 25000, 300, RED);
                for (int i = 0; i < tek_koll_plat; i++) {
                    DrawTexturePro(texPlatform, { 0,0,(float)texPlatform.width,(float)texPlatform.height }, tek_plat[i].forma, { 0,0 }, 0, WHITE);
                }
                for (int i = 0; i < tek_koll_toch; i++) {
                    if (tek_tochki[i].aktivna) {
                        DrawCircleV(tek_tochki[i].poziciya, 10.0f, ColorAlpha(tek_tochki[i].bonus_time ? GREEN : BLUE, 0.7f));
                        DrawCircleV(tek_tochki[i].poziciya, 6.0f, tek_tochki[i].bonus_time ? DARKGREEN : DARKBLUE);
                    }
                }

                // Цвет персонажа зависит от активных эффектов (визуальная подсказка)
                Color pColor = WHITE;
                if (igrok.hit_flash > 0.5f) pColor = RED;
                if (igrok.effect1 > 0) pColor = BLUE;
                if (igrok.effect2 > 0) pColor = ORANGE;
                if (igrok.effect4 > 0) pColor = PURPLE;

                Rectangle srcRect = { 0.0f, 0.0f, (float)texPerson.width, (float)texPerson.height };
                bool facingLeft = igrok.skorost.x < 0;
                if (facingLeft) srcRect.width = -srcRect.width;

                float bob = sinf(GetTime() * 8.0f) * 2.0f;
                if (!igrok.na_zemle) bob = 0.0f;

                float animatedHeight = igrok.vysota * igrok.anim_scale_y;
                float visualWidth = igrok.shirina * 2.2f;
                float visualHeight = animatedHeight;

                float visualX = igrok.poziciya.x + (igrok.shirina - visualWidth) / 2.0f;
                float visualY = igrok.poziciya.y + (igrok.vysota - visualHeight) / 2.0f + bob;

                Rectangle visualRect = { visualX, visualY, visualWidth, visualHeight };
                Vector2 originZero = { 0, 0 };
                DrawTexturePro(texPerson, srcRect, visualRect, originZero, 0.0f, pColor);
                EndMode2D();

                // Эффект мигания экрана при эффекте 3
                if (igrok.effect3 > 0.0f && ((int)(igrok.effect3 * 2.0f) % 2 == 0)) DrawRectangle(0, 0, okno_shirina, okno_vysota, BLACK);

                DrawVolumeButton(mainFont, ui.pauza, "Пауза", mishka, ui_scale);

                char b[64];
                snprintf(b, sizeof(b), "Уровень: %d", tekushiy_uroven);
                DrawTextEx(mainFont, b, { 160.0f * ui_scale, 25.0f * ui_scale }, 20.0f * ui_scale, 2.0f, ORANGE);
                snprintf(b, sizeof(b), "Время: %.1f", vremya_taymer);
                DrawTextEx(mainFont, b, { 310.0f * ui_scale, 25.0f * ui_scale }, 20.0f * ui_scale, 2.0f, SKYBLUE);
                snprintf(b, sizeof(b), "Очки: %d", ochki);
                DrawTextEx(mainFont, b, { 480.0f * ui_scale, 25.0f * ui_scale }, 20.0f * ui_scale, 2.0f, GREEN);
                DrawTextEx(mainFont, "Жизни:", { 600.0f * ui_scale, 25.0f * ui_scale }, 20.0f * ui_scale, 2.0f, WHITE);
                for (int i = 0; i < igrok.zhizni; i++) {
                    DrawTexturePro(texHeart, { 0,0,(float)texHeart.width,(float)texHeart.height },
                        { 700.0f * ui_scale + i * 35.0f * ui_scale, 20.0f * ui_scale, 28.0f * ui_scale, 28.0f * ui_scale },
                        { 0,0 }, 0, WHITE);
                }

                // Отрисовка панели саботажника (кнопки 1-4)
                float sp2 = 160.0f * ui_scale, vp2 = 280.0f * ui_scale, xp2 = 5.0f * ui_scale, yp2 = okno_vysota / 2.0f - vp2 / 2.0f;
                float sks2 = 130.0f * ui_scale, vks2 = 65.0f * ui_scale, ox2 = (sp2 - sks2) / 2.0f, pv2 = (vp2 - (vks2 * 4)) / 5.0f;
                const char* n[] = { "Замедление", "Гравитация", "Мигание", "Инверсия" };
                const char* o1[] = { "Скорость движения", "Увеличение силы", "Периодическое", "Инверсия клавиш" };
                const char* o2[] = { "игрока 0.5x", "гравитации 1.5x", "мигание экрана", "управления" };
                float* effs2[] = { &igrok.effect1, &igrok.effect2, &igrok.effect3, &igrok.effect4 };
                float* cds2[] = { &igrok.cooldown1, &igrok.cooldown2, &igrok.cooldown3, &igrok.cooldown4 };
                for (int i = 0; i < 4; i++) {
                    Rectangle r = { xp2 + ox2, yp2 + pv2 * (i + 1) + vks2 * i, sks2, vks2 };
                    bool isHovered = CheckCollisionPointRec(mishka, r);
                    Color btnColor;
                    if (*effs2[i] > 0) btnColor = YELLOW;
                    else if (*cds2[i] > 0) btnColor = RED;
                    else btnColor = GREEN;
                    if (isHovered && *cds2[i] <= 0 && *effs2[i] <= 0) btnColor = LIME;
                    DrawRectangleRec(r, btnColor);
                    char numTxt[2] = { (char)('1' + i), '\0' };
                    float numFontSize = 20.0f * ui_scale;
                    Vector2 numSize = MeasureTextEx(mainFont, numTxt, numFontSize, 2.0f);
                    Vector2 numPos = { r.x + r.width - numSize.x - 5.0f, r.y + r.height - numSize.y - 5.0f };
                    DrawTextEx(mainFont, numTxt, numPos, numFontSize, 2.0f, RED);
                    DrawTextEx(mainFont, n[i], { r.x + 5.0f, r.y + 8.0f }, 14.0f * ui_scale, 2.0f, BLACK);
                    DrawTextEx(mainFont, o1[i], { r.x + 5.0f, r.y + 32.0f }, 11.0f * ui_scale, 2.0f, BLACK);
                    DrawTextEx(mainFont, o2[i], { r.x + 5.0f, r.y + 48.0f }, 11.0f * ui_scale, 2.0f, BLACK);
                    if (*effs2[i] > 0 || *cds2[i] > 0) {
                        char txt[16];
                        snprintf(txt, sizeof(txt), "%.1fs", *effs2[i] > 0 ? *effs2[i] : *cds2[i]);
                        DrawTextEx(mainFont, txt, { r.x + r.width - 55.0f, r.y + 6.0f }, 13.0f * ui_scale, 2.0f, BLACK);
                    }
                }

                // Отображение стартового отсчёта (3..2..1..GO)
                if (v_otschete) {
                    const char* t = otschet_vremya > 1.5f ? "3" : (otschet_vremya > 1.0f ? "2" : (otschet_vremya > 0.5f ? "1" : "GO!"));
                    Vector2 tw = MeasureTextEx(mainFont, t, 80.0f * ui_scale, 2.0f);
                    DrawTextEx(mainFont, t, { okno_shirina / 2.0f - tw.x / 2.0f, okno_vysota / 2.0f - tw.y / 2.0f }, 80.0f * ui_scale, 2.0f, RED);
                }
                if (bonus_text_timer > 0) {
                    bonus_text_timer -= GetFrameTime();
                    Vector2 tw = MeasureTextEx(mainFont, "+10 секунд", 40.0f * ui_scale, 2.0f);
                    DrawTextEx(mainFont, "+10 секунд", { okno_shirina / 2.0f - tw.x / 2.0f, okno_vysota / 2.0f - tw.y / 2.0f }, 40.0f * ui_scale, 2.0f, GREEN);
                }

                // Если игра на паузе — затемнение и меню паузы
                if (tekushiy_ekran == EKRAN_PAUZA) {
                    DrawRectangle(0, 0, okno_shirina, okno_vysota, ColorAlpha(BLACK, 0.6f));
                    if (texPauseBg.id) DrawTexturePro(texPauseBg, { 0,0,(float)texPauseBg.width,(float)texPauseBg.height }, { 0,0,(float)okno_shirina,(float)okno_vysota }, { 0,0 }, 0, WHITE);
                    Vector2 ts2 = MeasureTextEx(mainFont, "Пауза", 50.0f * ui_scale, 2.0f);
                    DrawTextEx(mainFont, "Пауза", { okno_shirina / 2.0f - ts2.x / 2.0f, 120.0f * ui_scale }, 50.0f * ui_scale, 2.0f, WHITE);
                    DrawVolumeButton(mainFont, ui.p_resume, "Продолжить", mishka, ui_scale, CheckCollisionPointRec(mishka, ui.p_resume), DARKBLUE, BLUE, Color{ 0,0,100,255 });
                    DrawVolumeButton(mainFont, ui.p_restart, "Заново", mishka, ui_scale, CheckCollisionPointRec(mishka, ui.p_restart), DARKBLUE, BLUE, Color{ 0,0,100,255 });
                    DrawVolumeButton(mainFont, ui.p_menu, "Меню", mishka, ui_scale, CheckCollisionPointRec(mishka, ui.p_menu), DARKBLUE, BLUE, Color{ 0,0,100,255 });
                    char s[32]; snprintf(s, sizeof(s), "Звук: %s", soundEnabled ? "Вкл" : "Выкл");
                    DrawVolumeButton(mainFont, ui.p_sound, s, mishka, ui_scale, CheckCollisionPointRec(mishka, ui.p_sound), DARKBLUE, BLUE, Color{ 0,0,100,255 });
                }
            }
        }

        // Эффект затухания при смене экрана
        if (fadeAlpha > 0.0f) {
            DrawRectangle(0, 0, okno_shirina, okno_vysota, Fade(BLACK, fadeAlpha));
            fadeAlpha -= GetFrameTime() * 2.5f;
        }

        EndDrawing();
    }

    // Сохранение попытки при выходе (если была начата)
    if (attempt_start_game > 0) {
        int duration = (int)(GetTime() - attempt_start_game);
        SaveAttempt(stats, attempt_start_real, duration, ochki, attempt_levels, attempt_lives_lost);
    }

    // Очистка ресурсов
    for (int i = 0; i < BG_COUNT; i++) UnloadTexture(bgTextures[i]);
    UnloadTexture(texPlatform); UnloadTexture(texHeart); UnloadTexture(texPerson); UnloadTexture(texLava);
    UnloadTexture(texPauseBg); UnloadTexture(texGameOverBg); UnloadTexture(texObAvtoreBg);
    UnloadTexture(texPobedaBg); UnloadTexture(texZagruzkaBg);
    for (int i = 0; i < MUSIC_COUNT; i++) UnloadMusicStream(musicTracks[i]);
    UnloadSound(gameOverSound); UnloadSound(levelCompletedSound); UnloadSound(buttonSound);
    UnloadFont(mainFont);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}