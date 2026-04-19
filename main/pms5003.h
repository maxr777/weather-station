#include "driver/uart.h"
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

PMS5003 SetupPMS5003() {
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

	// "Stable data should be got at least 30 seconds after the sensor
	// wakeup from the sleep mode because of the fan’s performance."
	// 5 seconds should be OK
	vTaskDelay(pdMS_TO_TICKS(5000));

	PMS5003 pms = {};
	return pms;
}

bool ValidateChecksum(const u8 *buf) {
	int sum = 0;
	for (int i = 0; i < PMS5003_FRAME_SIZE - 2; ++i) {
		sum += buf[i];
	}

	int expected = ((u16)buf[PMS5003_FRAME_SIZE - 2] << 8 | buf[PMS5003_FRAME_SIZE - 1]);
	if (sum != expected) return false;

	return true;
}

typedef enum {
	OK,
	DIDNT_FIND_START_BYTES,
	INCOMPLETE_FRAME_ON_READ,
	INVALID_CHECKSUM,
} GetAirQualityErrorCodes;

const char *GetAirQualityErrorCodesToStr(const GetAirQualityErrorCodes code) {
	switch (code) {
	case OK:
		return "OK";
	case DIDNT_FIND_START_BYTES:
		return "DIDNT_FIND_START_BYTES";
	case INCOMPLETE_FRAME_ON_READ:
		return "INCOMPLETE_FRAME_ON_READ";
	case INVALID_CHECKSUM:
		return "INVALID_CHECKSUM";
	default:
		return "UNKNOWN";
	}
}

GetAirQualityErrorCodes GetAirQuality(PMS5003 *pms) {
	const int START1 = 0x42;
	const int START2 = 0x4d;

	u8 buf[PMS5003_BUF_SIZE] = {};

	// i < 64 to not loop forever
	bool match = false;
	bool found_start = false;
	for (int i = 0; i < 64; ++i) {
		u8 start;
		if (uart_read_bytes(PMS5003_UART_PORT, &start, sizeof(start), pdMS_TO_TICKS(100)) != 1) continue;
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

	if (!found_start) return DIDNT_FIND_START_BYTES;

	buf[0] = START1;
	buf[1] = START2;

	int len = uart_read_bytes(PMS5003_UART_PORT, buf + 2, PMS5003_FRAME_SIZE - 2, pdMS_TO_TICKS(100));
	if (len != PMS5003_FRAME_SIZE - 2) return INCOMPLETE_FRAME_ON_READ;

	if (!ValidateChecksum(buf))
		return INVALID_CHECKSUM;

	pms->pm10 = pms->data.data4;
	pms->pm25 = pms->data.data5;
	pms->pm100 = pms->data.data6;

	return OK;
}
