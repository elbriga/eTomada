#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "rgb-led.h"
#include "hardwareProfile.h"

// Hardware Profile - um para cada placa
extern const HardwareProfile hardwareProfile;

#define INTENSIDADE 0.1

static Adafruit_NeoPixel led;

void rgbLedInit()
{
    led = Adafruit_NeoPixel(1, hardwareProfile.ledPin, NEO_GRB + NEO_KHZ800);

    led.begin();
    led.clear();
    led.show();
}

void rgbLedWrite(uint8_t r, uint8_t g, uint8_t b)
{
    led.setPixelColor(0, r * INTENSIDADE, g * INTENSIDADE, b * INTENSIDADE);
    led.show();
}
