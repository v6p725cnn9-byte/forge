#pragma once

#include <string_view>

namespace forge::app {

inline const char* tr(std::string_view language, std::string_view key)
{
    const bool ru = language != "en";
    if (key == "title") return ru ? "FORGE" : "FORGE";
    if (key == "subtitle") return ru ? "Выживание" : "Survival";
    if (key == "single") return ru ? "Одиночная игра" : "Single player";
    if (key == "multi") return ru ? "Мультиплеер" : "Multiplayer";
    if (key == "settings") return ru ? "Настройки" : "Settings";
    if (key == "quit") return ru ? "Выход" : "Quit";
    if (key == "back") return ru ? "Назад" : "Back";
    if (key == "host") return ru ? "Создать сессию" : "Host session";
    if (key == "join") return ru ? "Присоединиться" : "Join";
    if (key == "address") return ru ? "Адрес (ip:порт)" : "Address (ip:port)";
    if (key == "port") return ru ? "Порт" : "Port";
    if (key == "graphics") return ru ? "Графика" : "Graphics";
    if (key == "controls") return ru ? "Управление" : "Controls";
    if (key == "sound") return ru ? "Звук" : "Sound";
    if (key == "language") return ru ? "Язык" : "Language";
    if (key == "resolution") return ru ? "Разрешение" : "Resolution";
    if (key == "fullscreen") return ru ? "Полный экран" : "Fullscreen";
    if (key == "vsync") return ru ? "Вертикальная синхронизация" : "VSync";
    if (key == "apply") return ru ? "Применить" : "Apply";
    if (key == "sensitivity") return ru ? "Чувствительность мыши" : "Mouse sensitivity";
    if (key == "invert_y") return ru ? "Инверсия оси Y" : "Invert Y axis";
    if (key == "master") return ru ? "Общая громкость" : "Master";
    if (key == "music") return ru ? "Музыка" : "Music";
    if (key == "sfx") return ru ? "Эффекты" : "Effects";
    if (key == "audio_note")
        return ru ? "Микшер сохранён. Воспроизведение звука — следующим слоем."
                  : "Mixer is saved. Playback comes in a later pass.";
    if (key == "bind_move") return ru ? "Движение  WASD" : "Move  WASD";
    if (key == "bind_look") return ru ? "Обзор  ПКМ" : "Look  RMB";
    if (key == "bind_run") return ru ? "Бег  Shift" : "Run  Shift";
    if (key == "bind_use") return ru ? "Добыча / extract  E" : "Gather / extract  E";
    if (key == "bind_craft") return ru ? "Костёр  C" : "Campfire  C";
    if (key == "bind_menu") return ru ? "Меню  Esc" : "Menu  Esc";
    if (key == "hud_session") return ru ? "Сессия" : "Session";
    if (key == "hud_night") return ru ? "НОЧЬ" : "NIGHT";
    if (key == "hud_day") return ru ? "День" : "Day";
    if (key == "hud_extracted") return ru ? "ЭВАКУАЦИЯ" : "EXTRACTED";
    if (key == "hud_failed") return ru ? "ПРОВАЛ" : "FAILED";
    if (key == "hud_drop") return ru ? "высадка" : "drop";
    if (key == "hud_help") return ru ? "F шаг  Shift бег  Ctrl присед  C ползти  Space прыжок  B костёр  E действие"
                                     : "F walk  Shift sprint  Ctrl crouch  C crawl  Space jump  B fire  E interact";
    if (key == "wood") return ru ? "Дерево" : "Wood";
    if (key == "stone") return ru ? "Камень" : "Stone";
    if (key == "cold") return ru ? "Холод" : "Cold";
    if (key == "hp") return ru ? "HP" : "HP";
    return key.data();
}

} // namespace forge::app
