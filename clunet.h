#ifndef __CLUNET_H__
#define __CLUNET_H__

#include "clunet_config.h"
#include <inttypes.h>

#define STATE_IDLE		0x00
#define STATE_ACTIVE		0x01
#define STATE_READY 		0x02
#define STATE_PROCESS		0x04
#define STATE_BITSTUFFING	0x08
#define STATE_ERROR		0x10

#define CLUNET_OFFSET_SRC_ADDRESS 0
#define CLUNET_OFFSET_DST_ADDRESS 1
#define CLUNET_OFFSET_COMMAND 2
#define CLUNET_OFFSET_SIZE 3
#define CLUNET_OFFSET_DATA 4

#define CLUNET_BROADCAST_ADDRESS 0xff

// комманды:
#define CLUNET_COMMAND_DISCOVERY 0
#define CLUNET_COMMAND_DISCOVERY_RESPONSE 0x01
#define CLUNET_COMMAND_BOOT_CONTROL 0x02
#define CLUNET_COMMAND_REBOOT 0x03
#define CLUNET_COMMAND_BOOT_COMPLETED 0x04
#define CLUNET_COMMAND_EEPROM_ADDR 0xFA
#define CLUNET_COMMAND_EEPROM_ADDR_REPLY 0xFB
#define CLUNET_COMMAND_CURRENT_ADDR 0xFC
#define CLUNET_COMMAND_CURRENT_ADDR_REPLY 0xFD
#define CLUNET_COMMAND_DEVICE_NAME_SET 0xF8
#define CLUNET_COMMAND_DEVICE_NAME_SET_REPLY 0xF9
#define CLUNET_COMMAND_PING 0xFE
#define CLUNET_COMMAND_PING_REPLY 0xFF

// приоритеты:
#define CLUNET_PRIORITY_NOTICE 1
#define CLUNET_PRIORITY_INFO 2
#define CLUNET_PRIORITY_MESSAGE 3
#define CLUNET_PRIORITY_COMMAND 4


uint8_t debug_buffer[128];

uint8_t reading_state;
uint8_t sending_state;
uint8_t reading_buffer[CLUNET_READING_BUFFER_SIZE];
uint8_t sending_buffer[CLUNET_SENDING_BUFFER_SIZE];

uint8_t clunet_device_id;

void clunet_timer_int (void);
void clunet_sending_timer_int (void);
void clunet_pin_int (void);
void clunet_init(void);
void clunet_set_on_data_received(void (*f)(uint8_t src_address, uint8_t command, uint8_t* data, uint8_t size));
void clunet_set_on_data_received_sniff(void (*f)(uint8_t src_address, uint8_t dst_address, uint8_t command, uint8_t* data, uint8_t size));
void clunet_send0(const uint8_t src_address, const uint8_t dst_address, uint8_t prio, const uint8_t command, const uint8_t* data, const uint8_t size);
void clunet_send(const uint8_t dst_address, const uint8_t prio, const uint8_t command, const uint8_t* data, const uint8_t size);

#endif