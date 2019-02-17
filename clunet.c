#include "clunet.h"
#include "main.h"

static uint8_t device_name[DEVICE_NAME_MAX_SIZE];

/* для совместимости с clunet 2.0 оставим программный подсчет CRC */
static uint8_t _crc_ibutton_update(uint8_t crc, uint8_t data) {
    unsigned int i;
    crc = crc ^ data;
    for (i = 0; i < 8; i++) {
        if (crc & 0x01) {
            crc = (crc >> 1) ^ 0x8C;
        } else {
            crc >>= 1;
        }
    }
    return crc;
}

static void (*cb_data_received)(uint8_t src_address, uint8_t command, uint8_t* data, uint8_t size) = 0;
static void (*cb_data_received_sniff)(uint8_t src_address, uint8_t dst_address, uint8_t command, uint8_t* data, uint8_t size) = 0;

void clunet_set_on_data_received(void (*f)(uint8_t src_address, uint8_t command, uint8_t* data, uint8_t size)) {
    cb_data_received = f;
    return;
}

void clunet_set_on_data_received_sniff(void (*f)(uint8_t src_address, uint8_t dst_address, uint8_t command, uint8_t* data, uint8_t size)) {
    cb_data_received_sniff = f;
    return;
}

static void on_packet_received(void) {
    uint8_t src_address = reading_buffer[1+CLUNET_OFFSET_SRC_ADDRESS];
    uint8_t dst_address = reading_buffer[1+CLUNET_OFFSET_DST_ADDRESS];
    uint8_t command = reading_buffer[1+CLUNET_OFFSET_COMMAND];
    uint8_t *data_ptr = &reading_buffer[1+CLUNET_OFFSET_DATA];
    uint8_t data_size = reading_buffer[1+CLUNET_OFFSET_SIZE];
    
    if (cb_data_received_sniff) {
	(*cb_data_received_sniff)(src_address,dst_address,command,data_ptr,data_size);
    }
    if ((src_address != clunet_device_id) && ((dst_address == clunet_device_id) || (dst_address == CLUNET_BROADCAST_ADDRESS))) {
	if (command == CLUNET_COMMAND_REBOOT) {
	    IWDG->KR=0xCCCC;
	    while (1);
	}
	switch (command) {
	    case CLUNET_COMMAND_DISCOVERY:
		clunet_send(src_address, CLUNET_PRIORITY_MESSAGE, CLUNET_COMMAND_DISCOVERY_RESPONSE, device_name, DEVICE_NAME_MAX_SIZE);
		return;
	    case CLUNET_COMMAND_PING:
		clunet_send(src_address, CLUNET_PRIORITY_COMMAND, CLUNET_COMMAND_PING_REPLY, data_ptr, data_size);
		return;
	    case CLUNET_COMMAND_CURRENT_ADDR:
		if ((data_size==1)&&((uint8_t)*data_ptr!=CLUNET_BROADCAST_ADDRESS)) {
		    clunet_device_id=data_ptr[0];
		}
		clunet_send(src_address, CLUNET_PRIORITY_COMMAND, CLUNET_COMMAND_CURRENT_ADDR_REPLY,(const uint8_t *)&clunet_device_id, 1);
		return;
	    case CLUNET_COMMAND_EEPROM_ADDR:
		//if (data_size==1) {
		    //eeprom_write_byte((uint8_t*)EEPROM_CLUNET_ADDR,data_ptr[0]);
		//}
		//addr=eeprom_read_byte((uint8_t*)EEPROM_CLUNET_ADDR);
		//clunet_send(src_address, CLUNET_PRIORITY_COMMAND, CLUNET_COMMAND_EEPROM_ADDR_REPLY,(const char *)&addr, 1);
		return;
	    case CLUNET_COMMAND_DEVICE_NAME_SET:
		if (data_size>0) {
		    for (uint8_t i=0;i<DEVICE_NAME_MAX_SIZE;++i) {
		        if (i<data_size) {
			    device_name[i]=data_ptr[i];
			    //eeprom_write_byte((uint8_t*)(EEPROM_DEVICE_NAME_ADDR+i),data_ptr[i]);
		        } else {
			    device_name[i]=0x00;
			    //eeprom_write_byte((uint8_t*)(EEPROM_DEVICE_NAME_ADDR+i),0x00);
		        }
		    }
		}
		clunet_send(src_address, CLUNET_PRIORITY_MESSAGE, CLUNET_COMMAND_DEVICE_NAME_SET_REPLY, device_name, DEVICE_NAME_MAX_SIZE);
		return;
	}
	if (cb_data_received && (reading_buffer[1+CLUNET_OFFSET_DST_ADDRESS]==clunet_device_id)) {
    	    (*cb_data_received)(src_address,command,data_ptr,data_size);
	}
    }
    return;
}

