#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#define MAX_I2C_ADDR 0x7F
#define BME280_ADDR 0x76

#define WHITE_LED_GPIO 13
#define BLUE_LED_GPIO 27
#define RED_LED_GPIO 33
#define GREEN_LED_GPIO 32

typedef struct {
  i2c_master_dev_handle_t handle;
  i2c_device_config_t config;
  double temp;
  double humidity;
  double pressure;
} BME280;

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

typedef enum {
  WHITE,
  BLUE,
  RED,
  GREEN,
  COLOR_COUNT,
} LEDColor;

int GetLEDGPIO(LEDColor c) {
  switch (c) {
  case WHITE: {
    return WHITE_LED_GPIO;
  }
  case BLUE: {
    return BLUE_LED_GPIO;
  }
  case RED: {
    return RED_LED_GPIO;
  }
  case GREEN: {
    return GREEN_LED_GPIO;
  }
  default: {
    return 0;
  }
  }
}

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

// BME280 SetupBME280(int address, i2c_master_bus_handle_t masterBusHandle) {
//   BME280 bme = {};
//   if (i2c_master_probe(masterBusHandle, i, -1) != ESP_OK) {
//     EnableLED();
//     vTaskDelay(1000 / portTICK_PERIOD_MS);
//     DisableLED();
//     esp_restart();
//   }
//
//   BME280 bme280 = {};
//
//   bme280.config.dev_addr_length = I2C_ADDR_BIT_LEN_7,
//   bme280.config.device_address = BME280_ADDR;
//   bme280.config.scl_speed_hz = 400000;
//
//   ESP_ERROR_CHECK(i2c_master_bus_add_device(masterBusHandle, &bme280.config,
//                                             &bme280.handle));
// }

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

  EnableLED(WHITE);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  EnableLED(BLUE);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  EnableLED(RED);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  EnableLED(GREEN);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  DisableLED(WHITE);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  DisableLED(BLUE);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  DisableLED(RED);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  DisableLED(GREEN);
  vTaskDelay(500 / portTICK_PERIOD_MS);

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
  // BME280 = SetupBME280(BME280_ADDR, masterBusHandle);
  // SH1106 = SetupSH1106(0x3C);
  // PMS5003 = SetupBME280();
  // EnableLED(GREEN);
  // vTaskDelay(1000 / portTICK_PERIOD_MS);
  // DisableLED(GREEN);

  // for (int i = 0; i <= MAX_I2C_ADDR; ++i) {
  //   if (i2c_master_probe(bus_handle, i, -1) == ESP_OK)
  //     printf("Found I2C address: 0x%02X\n", i);
  // }
  //
  // for (int i = 3; i >= 0; i--) {
  //   printf("Restarting in %d seconds...\n", i);
  //   vTaskDelay(1000 / portTICK_PERIOD_MS);
  // }

  printf("Restarting now.\n");
  fflush(stdout);
  // ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
  esp_restart();
}
