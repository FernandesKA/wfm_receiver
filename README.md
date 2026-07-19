# wfm_receiver

Приём, демодуляция и вывод звука широкополосного FM (WFM) через SDR.

Полный тракт работает end-to-end: приём IQ с HackRF или PlutoSDR → канальная
фильтрация и децимация → FM-демодуляция → de-emphasis → ресемплинг под
частоту звуковой карты → воспроизведение через ALSA.

## Возможности

- Приём IQ-потока с **HackRF One** и **PlutoSDR (ADALM-PLUTO)** через единый
  интерфейс `hardware::signal_source` (асинхронный колбэк с сырыми байтами).
- Полный DSP-тракт демодуляции WFM: канальный фильтр + децимация
  (equiripple FIR), FM-дискриминатор, de-emphasis, финальный ресемплинг.
- Вывод звука в реальном времени через ALSA (mono, 48 кГц).
- Конфигурация источника сигнала из JSON-файла + переопределение параметров
  через аргументы командной строки (Boost.ProgramOptions).
- Архитектура рассчитана на несколько источников сигнала
  (`config::signal_source_type`); сейчас поддержаны HackRF и PlutoSDR.

## Зависимости

- CMake >= 3.16, компилятор с поддержкой C++17
- [libhackrf](https://github.com/greatscottgadgets/hackrf) (`libhackrf-dev`)
- [libiio](https://github.com/analogdevicesinc/libiio) (`libiio-dev`) — для PlutoSDR
- ALSA (`libasound2-dev`) — вывод звука
- Boost >= 1.75, компоненты `json` и `program_options`
  (`libboost-json-dev`, `libboost-program-options-dev`)

На Ubuntu/Debian:

```bash
sudo apt install cmake libhackrf-dev libiio-dev libasound2-dev \
    libboost-json-dev libboost-program-options-dev
```

## Сборка

```bash
cmake -B build -S .
cmake --build build
```

По умолчанию собирается в `Release` (DSP-код в `Debug`/без оптимизаций на
порядок медленнее реального времени — этого достаточно, чтобы приём начал
отставать и звук стал прерывистым). Явно задать другой тип сборки можно
через `-DCMAKE_BUILD_TYPE=...`.

Собираются:
- `libwfm_hw.a` — статическая библиотека (приём с HackRF/PlutoSDR, DSP-тракт,
  аудиовывод, парсинг конфига)
- `wfm_receiver` — исполняемый приёмник

## Запуск

```bash
./build/wfm_receiver --config configs/pluto_signal_source.json --frequency 103400000
```

Без `--config` по умолчанию используется `configs/pluto_signal_source.json`.
Перед стартом приложение печатает итоговые параметры (после применения
JSON + CLI-оверрайдов), затем начинает приём и сразу проигрывает звук.
Остановка — `Ctrl+C`.

### Аргументы командной строки

| Флаг                 | Описание                                              |
|----------------------|--------------------------------------------------------|
| `-c, --config <path>`| путь к JSON-конфигу (по умолчанию `configs/pluto_signal_source.json`) |
| `-f, --frequency`    | центральная частота, Гц                                |
| `--lna-gain`         | HackRF: LNA gain, дБ (0-40, шаг 8)                     |
| `--vga-gain`         | HackRF: VGA gain, дБ (0-62, шаг 2)                     |
| `--amp-enable`       | HackRF: включить фронтенд-усилитель (true/false)        |
| `--serial`           | HackRF: серийный номер устройства (если их несколько)   |
| `--gain-mode`        | PlutoSDR: `manual`\|`fast_attack`\|`slow_attack`\|`hybrid` |
| `--rx-gain`          | PlutoSDR: ручной RX gain, дБ (только в режиме `manual`) |
| `--uri`              | PlutoSDR: URI контекста, напр. `usb:5.7.5`, `ip:192.168.2.1` |
| `--audio-device`     | ALSA-устройство воспроизведения (по умолчанию `default`) |
| `-h, --help`         | список аргументов                                      |

Частота дискретизации приёма **не настраивается** — она зафиксирована
(`config::hackrf_sample_rate_hz` = 2 МГц, `config::pluto_sample_rate_hz` ≈
2.083 МГц): это минимум, который поддерживает каждое железо, и одновременно
с запасом хватает на канал WFM (~200 кГц). Фиксация нужна, чтобы
коэффициенты канального фильтра и дециматора не пересчитывались на лету.

### Примеры JSON-конфигов

HackRF (`configs/signal_source.json`):

```json
{
    "source": "hack_rf",
    "frequency_hz": 100000000,
    "lna_gain_db": 16,
    "vga_gain_db": 20,
    "amp_enable": false,
    "serial_number": ""
}
```

PlutoSDR (`configs/pluto_signal_source.json`):

```json
{
    "source": "pluto_sdr",
    "frequency_hz": 100000000,
    "gain_control_mode": "fast_attack",
    "rx_gain_db": 40,
    "uri": ""
}
```

`uri`/`serial_number`, оставленные пустыми, означают автоопределение первого
найденного устройства.

## Структура проекта

```
inc/
  config/          конфиги (источник сигнала, DSP-блоки, аудиовывод) + JSON-загрузчик
  hw/               интерфейсы hardware-уровня: signal_source (+ hack_rf, pluto_sdr),
                    audio_sink (+ alsa_sink)
  dsp/              DSP-блоки: интерфейсы (block, channel_filter, resampler,
                    fm_demodulator, deemphasis_filter) и их реализации
  dsp/filters/      сгенерированные коэффициенты FIR-фильтров
src/
  hw/, config/, dsp/  реализации соответствующих заголовков
  app/main.cpp        приложение: CLI, полный приёмный тракт, аудиовывод
configs/              примеры JSON-конфигов (HackRF, PlutoSDR)
matlab/               синтез коэффициентов фильтра
.github/workflows/    CI (сборка на GitHub Actions, x86_64)
```

## Архитектура приёма и DSP-цепочка

`hardware::signal_source` — абстрактный источник IQ-данных
(`open/start/stop/close`), данные отдаются наружу через колбэк
`iq_data_callback_t` с сырым буфером байт; формат сэмплов (`int8_iq` у
HackRF, `int16_iq` у PlutoSDR) сообщается через `sample_format()`. Фабрика
`hardware::create_signal_source(cfg)` выбирает реализацию по `cfg.type`.

Обработка — цепочка блоков `dsp::block<In, Out>` (`inc/dsp/block.h`):
каждый блок принимает N сэмплов на входе и добавляет результат в выходной
вектор, может быть stateful между вызовами. Полный тракт, как он собран в
`main.cpp`:

```
signal_source (2 МГц / 2.083 МГц, int8/int16)
  -> iq_converter          нормализация в std::complex<float>
  -> equiripple_filter      канальный фильтр + децимация x10 (Parks-McClellan FIR)
  -> quadrature_demodulator  FM-дискриминатор: arg(z[n]*conj(z[n-1]))
  -> rc_deemphasis_filter    компенсация pre-emphasis (50 мкс)
  -> audio_resampler         полифазный ресемплинг под 48 кГц
  -> alsa_sink                воспроизведение
```

Приём и воспроизведение работают на разных потоках, связанных
потокобезопасной очередью (`audio_queue` в `main.cpp`): колбэк приёма
никогда не блокируется на записи в ALSA, чтобы не тормозить сам захват
данных с железа.

### Известные упрощения

- `equiripple_filter` спроектирован ровно под 2 МГц (HackRF); для PlutoSDR
  (2.083334 МГц) частоты среза фильтра сдвинуты на те же ~4%.
- Реализовано только моно (composite-сигнал не декодируется в стерео:
  19 кГц pilot + 38 кГц DSB-SC).
