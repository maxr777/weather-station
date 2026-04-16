#ifndef LED_H
#define LED_H

#include "driver/gpio.h"

#define WHITE_LED_GPIO 13
#define BLUE_LED_GPIO  27
#define RED_LED_GPIO   33
#define GREEN_LED_GPIO 32

typedef enum {
	WHITE,
	BLUE,
	RED,
	GREEN,
	COLOR_COUNT,
} LEDColor;

int GetLEDGPIO(LEDColor c) {
	switch (c) {
	case WHITE:
		return WHITE_LED_GPIO;
	case BLUE:
		return BLUE_LED_GPIO;
	case RED:
		return RED_LED_GPIO;
	case GREEN:
		return GREEN_LED_GPIO;
	default:
		return 0;
	}
}

// TODO: error check
void EnableLED(LEDColor c) {
	int gpio = GetLEDGPIO(c);
	if (gpio != 0)
		gpio_set_level(gpio, 1);
}

void DisableLED(LEDColor c) {
	int gpio = GetLEDGPIO(c);
	if (gpio != 0)
		gpio_set_level(gpio, 0);
}

#endif
