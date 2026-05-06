#ifndef PMS5003_H
#define PMS5003_H

#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "types.h"
#include <stdbool.h>

#define PMS5003_UART_PORT  UART_NUM_2
#define PMS5003_BAUD_RATE  9600
#define PMS5003_TX_PIN	   17
#define PMS5003_RX_PIN	   16
#define PMS5003_BUF_SIZE   1024
#define PMS5003_FRAME_SIZE 32

typedef struct {
	u16 frame_length;
	u16 data1;
	u16 data2;
	u16 data3;
	u16 data4;
	u16 data5;
	u16 data6;
	u16 data7;
	u16 data8;
	u16 data9;
	u16 data10;
	u16 data11;
	u16 data12;
	u16 data13;
	u16 checksum;
} PMS5003Data;

typedef struct {
	PMS5003Data data;
	int pm10;
	int pm25;
	int pm100;
} PMS5003;

static inline PMS5003Data ParsePMS5003Frame(const u8 *buf) {
	PMS5003Data data = {
	    .frame_length = (u16)((u16)buf[2] << 8 | buf[3]),
	    .data1 = (u16)((u16)buf[4] << 8 | buf[5]),
	    .data2 = (u16)((u16)buf[6] << 8 | buf[7]),
	    .data3 = (u16)((u16)buf[8] << 8 | buf[9]),
	    .data4 = (u16)((u16)buf[10] << 8 | buf[11]),
	    .data5 = (u16)((u16)buf[12] << 8 | buf[13]),
	    .data6 = (u16)((u16)buf[14] << 8 | buf[15]),
	    .data7 = (u16)((u16)buf[16] << 8 | buf[17]),
	    .data8 = (u16)((u16)buf[18] << 8 | buf[19]),
	    .data9 = (u16)((u16)buf[20] << 8 | buf[21]),
	    .data10 = (u16)((u16)buf[22] << 8 | buf[23]),
	    .data11 = (u16)((u16)buf[24] << 8 | buf[25]),
	    .data12 = (u16)((u16)buf[26] << 8 | buf[27]),
	    .data13 = (u16)((u16)buf[28] << 8 | buf[29]),
	    .checksum = (u16)((u16)buf[30] << 8 | buf[31]),
	};

	return data;
}

static inline bool ValidateChecksum(const u8 *buf) {
	int sum = 0;
	for (int i = 0; i < PMS5003_FRAME_SIZE - 2; ++i) {
		sum += buf[i];
	}

	int expected = ((u16)buf[PMS5003_FRAME_SIZE - 2] << 8 | buf[PMS5003_FRAME_SIZE - 1]);
	if (sum != expected) return false;

	return true;
}

typedef enum {
	PMS_OK,
	PMS_DIDNT_FIND_START_BYTES,
	PMS_INCOMPLETE_FRAME_ON_READ,
	PMS_INVALID_CHECKSUM,
} GetAirQualityErrorCodes;

static inline const char *GetAirQualityErrorCodesToStr(const GetAirQualityErrorCodes code) {
	switch (code) {
	case PMS_OK:
		return "PMS_OK";
	case PMS_DIDNT_FIND_START_BYTES:
		return "PMS_DIDNT_FIND_START_BYTES";
	case PMS_INCOMPLETE_FRAME_ON_READ:
		return "PMS_INCOMPLETE_FRAME_ON_READ";
	case PMS_INVALID_CHECKSUM:
		return "PMS_INVALID_CHECKSUM";
	default:
		return "UNKNOWN";
	}
}