#define BITSTUFF_SET1	sending_state|=STATE_BITSTUFFING
#define BITSTUFF_SET0	sending_state&=~STATE_BITSTUFFING
#define BITSTUFF	(sending_state&STATE_BITSTUFFING)
static uint8_t byte_mask;
static uint8_t byte_count;
static uint8_t bytes_limit;
static uint8_t crc;
static inline void next_bit() {
    byte_mask>>=1;
    if (!byte_mask) {
	if (byte_count&&(byte_count<bytes_limit)) {
    	    if (reading_state|STATE_ACTIVE) {
		crc=_crc_ibutton_update(crc,reading_buffer[byte_count]);
    	    } else if (sending_state|STATE_ACTIVE) {
    		//crc=_crc_ibutton_update(crc,sending_buffer[byte_count]);  // можно считать CRC перед отправкой и помещать в буфер, а можно во время отправки, здесь
    	    }
    	}
        byte_count++;
        byte_mask=0x80;
    }
    return;
}
static inline uint8_t cur_bit() {
    if (byte_mask&sending_buffer[byte_count]) {
        return 1;
    }
    return 0;
}
static uint8_t bit_out;
uint8_t count_bits() {
    bit_out=!bit_out;
    uint8_t bit_count=0;
    static uint8_t bit_in;
    if (BITSTUFF) {
        BITSTUFF_SET0;
        if (bit_in!=bit_out) {
            return 1;
        } else {
            ++bit_count;
        }
    }
    while (byte_count<bytes_limit) {
        ++bit_count;
        next_bit();
        bit_in = cur_bit();
        if (bit_count==5) {
            BITSTUFF_SET1;
            break;
        }
        if (bit_in!=bit_out) {
            break;
        }
    }
    return bit_count;
}
static inline void set_bit1() {
    reading_buffer[byte_count]|=byte_mask;
    return;
}

static inline void set_bit0() {
    reading_buffer[byte_count]&=~byte_mask;
    return;
}

void decount_bits(uint8_t ticks) {
    bit_out=!bit_out;
    if (BITSTUFF) {
        if (ticks<5) {
            BITSTUFF_SET0;
        }
        --ticks;
    } else {
        if (ticks>=5) {
            BITSTUFF_SET1;
        }
    }
    while (ticks--) {
        bit_out ? set_bit1() : set_bit0();
        next_bit();
    }
    return;
}

void clunet_timer_int(void) {

    /* тут надо переработать */
    if (!(reading_state & (STATE_READY|STATE_IDLE))) {
	reading_state=STATE_READY;
	//CLUNET_TIMER_REG=0;
	//CLUNET_TIMER_OCR=CLUNET_T;
	//CLUNET_TIMER_DISABLE;
    }

    if (sending_state==STATE_IDLE) {
	return;
    }
    if (reading_state&&!(reading_state|STATE_READY)) {
	return;
    }
    /* ********************** */
    
    if (sending_state==STATE_READY) {
	if (CLUNET_READ) {
	    CLUNET_TIMER_OCR=CLUNET_T*8;
	    return;
	}
	CLUNET_PIN_INT_DISABLE;
	reading_state=STATE_IDLE;
	sending_state=STATE_ACTIVE;
	CLUNET_OUT_SET0;
	byte_count=0;
	byte_mask=0x08;
	bit_out=0;
	bytes_limit=1+CLUNET_OFFSET_DATA+sending_buffer[1+CLUNET_OFFSET_SIZE]+1;
    }
	if (sending_state&STATE_ACTIVE) {
	    CLUNET_OUT_TOGGLE;
	    uint8_t cb=count_bits();
	    if (cb) {
		CLUNET_TIMER_OCR=CLUNET_T*cb;
		return;
	    } else {
		if (bit_out) {
		    CLUNET_TIMER_OCR=CLUNET_T*1;
		    return;
		} else {
		    CLUNET_OUT_SET0;
		    CLUNET_TIMER_DISABLE;
		    sending_state=STATE_IDLE;
		    CLUNET_PIN_INT_ENABLE;
		    reading_state=STATE_READY;
		}
	    }
	}
    return;
}

