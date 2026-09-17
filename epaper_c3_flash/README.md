# epaper_c3_flash - прошивка ESP32-C3 мимо Arduino IDE

Железо: ESP32-C3 Super Mini + WeAct Studio 2.9" (DEPG0290BS, 296x128),
драйвер-класс `GxEPD2_290_BS`. Плата поднимается как COM11.

Распиновка (из скетча, менять нельзя):

| Панель | ESP32-C3 GPIO |
|--------|---------------|
| SCK    | 4  |
| MOSI   | 6  |
| CS     | 7  |
| DC     | 1  |
| RST    | 10 |
| BUSY   | 3  |

Сам скетч лежит в `../epaper_web_uploader`, здесь только инструмент прошивки и
две диагностические прошивки.

## Прошить

```bash
python epaper_c3_flash/flash.py --erase
```

Собирает `../epaper_web_uploader` и заливает на COM11 со стёртой LittleFS.
Дальше `--erase` можно не передавать, если картинку на плате надо сохранить.
Весь вывод уходит в `flash.log`, в консоль печатается только хвост.

```bash
python epaper_c3_flash/flash.py epaper_clean --erase
```

```bash
python epaper_c3_flash/flash.py --port COM7 --compile-only
```

Имя скетча принимается как голое имя, путь от корня репозитория или абсолютный
путь - лишь бы в папке лежал `.ino`.

## Почему именно так, а не из IDE

Панель показывала снег после каждой прошивки. Это было не питание, не висящий
BUSY, не скорость SPI и не обновление библиотек. В LittleFS лежал битый
`/screen.bin`: IDE заливает с `EraseFlash=none`, поэтому картинка переживала
любую перепрошивку и `setup()` рисовал её заново. Помогла прошивка из
командной строки со стёртой файловой системой.

## Руками

Отдельно `arduino-cli` не установлен, он внутри Arduino IDE 2.3.10:

```
C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe
```

FQBN, ключ стирания в конце:

```
esp32:esp32:esp32c3:CDCOnBoot=default,CPUFreq=160,FlashFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=default,UploadSpeed=921600,EraseFlash=all
```

```
arduino-cli.exe compile --fqbn <FQBN> --build-cache-path .build-cache --build-path .build --upload --port COM11 --verify ../epaper_web_uploader
```

`--build-cache-path` обязателен, иначе ядро ESP32 пересобирается целиком каждый
раз, минут десять даже на крошечный скетч. У IDE свой тёплый кеш, до которого
CLI не дотягивается - отсюда и ощущение, что CLI абсурдно медленный.

Порт не должен быть занят: закрыть монитор порта в IDE перед запуском.

## После прошивки

1. На панели экран настройки: "E-Paper uploader", имя сети, пароль и
   `http://192.168.4.1`. Снег на этом шаге означает, что проблема ниже уровня
   приложения.
2. Монитор порта COM11 на 115200 (CDC включён, загрузочные сообщения видно):
   `LittleFS ready`, параметры точки доступа, `Stored bitmap is missing or invalid`.
3. Подключиться к `E-Paper-C3` / `epaper123`, открыть `http://192.168.4.1`,
   загрузить картинку.

Скетч около 1.13 МБ при партиции приложения 1.31 МБ, запас маленький.

## Диагностические прошивки

- `epaper_clean` - чистит панель в белое и останавливается. Ни Wi-Fi, ни
  LittleFS, SPI явно опущен до 2 МГц. Если и после неё снег, дело в проводах,
  SPI или классе драйвера.
- `epaper_text` - рисует текст и рамку, тот же минимальный путь.
