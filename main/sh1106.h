#ifndef SH1106_H
#define SH1106_H

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "types.h"

#define SH1106_ADDR 0x3C

typedef struct {
	i2c_master_dev_handle_t handle;
	i2c_device_config_t config;
	bool (*screen)[64];
} SH1106;

static inline SH1106 SetupSH1106(i2c_master_bus_handle_t masterBusHandle) {
	SH1106 sh = {};

	while (i2c_master_probe(masterBusHandle, SH1106_ADDR, pdMS_TO_TICKS(100)) != ESP_OK) {
		ESP_LOGW("SH1106", "Display not found, retrying...");
		vTaskDelay(pdMS_TO_TICKS(250));
	}

	sh.screen = calloc(128, sizeof(bool[64]));

	sh.config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
	sh.config.device_address = SH1106_ADDR;
	sh.config.scl_speed_hz = 100000;

	ESP_ERROR_CHECK(i2c_master_bus_add_device(masterBusHandle, &sh.config, &sh.handle));

	static const uint8_t init_seq[] = {
	    0x00,	// Control byte: command stream
	    0xAE,	// Display OFF
	    0xD5, 0x80, // Clock divide ratio / oscillator frequency
	    0xA8, 0x3F, // Multiplex ratio: 64 rows
	    0xD3, 0x00, // Display offset: 0
	    0x40,	// Display start line: 0
	    0xAD, 0x8B, // DC-DC enable (internal VCC)
	    0xA1,	// Segment remap: col 131 -> SEG0 (flip horizontal)
	    0xC8,	// COM scan direction: remapped (flip vertical)
	    0xDA, 0x12, // COM pins hardware config: alternative
	    0x81, 0x80, // Contrast: 128/255
	    0xD9, 0x1F, // Pre-charge period
	    0xDB, 0x40, // VCOMH deselect level
	    0xA4,	// Output follows RAM (not entire display ON)
	    0xA6,	// Normal display (not inverted)
	    0xAF,	// Display ON
	};

	ESP_ERROR_CHECK(i2c_master_transmit(sh.handle, init_seq, sizeof(init_seq), pdMS_TO_TICKS(500)));

	return sh;
}

typedef enum {
	SH_OK,
	SH_COMMANDS_TRANSMIT_FAIL,
	SH_SCREEN_TRANSMIT_FAIL,
} SH1106FlushErrorCodes;

static inline const char *FlushErrorCodesToStr(const SH1106FlushErrorCodes code) {
	switch (code) {
	case SH_OK:
		return "SH_OK";
	case SH_COMMANDS_TRANSMIT_FAIL:
		return "SH_COMMANDS_TRANSMIT_FAIL";
	case SH_SCREEN_TRANSMIT_FAIL:
		return "SH_SCREEN_TRANSMIT_FAIL";
	default:
		return "UNKNOWN";
	}
}

static inline SH1106FlushErrorCodes SH1106Flush(SH1106 *sh) {
	// Each page is 8 rows tall, the SH1106 basically thinks in terms of display[8][128]
	for (int page = 0; page < 8; ++page) {
		u8 commands[] = {
		    // 0x00 indicates that the following are commands, not pixel data
		    0x00,
		    // 0xB0 = page 0, 0xB1 = page 1, ..., 0xB7 = page 7
		    (u8)(0xB0 | page),
		    // Column addres is split into two nibbles - high nibble command is 0x1X, and low nibble is 0x0X
		    // I want to write the whole buffer in one go, so I start at column 2 (because the internal RAM of
		    // the SH1106 is 132 clumns wide, so the 128 visible pixels start at column 2)
		    // low nibble:
		    0x02,
		    // high nibble:
		    0x10,
		};

		if (i2c_master_transmit(sh->handle, commands, sizeof(commands), pdMS_TO_TICKS(100)) != ESP_OK) return SH_COMMANDS_TRANSMIT_FAIL;

		// +1 because of the control byte, 0x40
		u8 data[128 + 1];
		// Control byte, means everything the follows is pixel data
		data[0] = 0x40;
		// Each iteration builds the byte below
		for (int x = 0; x < 128; ++x) {
			// Represents all 8 vertical pixels in the current page at column x, start with all off
			u8 byte = 0;
			for (int bit = 0; bit < 8; ++bit) {
				// Look the current pixel up - if it's on, set the corresponding bit in the byte
				// page * 8 + bit = page * 8 gets me to the first row of this page and + bit moves down to
				// the specific row within it. E.g. for page 2 and bit 3 the row is 19 (row = 2 * 8 + 3 = 19)
				if (sh->screen[x][page * 8 + bit])
					byte |= (1 << bit);
			}
			data[x + 1] = byte;
		}

		if (i2c_master_transmit(sh->handle, data, sizeof(data), pdMS_TO_TICKS(100)) != ESP_OK) return SH_SCREEN_TRANSMIT_FAIL;
	}

	return SH_OK;
}

#endif
