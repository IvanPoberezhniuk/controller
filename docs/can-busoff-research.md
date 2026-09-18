# Діагностика CAN bus-off: ESP32-S3, STM32G431 і SN65HVD230

> Архів діагностики. Після цього дослідження runtime-архітектуру переведено на
> два окремі UART; CAN-модулі та CANH/CANL більше не використовуються.

## Висновок

Записи логічного аналізатора `5.sr` і `7.sr` локалізують несправність у фізичному передавальному тракті CAN-модуля ESP32, а не у швидкості CAN, CRC, пінах MCU чи прошивці STM32.

У `7.sr` права STM32 передає валідні Classic CAN кадри `0x181` на 500 kbit/s. Кадр має DLC 8, дані `00 01 00 00 00 00 00 00` і правильний CRC. Він доходить до `GPIO18/RX` ESP32. Контролер ESP32 формує на `GPIO17/TX` домінантний ACK-імпульс тривалістю один CAN-біт, точно в ACK slot. Проте цей імпульс не з’являється на приймальних виходах трансиверів, тобто SN65HVD230 на стороні ESP не переносить `TXD LOW` на `CANH/CANL`.

Найкраще пояснення, яке прямо передбачене даташитом SN65HVD230, — передавач трансивера вимкнений через стан входу `RS`, несправність мікросхеми або thermal shutdown. Найперше потрібно виміряти `RS` і примусово притиснути його до GND.

## Що підтверджено даташитами

### ESP32-S3

ESP32-S3 має вбудований контролер TWAI, але не має фізичного CAN-трансивера, тому зовнішній SN65HVD230 дійсно необхідний. `GPIO17` та `GPIO18` є повноцінними двонапрямними GPIO і можуть маршрутизуватися через GPIO Matrix. Для них вказана стандартна сила драйвера 10 mA, чого значно більше, ніж потрібно логічному входу `D/TXD` SN65HVD230.[^1][^2]

Конфігурація проєкту правильна:

- `GPIO17 = TWAI TX`;
- `GPIO18 = TWAI RX`;
- `500000 bit/s`;
- `enable_listen_only`, `enable_self_test` і `enable_loopback` не встановлені, тому контролер працює у звичайному режимі та повинен формувати ACK.

Офіційна документація ESP-IDF пояснює, що listen-only забороняє передачу всіх домінантних бітів, включно з ACK. Осцилограма `7.sr` показує ACK на `GPIO17`, тому ESP фактично не перебуває в listen-only. Перехід у bus-off після накопичення помилок також відповідає штатній поведінці TWAI.[^2]

### STM32G431

Даташит STM32G431 підтверджує:

- `PA11 = FDCAN1_RX`, alternate function `AF9`;
- `PA12 = FDCAN1_TX`, alternate function `AF9`.[^3]

У прошивці встановлені `FDCAN_MODE_NORMAL`, Classic CAN та timing:

```text
FDCAN clock = 170 MHz
prescaler   = 10
TSEG1       = 29
TSEG2       = 4
bitrate     = 170 MHz / (10 × (1 + 29 + 4)) = 500 kbit/s
sample point = 30 / 34 = 88.2%
```

Правильний CRC і стабільне декодування `0x181` незалежно підтверджують, що STM32, її clock, FDCAN timing і TX-трансивер працюють.

### SN65HVD230

У даташиті TI визначено пряме підключення без перехрещення:[^4]

```text
MCU TX  -> D/TXD, pin 1 трансивера
MCU RX <-  R/RXD, pin 4 трансивера
CANL    = pin 6
CANH    = pin 7
RS      = pin 8
```

`D/TXD = LOW` повинен створювати dominant state. `D/TXD = HIGH` або відкритий вхід створює recessive state.

Критичний режим `RS`:[^5]

| Напруга RS | Режим | Передавач | Приймач |
|---|---|---:|---:|
| `< 1.2 V`, пряме GND | High-speed | ON | ON |
| `< 1.2 V`, 10–100 kΩ до GND | Slope-control | ON | ON |
| `> 0.75 × VCC` | Standby/listen-only | **OFF** | **ON** |

При VCC 3.3 V standby гарантовано починається приблизно вище `2.48 V`. Саме цей режим дає симптом «RX бачить шину, TXD перемикається, але CANH/CANL не передають».

TI також вказує, що thermal shutdown вимикає тільки драйвер CANH/CANL, залишаючи шлях CAN bus → RXD працездатним.[^6] На холодному щойно заміненому модулі це менш імовірно, але пошкоджена або підроблена мікросхема може поводитися аналогічно.

## Особливість синього модуля Waveshare/клона

На схемі такого модуля `R1 = 10 kΩ` підключає `RS` до GND, тобто вмикає slope-control. `R2 = 120 Ω` постійно стоїть між CANH і CANL.[^7]

Два жовті штирі біля клемника — це паралельний вихід `CANH/CANL`, а **не джампер термінації**. Waveshare прямо попереджає не замикати CANL і CANH.[^8] Встановлений на ці два штирі jumper створює майже `0 Ω` між CANH і CANL. Попередній вимір це вже експериментально підтвердив:

```text
із jumper:  приблизно 0 Ω — CANH і CANL закорочені;
без jumper: приблизно 120 Ω — видно штатний R2.
```

