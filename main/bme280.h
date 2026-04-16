#ifndef BME280_H
#define BME280_H

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "types.h"

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
	float temperature;
	float humidity;
	float pressure;
} BME280;

BME280 SetupBME280(int addr, i2c_master_bus_handle_t masterBusHandle) {
	BME280 bme = {};
	while (i2c_master_probe(masterBusHandle, addr, -1) != ESP_OK) {
		vTaskDelay(pdMS_TO_TICKS(250));
	}

	// init the device handle on i2c
	bme.config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
	bme.config.device_address = addr;
	bme.config.scl_speed_hz = 400000;

	ESP_ERROR_CHECK(i2c_master_bus_add_device(masterBusHandle, &bme.config, &bme.handle));

	// reset - write 0xB6 to 0xE0
	{
		uint8_t buf[2] = {0xE0, 0xB6};
		ESP_ERROR_CHECK(i2c_master_transmit(bme.handle, buf, 2, 100));

		// wait for the chip to come back
		vTaskDelay(pdMS_TO_TICKS(10));
	}

	// check chip id - read 0xD0, should be 0x60
	{
		uint8_t reg = 0xD0;
		uint8_t val = 0;
		ESP_ERROR_CHECK(i2c_master_transmit_receive(bme.handle, &reg, 1, &val, sizeof(val), 100));
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
		ESP_ERROR_CHECK(i2c_master_transmit_receive(bme.handle, &reg, 1, raw, sizeof(raw), 100));

		bme.offsets.T1 = (u16)((u16)raw[1] << 8 | raw[0]);
		bme.offsets.T2 = (i16)((u16)raw[3] << 8 | raw[2]);
		bme.offsets.T3 = (i16)((u16)raw[5] << 8 | raw[4]);
		bme.offsets.P1 = (u16)((u16)raw[7] << 8 | raw[6]);
		bme.offsets.P2 = (i16)((u16)raw[9] << 8 | raw[8]);
		bme.offsets.P3 = (i16)((u16)raw[11] << 8 | raw[10]);
		bme.offsets.P4 = (i16)((u16)raw[13] << 8 | raw[12]);
		bme.offsets.P5 = (i16)((u16)raw[15] << 8 | raw[14]);
		bme.offsets.P6 = (i16)((u16)raw[17] << 8 | raw[16]);
		bme.offsets.P7 = (i16)((u16)raw[19] << 8 | raw[18]);
		bme.offsets.P8 = (i16)((u16)raw[21] << 8 | raw[20]);
		bme.offsets.P9 = (i16)((u16)raw[23] << 8 | raw[22]);

		u8 h1reg = 0xA1;
		ESP_ERROR_CHECK(i2c_master_transmit_receive(bme.handle, &h1reg, 1, &bme.offsets.H1, sizeof(bme.offsets.H1), 100));

		u8 hreg = 0xE1;
		u8 hraw[7];
		ESP_ERROR_CHECK(i2c_master_transmit_receive(bme.handle, &hreg, 1, hraw, sizeof(hraw), 100));

		bme.offsets.H2 = (i16)((u16)hraw[1] << 8 | hraw[0]);
		bme.offsets.H3 = hraw[2];
		bme.offsets.H4 = (i16)((i8)hraw[3] << 4 | (hraw[4] & 0x0F));
		bme.offsets.H5 = (i16)((i8)hraw[5] << 4 | (hraw[4] >> 4));
		bme.offsets.H6 = (i8)hraw[6];
	}

	// set the humidity oversampling to x1
	// oversampling - how many times the sensors sample before giving a reading
	// x1 - one raw sample, higher = less noise but takes longer and uses more power
	{
		u8 buf[2] = {0xF2, 0x01};
		ESP_ERROR_CHECK(i2c_master_transmit(bme.handle, buf, sizeof(buf), 100));
	}

	// set the temperature oversampling to x2
	// set the pressure oversampling to x1
	// set it to normal mode (auto toggles between measuring and standby)
	{
		u8 buf[2] = {0xF4, 0x27};
		ESP_ERROR_CHECK(i2c_master_transmit(bme.handle, buf, sizeof(buf), 100));
	}

	// set the standby time to 1000ms (1s wait in between measurements)
	// set the filter to off (give raw values, no smoothing of sudden spikes)
	{
		u8 buf[2] = {0xF5, 0xA0};
		ESP_ERROR_CHECK(i2c_master_transmit(bme.handle, buf, sizeof(buf), 100));
	}

	return bme;
}

