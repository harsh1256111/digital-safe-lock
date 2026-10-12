/*
 * Digital Safe Lock - custom keypad class, pointer-based PIN buffer, strcmp matching
 * Board: Arduino Uno (Wokwi). No Keypad library used.
 * States: LOCKED -> UNLOCKED, 3 wrong PINs -> LOCKOUT (10 s, non-blocking)
 */
#include <Arduino.h>
#include <Servo.h>

// ---------- Custom 4x4 matrix keypad (scan + debounce, non-blocking) ----------
class MatrixKeypad {
public:
  MatrixKeypad(const uint8_t (&rows)[4], const uint8_t (&cols)[4]) {
    for (uint8_t i = 0; i < 4; i++) { rowPins[i] = rows[i]; colPins[i] = cols[i]; }
  }

  void begin() {
    for (uint8_t i = 0; i < 4; i++) {
      pinMode(rowPins[i], OUTPUT);
      digitalWrite(rowPins[i], HIGH);
      pinMode(colPins[i], INPUT_PULLUP);
    }
  }

  // Returns a key exactly once per press, or 0 if nothing new.
  char poll() {
    const uint32_t now = millis();
    const char raw = scanRaw();
    if (raw != lastRaw) { lastRaw = raw; lastChange = now; }
    if (now - lastChange >= DEBOUNCE_MS && lastRaw != stable) {
      stable = lastRaw;
      if (stable) return stable;
    }
    return 0;
  }

private:
  static constexpr uint32_t DEBOUNCE_MS = 30;
  uint8_t rowPins[4];
  uint8_t colPins[4];
  char lastRaw = 0, stable = 0;
  uint32_t lastChange = 0;

  char scanRaw() {
    static const char MAP[] = "123A456B789C*0#D";  // 16 keys, row-major
    char found = 0;
    for (uint8_t r = 0; r < 4 && !found; r++) {
      digitalWrite(rowPins[r], LOW);
      for (uint8_t c = 0; c < 4; c++) {
        if (digitalRead(colPins[c]) == LOW) { found = MAP[r * 4 + c]; break; }
      }
      digitalWrite(rowPins[r], HIGH);
    }
    return found;
  }
};

// ---------- PIN buffer: private array + write-pointer ----------
class PinBuffer {
public:
  static constexpr uint8_t LEN = 4;

  PinBuffer() { clear(); }

  void clear() { head = buf; *head = '\0'; }

  bool push(char c) {
    if (head >= buf + LEN) return false;  // pointer bounds check
    *head++ = c;                          // write, then advance
    *head = '\0';
    return true;
  }

  uint8_t length() const { return head - buf; }  // pointer arithmetic
  bool full() const { return length() == LEN; }
  bool matches(const char* pin) const { return strcmp(buf, pin) == 0; }

private:
  char buf[LEN + 1];  // +1 for '\0'
  char* head;
};

// ---------- Safe lock state machine ----------
enum class LockState : uint8_t { LOCKED, UNLOCKED, LOCKOUT };

class SafeLock {
public:
  SafeLock(MatrixKeypad& k, uint8_t servoPin, uint8_t redPin, uint8_t greenPin)
    : keypad(k), pinServo(servoPin), pinRed(redPin), pinGreen(greenPin) {}

  void begin() {
    keypad.begin();
    pinMode(pinRed, OUTPUT);
    pinMode(pinGreen, OUTPUT);
    latch.attach(pinServo);
    enter(LockState::LOCKED);
  }

  void update() {
    const uint32_t now = millis();

    if (state == LockState::LOCKOUT) {
      keypad.poll();  // keep scanning but ignore input
      if (now - stateStart >= LOCKOUT_MS) { attempts = 0; enter(LockState::LOCKED); }
      else if (now - lastFlash >= 250) {  // non-blocking red flash
        lastFlash = now;
        digitalWrite(pinRed, !digitalRead(pinRed));
      }
      return;
    }

    const char key = keypad.poll();
    if (!key) return;

    if (state == LockState::UNLOCKED) {
      if (key == '#') enter(LockState::LOCKED);  // re-lock
      return;
    }

    // LOCKED
    if (key == '*') { pin.clear(); Serial.println(F("Cleared")); return; }
    if (key < '0' || key > '9') return;
    pin.push(key);
    Serial.print(F("PIN: "));
    for (uint8_t i = 0; i < pin.length(); i++) Serial.print('*');
    Serial.println();

    if (pin.full()) submit();
  }

private:
  static constexpr uint32_t LOCKOUT_MS = 10000;
  static constexpr uint8_t MAX_ATTEMPTS = 3;
  static constexpr uint8_t ANGLE_LOCKED = 0, ANGLE_OPEN = 90;
  const char* const SECRET = "1234";

  MatrixKeypad& keypad;
  Servo latch;
  PinBuffer pin;
  const uint8_t pinServo, pinRed, pinGreen;
  LockState state = LockState::LOCKED;
  uint8_t attempts = 0;
  uint32_t stateStart = 0, lastFlash = 0;

  void submit() {
    if (pin.matches(SECRET)) {
      attempts = 0;
      enter(LockState::UNLOCKED);
    } else if (++attempts >= MAX_ATTEMPTS) {
      enter(LockState::LOCKOUT);
    } else {
      Serial.print(F("Wrong PIN. Attempts left: "));
      Serial.println(MAX_ATTEMPTS - attempts);
    }
    pin.clear();
  }

  void enter(LockState next) {
    state = next;
    stateStart = millis();
    pin.clear();
    digitalWrite(pinGreen, next == LockState::UNLOCKED);
    digitalWrite(pinRed, next == LockState::LOCKED || next == LockState::LOCKOUT);
    latch.write(next == LockState::UNLOCKED ? ANGLE_OPEN : ANGLE_LOCKED);

    switch (next) {
      case LockState::LOCKED:   Serial.println(F("LOCKED - enter 4-digit PIN")); break;
      case LockState::UNLOCKED: Serial.println(F("UNLOCKED - press # to re-lock")); break;
      case LockState::LOCKOUT:  Serial.println(F("LOCKOUT - inputs disabled for 10 s")); break;
    }
  }
};

const uint8_t ROWS[4] = {2, 3, 4, 5};
const uint8_t COLS[4] = {6, 7, 8, 9};
MatrixKeypad keypad(ROWS, COLS);
SafeLock safe(keypad, 10, 11, 12);  // servo, red LED, green LED

void setup() {
  Serial.begin(115200);
  safe.begin();
}

void loop() {
  safe.update();  // non-blocking, no delay()
}
