#pragma once
#ifndef MAIN_HPP
#define MAIN_HPP

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>

#define BOOT_BUTTON_PIN    0
#define RGB_LED_PIN        48
#define IR_RECEIVER_PIN 15
#define IR_SENDER_PIN 16
#define REMOTE_SLOT_COUNT  8

#define DEBOUNCE_MS        50
#define DOUBLE_CLICK_MS    500
#define LONG_PRESS_MS      2000

enum AppMode
{
    MODE_NORMAL,
    MODE_PROGRAM
};

enum FeedbackBlink
{
    FEEDBACK_NONE,
    FEEDBACK_SUCCESS,
    FEEDBACK_FAIL
};

struct RgbColor
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct Slot
{
    bool programmed;
    uint32_t irCode;
};

#endif