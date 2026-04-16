#include "bme280.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "sdkconfig.h"
#include "types.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
	i2c_master_dev_handle_t handle;
	i2c_device_config_t config;
	bool screen[128][64];
} SH1106;

typedef struct {
	int pm10;
	int pm25;
	int pm100;
} PMS5003;

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
	busConfig.flags.enable_internal_pullup = 0;
	busConfig.flags.allow_pd = 0;

	i2c_master_bus_handle_t masterBusHandle;
	ESP_ERROR_CHECK(i2c_new_master_bus(&busConfig, &masterBusHandle));

	// Setup the sensors
	EnableLED(WHITE);
	BME280 bme280 = SetupBME280(BME280_ADDR, masterBusHandle);

	// EnableLED(BLUE);
	// SH1106 sh1106 = SetupSH1106(0x3C);

	// EnableLED(RED);
	// PMS5003 pms5003 = SetupBME280();

	EnableLED(GREEN);
	vTaskDelay(pdMS_TO_TICKS(1000));

	DisableLED(WHITE);
	DisableLED(BLUE);
	DisableLED(RED);
	DisableLED(GREEN);

	while (true) {
		GetWeatherReadings(&bme280);
		printf("temperature: %.2f C\nhumidity: %.2f%%\npressure: %.2f Pa\n\n",
		       bme280.temperature, bme280.humidity, bme280.pressure);

		// GetAirQuality(&pms5003);
		//
		// snprintf();
		//
		// DisplayText("abcd\nabcd\n");

		fflush(stdout);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}
