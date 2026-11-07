#include "main.hpp"

static AppMode s_mode = MODE_NORMAL;

static uint8_t s_selectedSlot = 0;
static Preferences s_preferences; 
static IRrecv s_irReceiver(IR_RECEIVER_PIN);
static IRsend s_irSender(IR_SENDER_PIN);

static decode_results s_irResults;
static Slot s_slots[REMOTE_SLOT_COUNT];

static Adafruit_NeoPixel s_pixel(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

static bool s_buttonRawState = HIGH;
static bool s_buttonState = HIGH;
static uint32_t s_lastDebounceTime = 0;

static bool s_buttonPressed = false;
static bool s_longPressTriggered = false;

static uint32_t s_pressStartTime = 0;

static uint8_t s_clickCount = 0;
static uint32_t s_lastClickTime = 0;

static uint32_t s_blinkTimestamp = 0;

static bool s_blinkState = true;
static FeedbackBlink s_feedbackBlink = FEEDBACK_NONE;

static bool s_feedbackActive = false;
static uint32_t s_feedbackStartTime = 0;

static const RgbColor s_slotColors[REMOTE_SLOT_COUNT] =
{
    {255,   0,   0},   // Rot
    {  0, 255,   0},   // Grün
    {  0,   0, 255},   // Blau
    {255, 255,   0},   // Gelb
    {255,   0, 255},   // Magenta
    {  0, 255, 255},   // Cyan
    {255, 128,   0},   // Orange
    {255, 255, 255}    // Weiß
};

static void loadSlots()
{
    for (uint8_t i = 0; i < REMOTE_SLOT_COUNT; i++)
    {
        char key[8];

        snprintf(key, sizeof(key), "s%u", i);

        s_slots[i].irCode = s_preferences.getULong(key, 0);

        s_slots[i].programmed = (s_slots[i].irCode != 0);

        log_i(
            "Slot %u loaded: programmed=%u code=0x%08X",
            i,
            s_slots[i].programmed,
            s_slots[i].irCode
        );
    }
}

static void saveSlot(uint8_t slot)
{
    char key[8];

    snprintf(key, sizeof(key), "s%u", slot);
 
    s_preferences.putULong(
        key,
        s_slots[slot].irCode
    );
 
    log_i(
        "Slot %u saved: 0x%08X",
        slot,
        s_slots[slot].irCode
    );
}

static bool receiveIrCode(uint32_t* code)
{
    if (!s_irReceiver.decode(&s_irResults))
    {
        return false;
    }

    *code = s_irResults.value;

    log_i(
        "IR Received: 0x%08X",
        *code
    );

    s_irReceiver.resume();

    return true;
}

static bool executeSlot(uint8_t slot)
{
    if (!s_slots[slot].programmed)
    {
        return false;
    }

    log_i(
        "Sending Slot %u -> 0x%08X",
        slot,
        s_slots[slot].irCode
    );

    s_irSender.sendNEC(
        s_slots[slot].irCode,
        32
    );

    return true;
}

static bool programSlot(uint8_t slot)
{  
    uint32_t code;

    if (!receiveIrCode(&code))
    {
        return false;
    }

    s_slots[slot].irCode = code;
    s_slots[slot].programmed = true;

    saveSlot(slot);

    log_i(
        "Slot %u programmed: 0x%08X",
        slot,
        code
    );

    return true;
}

static void nextSlot()
{
    s_selectedSlot++;

    if (s_selectedSlot >= REMOTE_SLOT_COUNT)
    {
        s_selectedSlot = 0;
    }

    log_i(
        "Selected Slot %u (%s)",
        s_selectedSlot,
        s_slots[s_selectedSlot].programmed
            ? "programmed"
            : "empty"
    );
}

static void toggleMode()
{
    if (s_mode == MODE_NORMAL)
    {
        s_mode = MODE_PROGRAM;
        log_i("Mode -> PROGRAM");
    }
    else
    {
        s_mode = MODE_NORMAL;
        log_i("Mode -> NORMAL");
    }
}

static void startSuccessBlink()
{
    s_feedbackBlink = FEEDBACK_SUCCESS;
    s_feedbackActive = true;
    s_feedbackStartTime = millis();

    s_pixel.setPixelColor(0, 0);
    s_pixel.show();

    log_i("Feedback SUCCESS");
}

static void startFailBlink()
{
    s_feedbackBlink = FEEDBACK_FAIL;
    s_feedbackActive = true;
    s_feedbackStartTime = millis();

    s_pixel.setPixelColor(0, 0);
    s_pixel.show();

    log_i("Feedback FAIL");
}

static void updateLed()
{
    const RgbColor& color = s_slotColors[s_selectedSlot];

    uint8_t brightness =
        s_slots[s_selectedSlot].programmed
    ? 64
    : 13;

    if (s_feedbackActive)
    { 
        uint32_t elapsed = millis() - s_feedbackStartTime;

        bool ledOn = false;

        if (s_feedbackBlink == FEEDBACK_SUCCESS)
        {
            /*
            * 250 ms gesamt
            * 2 Blinks
            *
            * OFF 31
            * ON  62
            * OFF 31
            * ON  62
            * OFF Rest
            */

            if ((elapsed >= 31 && elapsed < 93) ||
                (elapsed >= 124 && elapsed < 186))
            {
                    ledOn = true;
            }

            if (elapsed >= 250)
            {
                s_feedbackActive = false;
                s_feedbackBlink = FEEDBACK_NONE;
            }
        }
        else if (s_feedbackBlink == FEEDBACK_FAIL)
        {
            /*
            * 600 ms gesamt
            * 2 Blinks
            *
            * OFF 75
            * ON 225
            * OFF 75
            * ON 225
            */

            if ((elapsed >= 75 && elapsed < 300) ||
                (elapsed >= 375 && elapsed < 600))
            {
                ledOn = true;
            }

            if (elapsed >= 600)
            {
                s_feedbackActive = false;
                s_feedbackBlink = FEEDBACK_NONE;
            }
        }

        if (ledOn)
        {
            s_pixel.setPixelColor(
                0,
                s_pixel.Color(
                    (color.r * brightness) / 255,
                    (color.g * brightness) / 255,
                    (color.b * brightness) / 255
                )
            );
        }
        else
        {
            s_pixel.setPixelColor(0, 0);
        }

        s_pixel.show();

        return;
    }
    
    if (s_mode == MODE_NORMAL)
    {
        s_pixel.setPixelColor(
            0,
            s_pixel.Color(
                (color.r * brightness) / 255,
                (color.g * brightness) / 255,
                (color.b * brightness) / 255
            )
        );
    }
    else
    {
        if (millis() - s_blinkTimestamp >= 250)
        {
            s_blinkTimestamp = millis();
            s_blinkState = !s_blinkState;
        }

        if (s_blinkState)
        {
            s_pixel.setPixelColor(
                0,
                s_pixel.Color(
                    (color.r * brightness) / 255,
                    (color.g * brightness) / 255,
                    (color.b * brightness) / 255
                )
            );
        }
        else
        {
            s_pixel.setPixelColor(
                0,
                s_pixel.Color(0, 0, 0)
            );
        }
    }

    s_pixel.show();
}

static void handleSingleClick()
{
    log_i("Single Click");

    nextSlot();
}
static void handleDoubleClick()
{
    bool success;

    log_i("Double Click");

    if (s_mode == MODE_NORMAL)
    {
        success = executeSlot(s_selectedSlot);
    }
    else
    {
        success = programSlot(s_selectedSlot);
    }

    if (success)
    {
        startSuccessBlink();
    }
    else
    {
        startFailBlink();
    }
}

static void handleLongPress()
{
    log_i("Long Press");

    toggleMode();
}

static void updateButton()
{
    bool rawState = digitalRead(BOOT_BUTTON_PIN);

    if (rawState != s_buttonRawState)
    {
        s_lastDebounceTime = millis();
        s_buttonRawState = rawState;
    }

    if ((millis() - s_lastDebounceTime) > DEBOUNCE_MS)
    {
        if (rawState != s_buttonState)
        {
            s_buttonState = rawState;

            if (s_buttonState == LOW)
            {
                s_buttonPressed = true;
                s_longPressTriggered = false;
                s_pressStartTime = millis();
            }
            else
            {
                if (s_buttonPressed && !s_longPressTriggered)
                {
                    s_clickCount++;
                    s_lastClickTime = millis();
                }

                s_buttonPressed = false;
            }
        }
    }

    if (s_buttonPressed)
    {
        if (!s_longPressTriggered &&
            (millis() - s_pressStartTime >= LONG_PRESS_MS))
        {
            s_longPressTriggered = true;
            s_clickCount = 0;

            handleLongPress();
        }
    }

    if (s_clickCount > 0)
    {
        if (millis() - s_lastClickTime >= DOUBLE_CLICK_MS)
        {
            if (s_clickCount == 1)
            {
                handleSingleClick();
            }
            else
            {
                handleDoubleClick();
            }

            s_clickCount = 0;
        }
    }
}

void setup(){
    Serial.begin(115200); 

    s_irReceiver.enableIRIn();
    s_irSender.begin();

    log_i("IR Receiver initialized");

    s_preferences.begin("remote", false);

    loadSlots();

    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

    s_pixel.begin();
    s_pixel.setBrightness(64);
    s_pixel.clear();
    s_pixel.show();

    log_i("Remote Test");
    log_i("Boot Button : GPIO%d", BOOT_BUTTON_PIN);
    log_i("RGB LED     : GPIO%d", RGB_LED_PIN);
    log_i("IR RX       : GPIO%d", IR_RECEIVER_PIN);
    log_i("IR TX       : GPIO%d", IR_SENDER_PIN);
    log_i("Mode -> NORMAL");
    log_i("Slot -> 0");
}

void loop()
{

    if (s_mode == MODE_PROGRAM)
    {
        uint32_t code;

        if (receiveIrCode(&code))
        {
            s_slots[s_selectedSlot].irCode = code;
            s_slots[s_selectedSlot].programmed = true;

            saveSlot(s_selectedSlot);

            startSuccessBlink();

            log_i(
                "Slot %u programmed: 0x%08X",
                s_selectedSlot,
                code
            );
        }
    }
    updateButton();
    updateLed();
}