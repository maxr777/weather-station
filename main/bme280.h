#ifndef BME280_H
#define BME280_H

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "types.h"
#include <stdio.h>

#define BME280_ADDR 0x76

typedef struct {
	u16 T1;
	i16 T2;
	i16 T3;
	u16 P1;
	i16 P2;
	i16 P3;
	i16 P4;
	i16 P5;
	i16 P6;
	i16 P7;
	i16 P8;
	i16 P9;
	u8 H1;
	i16 H2;
	u8 H3;
	i16 H4;
	i16 H5;
	i8 H6;
} BME280Calibration;

typedef struct {
	i2c_master_dev_handle_t handle;
	i2c_device_config_t config;
	BME280Calibration offsets;
	// shared variable needed for pressure and humidity compensation
	i32 t_fine;
	double temp;
	double humidity;
	double pressure;
} BME280;

BME280 SetupBME280(int addr, i2c_master_bus_handle_t masterBusHandle) {
	BME280 bme280 = {};
	while (i2c_master_probe(masterBusHandle, addr, -1) != ESP_OK) {
		vTaskDelay(pdMS_TO_TICKS(250));
	}

	// init the device handle on i2c
	bme280.config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
	bme280.config.device_address = addr;
	bme280.config.scl_speed_hz = 400000;

	ESP_ERROR_CHECK(i2c_master_bus_add_device(masterBusHandle, &bme280.config, &bme280.handle));

	// reset - write 0xB6 to 0xE0
	{
		uint8_t buf[2] = {0xE0, 0xB6};
		ESP_ERROR_CHECK(i2c_master_transmit(bme280.handle, buf, 2, 100));

		// wait for the chip to come back
		vTaskDelay(pdMS_TO_TICKS(10));
	}

	// check chip id - read 0xD0, should be 0x60
	{
		uint8_t reg = 0xD0;
		uint8_t val = 0;
		ESP_ERROR_CHECK(i2c_master_transmit_receive(bme280.handle, &reg, 1, &val, sizeof(val), 100));
		if (val != 0x60) {
			ESP_LOGE("BME280", "Wrong chip ID: expected 0x60, got 0x%02X", val);
			esp_restart();
		}
	}

	// read the offset values - they're used for calibration of each bme280, as there are factory variances
	// so you apply the offsets to the raw values you get the get the actual values
	{
		u8 reg = 0x88;
		u8 raw[24];
		ESP_ERROR_CHECK(i2c_master_transmit_receive(bme280.handle, &reg, 1, raw, sizeof(raw), 100));
		bme280.offsets.T1 = (u16)((u16)raw[1] << 8 | raw[0]);
		bme280.offsets.T2 = (i16)((u16)raw[3] << 8 | raw[2]);
		bme280.offsets.T3 = (i16)((u16)raw[5] << 8 | raw[4]);
		bme280.offsets.P1 = (u16)((u16)raw[7] << 8 | raw[6]);
		bme280.offsets.P2 = (i16)((u16)raw[9] << 8 | raw[8]);
		bme280.offsets.P3 = (i16)((u16)raw[11] << 8 | raw[10]);
		bme280.offsets.P4 = (i16)((u16)raw[13] << 8 | raw[12]);
		bme280.offsets.P5 = (i16)((u16)raw[15] << 8 | raw[14]);
		bme280.offsets.P6 = (i16)((u16)raw[17] << 8 | raw[16]);
		bme280.offsets.P7 = (i16)((u16)raw[19] << 8 | raw[18]);
		bme280.offsets.P8 = (i16)((u16)raw[21] << 8 | raw[20]);
		bme280.offsets.P9 = (i16)((u16)raw[23] << 8 | raw[22]);

		u8 h1reg = 0xA1;
		ESP_ERROR_CHECK(i2c_master_transmit_receive(bme280.handle, &h1reg, 1, &bme280.offsets.H1, sizeof(bme280.offsets.H1), 100));

		u8 hreg = 0xE1;
		u8 hraw[7];
		ESP_ERROR_CHECK(i2c_master_transmit_receive(bme280.handle, &hreg, 1, hraw, sizeof(hraw), 100));
		bme280.offsets.H2 = (i16)((u16)hraw[1] << 8 | hraw[0]);
		bme280.offsets.H3 = hraw[2];
		bme280.offsets.H4 = (i16)((u16)hraw[3] << 4 | (hraw[4] & 0x0F));
		bme280.offsets.H5 = (i16)((u16)hraw[5] << 4 | (hraw[4] >> 4));
		bme280.offsets.H6 = (i8)hraw[6];
	}

	// set the humidity oversampling to x1
	// oversampling - how many times the sensors sample before giving a reading
	// x1 - one raw sample, higher = less noise but takes longer and uses more power
	{
		u8 buf[2] = {0xF2, 0x01};
		ESP_ERROR_CHECK(i2c_master_transmit(bme280.handle, buf, sizeof(buf), 100));
	}

	// set the temperature oversampling to x2
	// set the pressure oversampling to x1
	// set it to normal mode (auto toggles between measuring and stanby)
	{
		u8 buf[2] = {0xF4, 0x27};
		ESP_ERROR_CHECK(i2c_master_transmit(bme280.handle, buf, sizeof(buf), 100));
	}

	// set the standby time to 1000ms (1s wait in between measurements)
	// set the filter to off (give raw values, no smoothing of sudden spikes)
	{
		u8 buf[2] = {0xF5, 0xA0};
		ESP_ERROR_CHECK(i2c_master_transmit(bme280.handle, buf, sizeof(buf), 100));
	}

	return bme280;
}

/*
void GetWeatherReadings(BME280 *bme280) {
	{
		u8 reg = 0xF7;
		u8 raw[8];
		ESP_ERROR_CHECK(i2c_master_transmit_receive(bme280.handle, &reg, 1, raw, sizeof(raw), 100));

		// pressure - 20 bit
		i32 raw_p = (i32)((u32)raw[0] << 12 | (u32)raw[1] << 4 | raw[2] >> 4);
		// temperature - 20 bit
		i32 raw_t = (i32)((u32)raw[3] << 12 | (u32)raw[4] << 4 | raw[5] >> 4);
		// humidity - 16 bit
		i32 raw_h = (i32)((u32)raw[6] << 8 | raw[7]);
	}
}
*/

#endif