На жодному з трьох модулів цей жовтий jumper встановлювати не можна.

## Перевірка без осцилографа

### 1. Перевірка RS — пріоритет №1

З увімкненим живленням виміряти напругу:

```text
SN65HVD230 pin 8 (RS) -> GND
```

Очікування: близько `0 V`, обов’язково нижче `1.2 V`.

Якщо там понад `2.4–2.5 V`, трансивер перебуває у standby: він приймає CAN, але не може передавати ACK. Якщо напруга невизначена між 1.2 і 2.48 V, режим також некоректний.

Для контрольного тесту можна на короткий час з’єднати `RS/pin 8` безпосередньо з GND. Практичну рекомендацію зашунтувати штатний 10 kΩ резистор `R1`, щоб RS був прямо на GND, також дає технічний модератор ST для аналогічної проблеми зі SN65HVD230.[^9]

### 2. Статичний тест передавача мультиметром

Якщо RS близько 0 V:

1. Від’єднати `GPIO17` від контакту `TX` модуля.
2. Залишити VCC=3.3 V, GND, CANH і CANL підключеними.
3. Через резистор 1 kΩ притиснути `TX/D` модуля до GND на кілька секунд.
4. Виміряти CANH і CANL відносно GND.

Очікувано:

```text
TX HIGH/open: CANH ≈ CANL ≈ 2.2–2.5 V, recessive
TX LOW:       CANH ≈ 3.0–3.3 V
              CANL ≈ 1.0–1.5 V, dominant
```

Якщо TX LOW, RS LOW і VCC=3.3 V, але CANH/CANL не розходяться, модуль або його мікросхема несправні. TI Support описує той самий діагностичний критерій: якщо TXD перемикається, а CANH/CANL — ні, несправність знаходиться у трансивері/його режимі.[^10]

### 3. Термінація

Міряти тільки при повністю вимкненому живленні:

```text
CANH <-> CANL = близько 60 Ω
```

У фінальній шині з ESP32 посередині та двома STM32 на фізичних кінцях потрібно залишити по одному 120 Ω лише на двох кінцях. Центральний модуль ESP не повинен додавати третій 120 Ω, інакше загальний опір стане приблизно 40 Ω.[^4]

## Ранжування причин

1. **RS не притягнутий до GND / модуль у standby** — повний збіг із `7.sr` та таблицею режимів TI.
2. **Несправний або контрафактний VP230/SN65HVD230** — особливо якщо RS LOW і статичний TX-тест не змінює CANH/CANL.
3. **Немає реальних 3.0–3.6 V безпосередньо на pin 3 мікросхеми або погана земля** — перевірити на ніжках IC, не лише на виході MP1584.
4. **Thermal shutdown або пошкодження після попереднього короткого CANH–CANL jumper’ом** — можливе, але заміна модуля робить це менш імовірним.
5. **Прошивка/pin mux/timing** — малоймовірно: `7.sr` уже доводить правильний RX, декодування 500 kbit/s та формування ACK на GPIO17.

## Рекомендована наступна дія

Не змінювати прошивки. Спочатку виміряти `RS/pin 8 → GND`. Якщо це не близько 0 V, притиснути RS прямо до GND і повторити CAN-тест. Це найкоротший експеримент, який або одразу виправить зв’язок, або остаточно відокремить проблему режиму RS від несправної мікросхеми.

## Джерела

[^1]: Espressif Systems, [ESP32-S3 Series Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf), pin overview and GPIO drive strength.
[^2]: Espressif Systems, [ESP-IDF TWAI Programming Guide for ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/twai.html), hardware connection, modes, errors and recovery.
[^3]: STMicroelectronics, [STM32G431x6/x8/xB Datasheet](https://www.st.com/resource/en/datasheet/stm32g431r8.pdf), alternate-function table for PA11/PA12.
[^4]: Texas Instruments, [SN65HVD230/231/232 Datasheet](https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf), pin functions and typical CAN network termination.
[^5]: Texas Instruments, [SN65HVD230 Datasheet — Device Functional Modes](https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf), sections 10.4.1–10.4.6 and Tables 2–4.
[^6]: Texas Instruments, [SN65HVD230 Datasheet — Thermal Shutdown](https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf), section 10.3.2.
[^7]: Waveshare, [SN65HVD230 CAN Board](https://www.waveshare.com/product/sn65hvd230-can-board.htm), official board resources and schematic; the published schematic uses R1=10 kΩ from RS to GND and R2=120 Ω across CANH/CANL.
[^8]: Waveshare Wiki, [SN65HVD230 CAN Board](https://www.waveshare.com/wiki/SN65HVD230_CAN_Board), warning not to short CANL and CANH.
[^9]: STMicroelectronics Community, [STM32 FDCAN node sees nothing on bus when CAN-to-USB adapter is removed](https://community.st.com/stm32-mcus-products-25/stm32-fdcan-node-sees-nothing-on-bus-when-can-to-usb-adapter-removed-152218), recommendation to shunt R1 and tie RS directly to GND.
[^10]: Texas Instruments E2E Support, [SN65HVD230 Interface Forum](https://e2e.ti.com/support/interface-group/interface/f/interface-forum/1223563/sn65hvd230-interface-forum), TXD versus CANH/CANL diagnostic guidance.
