#include "bme280.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "pms5003.h"
#include "sdkconfig.h"
#include "sh1106.h"
#include "types.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

void app_main(void) {
	// Setup the LED GPIOs as output
	gpio_reset_pin(WHITE_LED_GPIO);
	gpio_set_direction(WHITE_LED_GPIO, GPIO_MODE_OUTPUT);
	gpio_reset_pin(BLUE_LED_GPIO);
	gpio_set_direction(BLUE_LED_GPIO, GPIO_MODE_OUTPUT);
	gpio_reset_pin(RED_LED_GPIO);
	gpio_set_direction(RED_LED_GPIO, GPIO_MODE_OUTPUT);
	gpio_reset_pin(GREEN_LED_GPIO);
	gpio_set_direction(GREEN_LED_GPIO, GPIO_MODE_OUTPUT);

	// Setup the i2c bus
	i2c_master_bus_config_t busConfig = {};
	busConfig.i2c_port = -1;
	busConfig.sda_io_num = 21;
	busConfig.scl_io_num = 22;
	busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
	busConfig.glitch_ignore_cnt = 7;
	busConfig.intr_priority = 0;
	busConfig.trans_queue_depth = 0;
	// Internal pullups work fine with both BME280 and SH1106, so I didn't use any external ones
	busConfig.flags.enable_internal_pullup = 1;
	busConfig.flags.allow_pd = 0;

	i2c_master_bus_handle_t masterBusHandle;
	ESP_ERROR_CHECK(i2c_new_master_bus(&busConfig, &masterBusHandle));

	// Setup the sensors
	EnableLED(WHITE);
	SH1106 sh1106 = SetupSH1106(masterBusHandle);

	EnableLED(BLUE);
	BME280 bme280 = SetupBME280(masterBusHandle);

	EnableLED(RED);
	PMS5003 pms5003 = SetupPMS5003();

	EnableLED(GREEN);
	vTaskDelay(pdMS_TO_TICKS(1000));

	DisableLED(WHITE);
	DisableLED(BLUE);
	DisableLED(RED);
	DisableLED(GREEN);

	while (true) {
		GetWeatherReadingsErrorCodes BME280ErrorCode = GetWeatherReadings(&bme280);
		GetAirQualityErrorCodes PMS5003ErrorCode = GetAirQuality(&pms5003);

		char temp[20];
		char humidity[20];
		char pressure[20];
		char pm10[20];
		char pm25[20];
		char pm100[20];

		if (BME280ErrorCode == BME_OK) {
			snprintf(temp, sizeof(temp), "T: %.0f C", bme280.temperature);
			snprintf(humidity, sizeof(humidity), "H: %.0f%%", bme280.humidity);
			snprintf(pressure, sizeof(pressure), "P: %.0f hPa", bme280.pressure / 100);
		} else {
			ESP_LOGE("BME280", "%s", GetWeatherReadingsErrorCodesToStr(BME280ErrorCode));
			snprintf(temp, sizeof(temp), "T: %.0f C*", bme280.temperature);
			snprintf(humidity, sizeof(humidity), "H: %.0f%%*", bme280.humidity);
			snprintf(pressure, sizeof(pressure), "P: %.0f hPa*", bme280.pressure / 100);
		}

		if (PMS5003ErrorCode == PMS_OK) {
			snprintf(pm10, sizeof(pm10), "PM 1.0: %d", pms5003.pm10);
			snprintf(pm25, sizeof(pm25), "PM 2.5: %d", pms5003.pm25);
			snprintf(pm100, sizeof(pm100), "PM 10:  %d", pms5003.pm100);
		} else {
			ESP_LOGE("PMS5003", "%s", GetAirQualityErrorCodesToStr(PMS5003ErrorCode));
			snprintf(pm10, sizeof(pm10), "PM 1.0: %d*", pms5003.pm10);
			snprintf(pm25, sizeof(pm25), "PM 2.5: %d*", pms5003.pm25);
			snprintf(pm100, sizeof(pm100), "PM 10:  %d*", pms5003.pm100);
		}

		SH1106ClearScreen(&sh1106);
		SH1106PrintText(&sh1106, temp, 1);
		SH1106PrintText(&sh1106, humidity, 2);
		SH1106PrintText(&sh1106, pressure, 3);
		SH1106PrintText(&sh1106, pm10, 4);
		SH1106PrintText(&sh1106, pm25, 5);
		SH1106PrintText(&sh1106, pm100, 6);
		SH1106Flush(&sh1106);

		fflush(stdout);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}