// temperature - returns degrees C * 100 (e.g. 2345 = 23.45°C)
i32 CompensateTemperature(BME280 *bme, i32 raw_t) {
	i32 var1, var2, T;

	var1 = ((((raw_t >> 3) - ((i32)bme->offsets.T1 << 1))) * ((i32)bme->offsets.T2)) >> 11;
	var2 = (((((raw_t >> 4) - ((i32)bme->offsets.T1)) * ((raw_t >> 4) - ((i32)bme->offsets.T1))) >> 12) *

		((i32)bme->offsets.T3)) >>
	       14;

	bme->t_fine = var1 + var2;

	T = (bme->t_fine * 5 + 128) >> 8;

	return T;
}

// pressure - returns Pa * 256
u32 CompensatePressure(BME280 *bme, i32 raw_p) {
	i64 var1, var2, p;
	var1 = ((i64)bme->t_fine) - 128000;

	var2 = var1 * var1 * (i64)bme->offsets.P6;
	var2 = var2 + ((var1 * (i64)bme->offsets.P5) << 17);
	var2 = var2 + (((i64)bme->offsets.P4) << 35);

	var1 = ((var1 * var1 * (i64)bme->offsets.P3) >> 8) + ((var1 * (i64)bme->offsets.P2) << 12);
	var1 = (((((i64)1) << 47) + var1)) * ((i64)bme->offsets.P1) >> 33;

	if (var1 == 0) {
		return 0; // avoid exception caused by division by zero
	}

	p = 1048576 - raw_p;
	p = (((p << 31) - var2) * 3125) / var1;

	var1 = (((i64)bme->offsets.P9) * (p >> 13) * (p >> 13)) >> 25;
	var2 = (((i64)bme->offsets.P8) * p) >> 19;

	p = ((p + var1 + var2) >> 8) + (((i64)bme->offsets.P7) << 4);

	return (u32)p;
}

// humidity - returns %RH * 1024
uint32_t CompensateHumidity(BME280 *bme, int32_t adc_H) {
	i32 v_x1_u32r;

	v_x1_u32r = (bme->t_fine - ((i32)76800));
	v_x1_u32r = (((((adc_H << 14) - (((i32)bme->offsets.H4) << 20) - (((i32)bme->offsets.H5) * v_x1_u32r)) +
		       ((i32)16384)) >>
		      15) *
		     (((((((v_x1_u32r *
			    ((i32)bme->offsets.H6)) >>
			   10) *
			  (((v_x1_u32r * ((i32)bme->offsets.H3)) >> 11) +
			   ((i32)32768))) >>
			 10) +
			((i32)2097152)) *
			   ((i32)bme->offsets.H2) +
		       8192) >>
		      14));

	v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
				   ((i32)bme->offsets.H1)) >>
				  4));

	v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);

	v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);

	return (u32)(v_x1_u32r >> 12);
}

void GetWeatherReadings(BME280 *bme) {
	u8 reg = 0xF7;
	u8 raw[8];
	ESP_ERROR_CHECK(i2c_master_transmit_receive(bme->handle, &reg, 1, raw, sizeof(raw), 100));

	// pressure - 20 bit
	i32 raw_p = (i32)((u32)raw[0] << 12 | (u32)raw[1] << 4 | raw[2] >> 4);
	// temperature - 20 bit
	i32 raw_t = (i32)((u32)raw[3] << 12 | (u32)raw[4] << 4 | raw[5] >> 4);
	// humidity - 16 bit
	i32 raw_h = (i32)((u32)raw[6] << 8 | raw[7]);

	bme->temperature = CompensateTemperature(bme, raw_t) / 100.0f;
	bme->humidity = CompensateHumidity(bme, raw_h) / 1024.0f;
	bme->pressure = CompensatePressure(bme, raw_p) / 256.0f;
}

#endif
