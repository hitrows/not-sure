# Not Sure — статус для Cowork

**Проект:** аудиоплагин (агрессивный feedback-лимитер с charge-dependent
release, вдохновлён Shure Level-Loc — не клон). JUCE 8.0.15, форматы **AU + VST3**
на macOS (+ Standalone для прослушивания, в установщик не идёт). Автор пишет
по-русски, весь код/строки — по-английски. Рабочее правило: **всегда предлагать
перед действием и ждать явного «да»**.

**Путь проекта:** `/Users/hitrows/Developer/Not Sure`
**Версия:** 0.8.0 — **бета собрана и роздана друзьям** (`.pkg`, установка на
чистой машине проверена).

## Где мы сейчас — бета 0.8.0 в раздаче

- **Сборка/валидация зелёные:** `BUILD SUCCEEDED`, `AU VALIDATION SUCCEEDED`,
  `Component Version: 0.8.0`, 10 параметров. VST3 прошёл **pluginval strictness
  8 и 10** (редактор, автоматизация, state, bypass).
- **DSP (все стадии):** feedback-лимитер + charge-release (Sag), асимметричный
  вейвшейпер Crunch с оверсэмплингом вокруг него, Darkness (tilt), Auto gain.
  `Source/dsp/LimiterCore.{h,cpp}` — без единого `#include <juce...>`.
- **Кастомный редактор** по `UI-SPEC.md`: панель, 6 крутилок с живой риской,
  3 переключателя-тайла, лампа, тумблер bypass. Текст — в арте, tooltips.
- **9 пресетов папками** через `.aupreset` на диске (см. ниже).
- **`.pkg`-установщик** (AU + VST3 + пресеты), unsigned (см. ниже).

## Серия оптимизаций DSP (0.7.x)

Цель — легче/быстрее/лучше. Протокол каждой правки: diff → null-тест
(характер не должен двигаться) → Release-замер на `notsure-render`.
**Патч-версия инкрементится автоматически каждую итерацию** (правило в памяти).

| Пункт | Версия | Что | Результат |
| --- | --- | --- | --- |
| 1 | 0.7.0 | `fastTanh` ([7/6] Паде) вместо `std::tanh` в шейпере+softCeiling | ~11% CPU; null −64 dB |
| 2B | — | быстрые log/exp в детекторе | измерено, **откачено** (детектор на базовой частоте → 0 выигрыша на Apple Silicon) |
| 4 | 0.7.1 | оверсэмплер: кольцевой буфер вместо сдвигов + свёртка по симметрии (24→12 умн.) | ~15% CPU; null −128 dB (бит-в-бит) |
| 5 | 0.7.2 | сглаживание применяемых гейнов (drive, crunch, darkness, autogain, mix, trim), one-pole ~20 мс | нет zipper/щелчков; steady-state null −128 dB |
| 3 | 0.7.3 | `setParams` пропускает пересчёт коэффициентов, если параметры не менялись | звук бит-в-бит; в DAW убран per-block холостой пересчёт |

Замер: тяжёлый 4x-тракт **~51 → ~45 мс** в рендере. Важно: выигрыши видны
только в **Release** (-O3+LTO); в Debug `-O0` библиотечный код обгоняет
аппроксимации. Пункт 3 в рендере не проявляется (там `setParams` зовётся
один раз), только в живом хосте.

Ключевые детали реализации:
- `fastTanh`/сглаживание/dirty-флаг живут в `LimiterCore` (JUCE-free, чтобы
  `notsure-render` слышал ровно то же, что плагин).
- Сглаживатели праймятся на таргеты в `reset()` (нет паразитного разгона на
  старте воспроизведения), а смена пресета/ручки мид-плейбек честно рампится.
- `Params::operator==` через `= default` (C++20) — обходит `-Wfloat-equal`.
- Латентность оверсэмплера не изменилась — хост ничего не переравнивает.

**Что осталось по CPU:** по сути нечего — два горячих места (шейпер и
халфбенд, оба на 4x) закрыты; детектор на базовой частоте не бутылочное горло.
Дальнейшее — только вариант A пункта 2 (перевод гейн-компьютера в линейный
домен), но он рискует формой release и требует отдельного null-теста по огибающей.

## Пресеты — папками через .aupreset (НЕ AU factory)

Logic **не разбирает «/» в имени** на папки. Настоящие папки — у `.aupreset`
файлов на диске под `~/Library/Audio/Presets/Hitrows/Not Sure/<категория>/`,
видны в меню **«Settings»** (▾ вверху окна плагина).

- **AU factory-программы отключены** (`getNumPrograms()==0`) — убирает плоское
  меню «AU Presets». auval: секция factory presets пустая, валидация цела.
- **9 `.aupreset` по папкам** генератором `tools/make-presets.mm`:
  `Default` (верх), `Drums/` (Room Crush, Snare Fatten, Bus Glue, Loop Destroy),
  `Bass/Weight`, `Vocal/` (Grit, sila), `Keys/Synth Thicken`.
