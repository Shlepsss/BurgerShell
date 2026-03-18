# DOS GUI Shell (320x200, Mode 13h)

Это не «отдельная ОС», а GUI-программа под DOS (real mode), которая использует:
- графику через BIOS/VGA Mode `13h`
- мышь через драйвер `INT 33h`
- файловую систему через DOS (обычный `fopen/fread/fwrite`)

## Сборка

В каталоге `D:\dosbox`:

- `botaiai.bat` собирает `shell.exe` (OpenWatcom, small model).

## Запуск

В DOS:

- `shell.exe`

Управление:
- верхняя панель: `Files`, `Calc`, `About`, `Exit`
- в `Files`: клик по файлу `.TXT`, затем `Enter` чтобы открыть
- в `Editor`: печать ASCII, `Backspace`, `Enter`, `Ctrl+S` — сохранить, `ESC` — закрыть
- в `Calc`: набери `12+34` и `Enter`

## Как запустить на реальном ПК без DOSBox

Самый простой легальный вариант — FreeDOS (он бесплатный и поддерживает `CTMOUSE`).

Идея такая:
1) Загрузиться в FreeDOS с дискеты/USB.
2) Скопировать `shell.exe` на диск (или на ту же дискету/USB).
3) Добавить автозапуск в `AUTOEXEC.BAT`.

### Вариант A: дискета 1.44MB
1) Сделай загрузочную дискету FreeDOS (boot disk image -> записать на дискету).
2) После загрузки FreeDOS скопируй `SHELL.EXE` на `A:\` (если влезает) или на `C:\SHELL\`.
3) Отредактируй `A:\AUTOEXEC.BAT` (или `C:\AUTOEXEC.BAT`) и добавь строку:
   - `SHELL.EXE`

Важно:
- для мыши должен быть загружен драйвер (`CTMOUSE` / `MOUSE`). Обычно FreeDOS грузит его сам, или это можно добавить в `AUTOEXEC.BAT`.

### Вариант B: USB (проще на современном ПК)
1) Сделай загрузочную флешку FreeDOS.
2) Положи туда `SHELL.EXE`.
3) В `AUTOEXEC.BAT` добавь запуск `SHELL.EXE`.