void clunet_pin_int(void) {
    if (reading_state&STATE_IDLE) return;
    if (reading_state & STATE_READY) { // если поступил первый бит, подготавливаемся
	reading_state = STATE_ACTIVE;
	CLUNET_TIMER_REG=0;
	CLUNET_TIMER_OCR=CLUNET_T*8;
	CLUNET_TIMER_RESET_FLAG;
	CLUNET_TIMER_ENABLE;
        byte_count=0;
        byte_mask=0x08;
        crc=0;
        bytes_limit=CLUNET_READING_BUFFER_SIZE;
        bit_out=0;
        return;
    }
    if (reading_state & STATE_ACTIVE) { // если в процессе чтения
        const uint8_t ticks = CLUNET_TIMER_REG;
        if ((ticks >= CLUNET_T/2)&&(ticks < (5*CLUNET_T+CLUNET_T/2))) { // отбрасываем слишком котткие и слишком длинные
	    CLUNET_TIMER_REG=0;
	    uint8_t bits=(ticks+(CLUNET_T/2))/CLUNET_T;
	    decount_bits(bits);
	    if (byte_count==1+CLUNET_OFFSET_SIZE+1) {
		bytes_limit=1+reading_buffer[1+CLUNET_OFFSET_SIZE]+CLUNET_OFFSET_DATA;
		reading_buffer[12]=bytes_limit;
		return;
	    }
	    if (byte_count>bytes_limit) {
		if (crc == reading_buffer[bytes_limit]) {
		    reading_state=STATE_PROCESS;
		    on_packet_received();
		} else {
		    reading_state=STATE_PROCESS|STATE_ERROR;
		}
	    }
	}
    }
    return;
}

void clunet_init(void) {
    reading_state=STATE_READY;
    sending_state=STATE_IDLE;
    CLUNET_TIMER_OCR=CLUNET_T;
    CLUNET_TIMER_DISABLE;
    clunet_device_id=CLUNET_DEVICE_ID;
    /* подготовить таймеры и прерывания */
    return;
}

void clunet_send(const uint8_t dst_address, uint8_t prio, const uint8_t command, const uint8_t* data, const uint8_t size) {
    clunet_send0(clunet_device_id, dst_address, prio, command, data, size);
    return;
}

void clunet_send0(const uint8_t src_address, const uint8_t dst_address, uint8_t prio, const uint8_t command, const uint8_t* data, const uint8_t size) {
    prio = (prio > 8) ? 8 : prio ? : 1;
    prio--;
    sending_buffer[0]=prio|0x08;
    uint8_t crc=0;
    crc=_crc_ibutton_update(crc,src_address);
    sending_buffer[1+CLUNET_OFFSET_SRC_ADDRESS]=src_address;
    crc=_crc_ibutton_update(crc,dst_address);
    sending_buffer[1+CLUNET_OFFSET_DST_ADDRESS]=dst_address;
    crc=_crc_ibutton_update(crc,command);
    sending_buffer[1+CLUNET_OFFSET_COMMAND]=command;
    crc=_crc_ibutton_update(crc,size);
    sending_buffer[1+CLUNET_OFFSET_SIZE]=size;
    for (uint8_t i=0;i<size;++i) {
	crc=_crc_ibutton_update(crc,data[i]);
	sending_buffer[1+CLUNET_OFFSET_DATA+i]=data[i];
    }
    sending_buffer[1+CLUNET_OFFSET_DATA+size]=crc;
    sending_state=STATE_READY;
    CLUNET_TIMER_REG=0;
    CLUNET_TIMER_RESET_FLAG;
    CLUNET_TIMER_OCR=CLUNET_T*8;
    CLUNET_TIMER_ENABLE;
    return;
}