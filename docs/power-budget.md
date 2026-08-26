# UGV low-voltage power budget

This budget covers only the two MP1584EN modules used for controller logic,
sensors, encoders, CAN transceivers, and IBT-2 logic. Motor power, lights, and
Raspberry Pi 5 power are separate and are not included.

The values below are conservative design allocations, not measured currents.
Unknown module currents deliberately include generous allowances. Replace
them with measured startup, idle, radio-transmit, and worst-case operating
values during hardware bring-up.

## MCU requirements versus complete rails

| Item | Datasheet/estimated load | Supply capacity used in this design |
| --- | ---: | ---: |
| STM32G431 silicon at 170 MHz | about 24 mA typical, `3.3 V x 0.024 A = 0.079 W` | - |
| Complete STM32 board | application-dependent | 100 mA, `0.33 W` |
| Complete local STM32 rail including CAN, three encoders, and three IBT-2 logic interfaces | application-dependent | 500 mA, `1.65 W` |
| ESP32-S3 silicon during maximum-power Wi-Fi TX | up to about 340 mA peak, `3.3 V x 0.340 A = 1.12 W` | - |
| ESP32-S3 board | Espressif recommends at least a 500 mA supply | 600 mA, `1.98 W` allocation |
| Complete central 3.3 V rail including ESP32 board and peripherals | application-dependent | 1.00 A, `3.30 W` |

The STM32 does not need 1.65 W by itself, and the ESP32 does not continuously
consume 3.30 W. Those larger values are converter sizing budgets for every
consumer on the associated rail plus margin. Radio current peaks and converter
transient response make the current rating and local decoupling as important
as the average wattage. Fit at least 10 uF at the ESP32 board power entrance
and 100 nF at each IC supply pair, in addition to the converter module's own
capacitors.

## Converter allocation

| Converter | Output | Design current | Design power | Share of 10 W ceiling |
| --- | ---: | ---: | ---: | ---: |
| Shared 3.3 V logic rail | 3.30 V | 2.00 A | 6.60 W | 66% |
| XR4/GPS rail | 5.00 V | 0.60 A | 3.00 W | 30% |
| **Both outputs** | mixed | - | **9.60 W** | split across two converters |

The shared 3.3 V converter operates at 66% of the stated 10 W ceiling and must
pass enclosure-temperature and remote-node voltage-drop tests. The combined
9.60 W is split between two converters at different output voltages.

## Per-node 3.3 V allocation

This table applies separately to Left and Right.

| Consumers on one STM32 node | Quantity | Current allocation |
| --- | ---: | ---: |
| STM32G431 board, MCU peripherals, LEDs, and board losses | 1 | 100 mA |
| SN65HVD230 CAN transceiver | 1 | 20 mA |
| GB37-520B Hall encoders | 3 | 75 mA total; provisional 25 mA each |
| AHC244D-equipped IBT-2 logic interfaces and LEDs | 3 | 150 mA total; provisional 50 mA each |
| Wiring loss, tolerance, and expansion margin | - | 155 mA |
| **Design total per node** | - | **500 mA at 3.3 V = 1.65 W** |

The STM32 silicon itself typically draws about 24 mA with CoreMark running at
170 MHz, before application-specific peripheral and board loads. The 100 mA
board allocation is therefore intentionally conservative. The SN65HVD230 data
sheet specifies up to 17 mA supply current in dominant or recessive mode, which
is rounded up to 20 mA here.

## Central 3.3 V allocation

| Consumers | Current allocation |
| --- | ---: |
| ESP32-S3 board including Wi-Fi current peaks | 600 mA |
| SN65HVD230 CAN transceiver | 20 mA |
| SH1106 OLED | 50 mA provisional |
| QMI8658A, rotary encoder, ambient-light sensor, and pull-ups | 50 mA provisional |
| Wiring loss, tolerance, and expansion margin | 280 mA |
| **Design total** | **1.00 A at 3.3 V = 3.30 W** |

Espressif recommends a supply capable of at least 500 mA for a single ESP32-S3
supply. The 600 mA allocation reserves additional peak margin, while the full
1 A rail budget includes the external 3.3 V peripherals.

## Central 5 V allocation

| Consumers | Current allocation |
| --- | ---: |
| RadioMaster XR4 receiver, including RF/Wi-Fi activity | 300 mA provisional |
| HGLRC M100-5883 GPS/compass | 150 mA provisional |
| Wiring loss, tolerance, and expansion margin | 150 mA |
| **Design total** | **600 mA at 5.0 V = 3.00 W** |

RadioMaster specifies a 4.5-8.4 V input range but does not publish maximum XR4
input current. HGLRC specifies a 3.3-5 V input range but does not publish the
M100-5883 maximum current. The allocations above must therefore be confirmed
with an ammeter, especially with XR4 telemetry and Wi-Fi active and during GPS
cold start.

## Estimated battery-side logic load

The two converters have a combined design output allocation of 9.60 W. Using
85% conversion efficiency for planning gives:

```text
Estimated converter input power = 9.60 W / 0.85 = 11.3 W
Estimated current from a 12 V source = 11.3 W / 12 V = 0.94 A
```

This is only the low-voltage electronics branch. It excludes all six motors,
Raspberry Pi 5, camera, lights, buzzer load, and conversion losses in their
separate supplies.

## Validation requirements

1. Adjust each converter with its output disconnected, then power-cycle and
   verify `3.30 V` or `5.00 V` before attaching electronics.
2. Measure each rail at startup, idle, full CAN traffic, ESP32 Wi-Fi transmit,
   XR4 telemetry transmit, and GPS cold start.
3. Test converter temperature in the enclosure at the highest expected ambient
   temperature. Treat 10 W as a ceiling, not a continuous operating target.
4. Recalculate the branch fuse and wire size from measured current plus startup
   margin.
5. Never parallel converter outputs or connect an onboard/USB regulator to a
   rail already driven by an MP1584EN.

## Datasheet basis

- [MP1584 datasheet](https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/Datasheet/lang/en/sku/MP1584EN-LF-Z/)
- [STM32G431CB datasheet](https://www.st.com/resource/en/datasheet/stm32g431cb.pdf)
- [SN65HVD230 datasheet](https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf)
- [ESP32-S3 hardware design guidelines](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html)
- [RadioMaster XR4 specifications](https://www.radiomasterrc.com/products/xr4-gemini-xrossband-dual-band-expresslrs-receiver)
- [HGLRC M100-5883 specifications](https://www.hglrc.com/products/m100-5883-gps)