// TODO: Actually test unplugging when it's running (since it's soldered on rn)
static inline GetAirQualityErrorCodes GetAirQuality(PMS5003 *pms) {
	const int START1 = 0x42;
	const int START2 = 0x4d;

	// It only holds one frame so I'm using the FRAME_SIZE
	// The BUF_SIZE constant is for the UART driver (how big the queue can get)
	u8 buf[PMS5003_FRAME_SIZE] = {};

	bool match = false;
	bool found_start = false;
	for (int i = 0; i < PMS5003_FRAME_SIZE + 1; ++i) {
		u8 start;
		if (uart_read_bytes(PMS5003_UART_PORT, &start, sizeof(start), pdMS_TO_TICKS(10)) != 1) continue;
		if (start == START1) {
			match = true;
			continue;
		}
		if (start == START2 && match) {
			found_start = true;
			break;
		}
		match = false;
	}

	if (!found_start) return PMS_DIDNT_FIND_START_BYTES;

	buf[0] = START1;
	buf[1] = START2;

	int len = uart_read_bytes(PMS5003_UART_PORT, buf + 2, PMS5003_FRAME_SIZE - 2, pdMS_TO_TICKS(100));
	if (len != PMS5003_FRAME_SIZE - 2) return PMS_INCOMPLETE_FRAME_ON_READ;

	if (!ValidateChecksum(buf))
		return PMS_INVALID_CHECKSUM;

	pms->data = ParsePMS5003Frame(buf);
	pms->pm10 = pms->data.data4;
	pms->pm25 = pms->data.data5;
	pms->pm100 = pms->data.data6;

	return PMS_OK;
}

static inline PMS5003 SetupPMS5003() {
	uart_config_t uart_config = {
	    .baud_rate = PMS5003_BAUD_RATE,
	    .data_bits = UART_DATA_8_BITS,
	    .parity = UART_PARITY_DISABLE,
	    .stop_bits = UART_STOP_BITS_1,
	    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	};
	ESP_ERROR_CHECK(uart_param_config(PMS5003_UART_PORT, &uart_config));

	// Using UART_PIN_NO_CHANGE for TX as I'm only reading from the PMS5003 - it's plugged in, though
	ESP_ERROR_CHECK(uart_set_pin(PMS5003_UART_PORT, UART_PIN_NO_CHANGE, PMS5003_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

	ESP_ERROR_CHECK(uart_driver_install(PMS5003_UART_PORT, PMS5003_BUF_SIZE * 2, 0, 0, NULL, 0));

	PMS5003 pms = {};

	// Do one read to check if the PMS works (is plugged in, for example)
	// If there's something wrong, restart (don't start with a broken sensor)
	vTaskDelay(pdMS_TO_TICKS(1000));
	{
		bool boot = false;
		for (int i = 0; i < 5; ++i) {
			if (GetAirQuality(&pms) != PMS_OK) {
				ESP_LOGW("PMS5003", "PMS5003 not found, retrying...");
				vTaskDelay(pdMS_TO_TICKS(250));
			} else {
				boot = true;
				break;
			}
		}
		if (!boot) esp_restart();
	}

	/*
	 * NOTE: I decided to disable it because I don't like waiting
	 * Even with 5 seconds of wait the readings would be unstable for a couple
	 * of seconds anyway, so it's not like waiting 5s in the setup helped anything
	 */

	// "Stable data should be got at least 30 seconds after the sensor
	// wakeup from the sleep mode because of the fan’s performance."
	// 4 seconds should be OK
	// vTaskDelay(pdMS_TO_TICKS(4000));

	return pms;
}

const char *GetPM10Quality(int q) {
	if (q <= 10) return "G";
	if (q <= 25) return "M";
	if (q <= 40) return "U1";
	if (q <= 100) return "U2";
	return "UH";
}

const char *GetPM25Quality(int q) {
	if (q <= 12) return "G";
	if (q <= 35.4) return "M";
	if (q <= 55.4) return "U1";
	if (q <= 150.4) return "U2";
	if (q <= 250.4) return "U3";
	return "HZ";
}

const char *GetPM100Quality(int q) {
	if (q <= 54) return "G";
	if (q <= 154) return "M";
	if (q <= 254) return "U1";
	if (q <= 354) return "U2";
	if (q <= 424) return "U3";
	return "HZ";
}

#endif
