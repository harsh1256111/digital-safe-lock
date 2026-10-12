# Digital Safe Lock (Custom Keypad Class)

A memory-efficient safe lock for Arduino Uno. It uses **no Keypad library**:
the 4x4 matrix scanning, debouncing, PIN buffer and state machine are all
hand-written C++ classes. Runs in the [Wokwi](https://wokwi.com) simulator.

**Live demo:** open `dashboard.html` to try the logic in your browser.

## How it works
Enter the 4-digit PIN (default `1234`). Correct PIN: servo turns 90 degrees
and the safe is `UNLOCKED`. Three wrong PINs trigger a 10 second `LOCKOUT`
with a flashing red LED and all input ignored. Everything is timed with
`millis()`; there is no `delay()`.

| Key | LOCKED | UNLOCKED | LOCKOUT |
|-----|--------|----------|---------|
| 0-9 | add digit (auto-submits at 4) | ignored | ignored |
| `*` | clear entry | ignored | ignored |
| `#` | ignored | re-lock | ignored |

## State transitions
| From | To | Condition |
|------|----|-----------|
| LOCKED | UNLOCKED | 4 digits match PIN |
| LOCKED | LOCKED | wrong PIN, attempts < 3 |
| LOCKED | LOCKOUT | 3rd wrong PIN |
| UNLOCKED | LOCKED | `#` pressed |
| LOCKOUT | LOCKED | 10 s elapsed (attempts reset) |

## C++ concepts demonstrated
- `MatrixKeypad`: private pin arrays, row/column scanning, non-blocking debounce, returns each press once
- `PinBuffer`: private `char[5]` buffer with a write pointer (`*head++`), bounds check by pointer comparison, length via pointer subtraction
- `strcmp` for PIN matching on null-terminated character arrays
- `enum class LockState` and a class-based state machine
- Constructor injection (`SafeLock` takes a `MatrixKeypad&`), `constexpr` constants

## Components (simulated)
| Part | Pins |
|------|------|
| Arduino Uno | - |
| 4x4 membrane keypad | rows 2-5, columns 6-9 |
| Servo (latch) | 10 |
| Red LED + 220 ohm | 11 |
| Green LED + 220 ohm | 12 |
| Serial Monitor (115200 baud) | status output |

## Run it
1. On wokwi.com create a new Arduino Uno project.
2. Paste `safe_lock.ino` and `diagram.json`.
3. Press Play, open the Serial Monitor, and click keypad buttons.

## Ideas to extend
LCD output, buzzer on lockout, PIN stored in EEPROM, change-PIN mode.

## Files
`safe_lock.ino`, `diagram.json`, `dashboard.html`