- Генератор берёт шаблон `ClassInfo` из AU и подменяет `jucePluginState`
  собранным вручную состоянием с **фактическими** значениями; формат блоба из
  исходника JUCE (`copyXmlToBinary`: `[LE magic 0x21324356][LE len][XML][0]`).
  Не зависит от AU-параметров (те нормализованы 0..1 + скью — «сырые» значения
  ломают середину; был баг, исправлен). Источник значений — `Source/Presets.h`.
- Перегенерация:
  ```sh
  clang++ -std=c++17 -fobjc-arc -ObjC++ tools/make-presets.mm \
    -framework AudioToolbox -framework AudioUnit \
    -framework Foundation -framework CoreFoundation -o /tmp/make-presets
  /tmp/make-presets
  ```

## Полная сборка / валидация

```sh
/opt/homebrew/bin/cmake -B build -G Xcode
/opt/homebrew/bin/cmake --build build --config Debug
auval -v aufx Nsur Htrw
```
Оффлайн-рендерер (тюнинг по слуху / null-тесты): `./build/tools/notsure-render
loop.wav out.wav --crush 7 --crunch 5 --sag 9`. Для CPU-замеров собирать
`--config Release --target notsure-render`.

## Форматы и установщик (0.8.0)

- **Форматы:** `FORMATS AU VST3 Standalone` в CMakeLists. VST3 добавлен в 0.8.0
  и проверен pluginval (strictness 8/10). Standalone — только для локального
  прослушивания, в установщик не идёт. Валидатор: `brew install --cask
  pluginval`, затем `pluginval --strictness-level 10 --validate <.vst3>`.
- **Установщик:** `tools/build-installer.sh` одной командой собирает
  `dist/NotSure-0.8.0.pkg` (Release, **unsigned**). Внутри два компонента через
  `pkgbuild` + `productbuild`:
  - плагины → **системная** `/Library/Audio/Plug-Ins/{Components,VST3}`;
  - 9 `.aupreset` → **пользовательская** `~/Library/Audio/Presets/Hitrows/Not
    Sure/` через **postinstall** (system-домен не может писать в `$HOME`;
    скрипт берёт console-user через `stat -f%Su /dev/console` и `chown`).
  - `distribution.xml`: title, readme-панель из `README-BETA.md`, macOS ≥ 10.15,
    без license/выбора пути. Standalone НЕ ставится.
- **Грабли установщика:** pkgbuild externализует `com.apple.provenance` как
  AppleDouble `._*` в BOM (его `xattr -cr` не снимает — защищён), но Installer
  их мёржит — на целевой машине литеральных `._` нет (проверено
  `pkgutil --expand-full`). Плагины в системной папке — uninstall/`xattr` в
  README идут через `sudo`.
- **Подпись:** сейчас unsigned (бета); README учит обход Gatekeeper. Задел под
  Developer ID + notarytool закомментирован в конце `build-installer.sh`.

## Параметры (10)

`crush`, `crunch`, `sag`, `darkness`, `mix`, `trim`, `autogain`, `attack`
(0.3/1.3/4.0 ms), `oversampling` (1x/2x/4x), `bypass`.

## Грабли (не повторять)

- Проект и `build/` вне iCloud/`~/Documents` — иначе codesign падает на FinderInfo.
- `cmake` не в PATH — только `/opt/homebrew/bin/cmake`; нужен полный Xcode.
- CPU-замеры только в **Release**; Debug вводит в заблуждение.
- Идентичность заморожена: `PLUGIN_CODE=Nsur`, `MANUFACTURER_CODE=Htrw`,
  `BUNDLE_ID=com.hitrows.notsure`, JUCE pinned 8.0.15.
- Параметр-ID в `Source/Parameters.h` — вечные (ломают recall сессий/`.aupreset`).
- AU-параметры JUCE нормализованы 0..1 (+скью) — для пресетов писать состояние
  напрямую, не через `AudioUnitSetParameter`.
- Logic сканит AU только при запуске: после пересборки
  `killall -9 AudioComponentRegistrar` + полный ⌘Q Logic.
- Имена ресурсов в `BinaryData` — без дефисов/подчёркиваний.
- `.aupreset` идут в **пользовательскую** папку (Logic только там их видит) —
  установщик кладёт их туда через postinstall, плагины отдельно в системную.

## Следующие шаги

1. Собрать фидбэк бета-тестеров (в README просят: падения, несохранение
   параметров, пресеты, щелчки при автоматизации).
2. Обновить CLAUDE.md (устарело: «getNumPrograms stays at 1», свой
   preset-браузер, «AU only»).
3. Мелочь VST3/AU: латентность репортится нулём на `prepareToPlay` (оверсэмплер
   получает фактор в первом `processBlock`); можно выставлять сразу в prepare.
4. Если купят Developer ID — раскомментировать блок подписи/нотаризации в
   `build-installer.sh`.
5. Опционально — пункт 2A (линейный гейн-компьютер) с null-тестом по release.
