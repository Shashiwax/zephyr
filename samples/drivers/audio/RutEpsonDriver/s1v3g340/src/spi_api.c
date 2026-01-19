/////////////////////////////////////////////////////////////////////////////////
// File Name: spi_api.c
// Description: Zephyr Adaptation for Epson S1V3G340 API (Manual Config + Debug)
/////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/printk.h> 

#include "../spi_api.h"
#include "../isc_msgs.h"

/* -------------------------------------------------------------------------- */
/* GLOBAL VARIABLES                                                           */
/* -------------------------------------------------------------------------- */

#define MAX_RECEIVED_DATA_LEN   20

static unsigned char    aucReceivedData[MAX_RECEIVED_DATA_LEN];
static unsigned short   usMessageErrorCode;
static unsigned short   usBlockedMessageID;
static unsigned short   usSequenceStatus;

uint8_t check_spi_rx[30];
uint8_t check_spi_tx[30];
int i = 0;

/* -------------------------------------------------------------------------- */
/* HARDWARE DEFINITIONS                                                       */
/* -------------------------------------------------------------------------- */

/* 1. Define Nodes */
#define GPIO_NODE  DT_PATH(zephyr_user)
#define SPI_BUS_NODE DT_NODELABEL(spi2) 

/* 2. Global Driver Objects (Manual Configuration) */
const struct device *spi_dev;         
struct spi_config epson_spi_cfg;      
struct spi_cs_control epson_cs_ctrl;  

/* 3. Define GPIO Specs from Device Tree */
const struct gpio_dt_spec epson_gpio_msgrdy   = GPIO_DT_SPEC_GET(GPIO_NODE, epson_msgrdy_gpios);
const struct gpio_dt_spec epson_gpio_stbyexit = GPIO_DT_SPEC_GET(GPIO_NODE, epson_stby_gpios);
const struct gpio_dt_spec epson_gpio_reset    = GPIO_DT_SPEC_GET(GPIO_NODE, epson_reset_gpios);
const struct gpio_dt_spec epson_gpio_mute     = GPIO_DT_SPEC_GET(GPIO_NODE, epson_mute_gpios);
const struct gpio_dt_spec epson_gpio_cs       = GPIO_DT_SPEC_GET(GPIO_NODE, epson_cs_gpios); 

/* -------------------------------------------------------------------------- */
/* CONFIGURATION ARRAYS                                                       */
/* -------------------------------------------------------------------------- */

unsigned char aucIscSequencerConfigReq_S_0001[] = {
    0x00, 0xAA, 0x10, 0x00, 0xC4, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00,
};
int iIscSequencerConfigReqLen_S_0001 = sizeof(aucIscSequencerConfigReq_S_0001);

unsigned char aucIscSequencerConfigReq_S_0002[] = {
    0x00, 0xAA, 0x10, 0x00, 0xC4, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00,
};
int iIscSequencerConfigReqLen_S_0002 = sizeof(aucIscSequencerConfigReq_S_0002);

unsigned char aucIscSequencerConfigReq_S_0003[] = {
    0x00, 0xAA, 0x10, 0x00, 0xC4, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x02, 0x00,
};
int iIscSequencerConfigReqLen_S_0003 = sizeof(aucIscSequencerConfigReq_S_0003);

unsigned char aucIscSequencerConfigReq_S_0004[] = {
    0x00, 0xAA, 0x10, 0x00, 0xC4, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x03, 0x00,
};
int iIscSequencerConfigReqLen_S_0004 = sizeof(aucIscSequencerConfigReq_S_0004);

unsigned char aucIscSequencerConfigReq_S_0005[] = {
    0x00, 0xAA, 0x10, 0x00, 0xC4, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x04, 0x00,
};
int iIscSequencerConfigReqLen_S_0005 = sizeof(aucIscSequencerConfigReq_S_0005);


/* -------------------------------------------------------------------------- */
/* EPSON_Initialize                                                           */
/* Description: Detailed debug version to find the crash point                */
/* -------------------------------------------------------------------------- */
int EPSON_Initialize(void)
{
    printk("DEBUG: > EPSON_Initialize Start\n");

    /* A. Initialize SPI Bus Pointer */
    spi_dev = DEVICE_DT_GET(SPI_BUS_NODE);
    if (!device_is_ready(spi_dev)) {
        printk("CRITICAL ERROR: SPI Bus not ready!\n");
        return -1;
    }
    printk("DEBUG: > SPI Bus Found\n");

    /* B. Configure SPI Settings */
    epson_cs_ctrl.gpio = epson_gpio_cs;
    epson_cs_ctrl.delay = 0;

    epson_spi_cfg.frequency = 2000000; 
    epson_spi_cfg.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB;
    /*epson_spi_cfg.operation |= SPI_MODE_CPOL | SPI_MODE_CPHA;*/

    epson_spi_cfg.slave = 0;
    epson_spi_cfg.cs = epson_cs_ctrl;

    /* C. Initialize GPIOs */
    printk("DEBUG: > Configuring GPIOs...\n");

    // MSGRDY pin: input
    gpio_pin_configure_dt(&epson_gpio_msgrdy, GPIO_INPUT);

    // STBY pin: output, initial = true (high => out of standby)
    gpio_pin_configure_dt(&epson_gpio_stbyexit, GPIO_OUTPUT_ACTIVE);

    // RESET pin: output, not asserted (= inactive)
    // Assuming reset is declared GPIO_ACTIVE_LOW in DTS, INACTIVE => physical high
    gpio_pin_configure_dt(&epson_gpio_reset, GPIO_OUTPUT_INACTIVE);

    // MUTE pin: output, initial = false (low => unmuted)
    gpio_pin_configure_dt(&epson_gpio_mute, GPIO_OUTPUT_INACTIVE);

    // CS pin: output, initial = true (deselected)
    // Assuming CS is GPIO_ACTIVE_LOW in DTS, INACTIVE => physical high
    gpio_pin_configure_dt(&epson_gpio_cs, GPIO_OUTPUT_INACTIVE);

    return 0;

}


/* -------------------------------------------------------------------------- */
/* S1V30340_Initialize_Audio_Config                                           */
/* Description: Debug version with Loop Counter                               */
/* -------------------------------------------------------------------------- */
int S1V30340_Initialize_Audio_Config(void)
{
    unsigned short usReceivedMessageID;
    int iError = 0;
    int message_ready = 0;
    bool Request_Response_Success = false;
    int retry_count = 0;

    printk("DEBUG: > Starting Protocol Initialization...\n");

    /* --- 1. Send ISC_RESET_REQ --- */
    printk("DEBUG: > Sending ISC_RESET_REQ...\n");
    
    while(Request_Response_Success == false)
    {
        /* Send command (blindly) */
        SPI_SendMessage(aucIscResetReq, &usReceivedMessageID);
        
        /* Check Ready Pin */
        message_ready = gpio_pin_get_dt(&epson_gpio_msgrdy);
        
        /* Print status every ~1 second so we know it's alive */
        if (retry_count++ % 1000 == 0) {
             printk("DEBUG: Loop %d - Waiting for MSGRDY (D3). State: %d\n", retry_count, message_ready);
        }

        if (message_ready == 1)
        {
            printk("DEBUG: MSGRDY High! Reading response...\n");
            iError = SPI_ReceiveMessage(&usReceivedMessageID);
            
            if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_RESET_RESP) {
                printk("DEBUG: Reset Response Error: %d ID: 0x%X\n", iError, usReceivedMessageID);
                return iError;
            } else {
                printk("DEBUG: Reset Response OK\n");
                Request_Response_Success = true;
            }
        }
        k_msleep(1);
    }

    /* --- 2. Registry key-code --- */
    Request_Response_Success = false;
    printk("DEBUG: > Sending ISC_TEST_REQ (Keycode)...\n");
    while(Request_Response_Success == false)
    {
        SPI_SendMessage(aucIscTestReq, &usReceivedMessageID);
        message_ready = gpio_pin_get_dt(&epson_gpio_msgrdy);
        
        if (message_ready == 1) {
            iError = SPI_ReceiveMessage(&usReceivedMessageID);
            if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_TEST_RESP) {
                return iError;
            } else {
                Request_Response_Success = true;
            }
        }
        k_msleep(1);
    }

    /* --- 3. Get version info --- */
    Request_Response_Success = false;
    while(Request_Response_Success == false)
    {
        SPI_SendMessage(aucIscVersionReq, &usReceivedMessageID);
        message_ready = gpio_pin_get_dt(&epson_gpio_msgrdy);
        
        if (message_ready == 1) {
            iError = SPI_ReceiveMessage(&usReceivedMessageID);
            if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_VERSION_RESP) {
                return iError;
            } else {
                Request_Response_Success = true;
            }
        }
        k_msleep(1);
    }

    /* --- 4. Set volume & sampling freq --- */
    Request_Response_Success = false;
    while(Request_Response_Success == false)
    {
        SPI_SendMessage(aucIscAudioConfigReq, &usReceivedMessageID);
        message_ready = gpio_pin_get_dt(&epson_gpio_msgrdy);
        
        if (message_ready == 1) {
            iError = SPI_ReceiveMessage(&usReceivedMessageID);
            if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_AUDIO_CONFIG_RESP) {
                return iError;
            } else {
                Request_Response_Success = true;
            }
        }
        k_msleep(1);
    }

    return iError;
}

/* -------------------------------------------------------------------------- */
/* S1V30340_Play_Specific_Audio                                               */
/* -------------------------------------------------------------------------- */
int S1V30340_Play_Specific_Audio(unsigned char aucIscSequencer_element)
{
    unsigned short usReceivedMessageID;
    int iError = 0;
    int message_ready;
    bool Request_Response_Success = false;

    unsigned char aucIscSequencerConfig[] = {
        0x00, 0xAA, 0x10, 0x00, 0xC4, 0x00, 0x01, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00,
    };

    aucIscSequencerConfig[16] = aucIscSequencer_element;

    /* Send Config */
    while(Request_Response_Success == false)
    {
        SPI_SendMessage(aucIscSequencerConfig, &usReceivedMessageID);
        message_ready = gpio_pin_get_dt(&epson_gpio_msgrdy);
        
        if (message_ready == 1) {
            iError = SPI_ReceiveMessage(&usReceivedMessageID);
            if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_SEQUENCER_CONFIG_RESP) {
                return iError;
            } else {
                Request_Response_Success = true;
            }
        }
        k_msleep(1);
    }
    aucIscSequencerStartReq[6] = 0;
    Request_Response_Success = false;

    /* Send Start */
    while(Request_Response_Success == false)
    {
        SPI_SendMessage(aucIscSequencerStartReq, &usReceivedMessageID);
        message_ready = gpio_pin_get_dt(&epson_gpio_msgrdy);
        
        if (message_ready == 1) {
            iError = SPI_ReceiveMessage(&usReceivedMessageID);
            if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_SEQUENCER_START_RESP) {
                return iError;
            } else {
                Request_Response_Success = true;
            }
        }
        k_msleep(1);
    }
    return iError;
}


/* -------------------------------------------------------------------------- */
/* S1V30340_Wait_For_Termination                                              */
/* -------------------------------------------------------------------------- */
int S1V30340_Wait_For_Termination(void)
{
    unsigned short usReceivedMessageID;
    int iError = 0;
    int message_ready;

    do {
        message_ready = gpio_pin_get_dt(&epson_gpio_msgrdy);
        if (message_ready == 1) {
            iError = SPI_ReceiveMessage(&usReceivedMessageID);
        }

        if (iError < SPIERR_SUCCESS || usReceivedMessageID == ID_ISC_MSG_BLOCKED_RESP) {
            return iError;
        }
        k_msleep(1);
    } while (usReceivedMessageID != ID_ISC_SEQUENCER_STATUS_IND);

    return iError;
}

/* -------------------------------------------------------------------------- */
/* S1V30340_Stop_Specific_Audio                                               */
/* -------------------------------------------------------------------------- */
int S1V30340_Stop_Specific_Audio(void)
{
    unsigned short usReceivedMessageID;
    int iError = 0;
    int message_ready;
    bool Request_Response_Success = false;

    while(Request_Response_Success == false)
    {
        SPI_SendMessage(aucIscSequencerStopReq, &usReceivedMessageID);
        message_ready = gpio_pin_get_dt(&epson_gpio_msgrdy);
        
        if (message_ready == 1) {
            iError = SPI_ReceiveMessage(&usReceivedMessageID);
            if (iError < SPIERR_SUCCESS || usReceivedMessageID != ID_ISC_SEQUENCER_STOP_RESP) {
                return iError;
            } else {
                Request_Response_Success = true;
            }
        }
        k_msleep(1);
    }
    return iError;
}

/* -------------------------------------------------------------------------- */
/* SPI_SendReceiveByte                                                        */
/* -------------------------------------------------------------------------- */

unsigned char SPI_SendReceiveByte(unsigned char ucSendData)
{
    unsigned char ucReceivedData = 0x0;
    uint8_t spi_rx = 0;
    uint8_t spi_tx = 0x00;

    /* RUTRONIK Manipulation of Driver */
    spi_tx = ucSendData;
    
    /* Safety Check: Don't write outside the array */
    if (i < 30) {
        check_spi_tx[i] = spi_tx;
    }

    /* 3. Adaptation Zephyr (Buffers obligatoires) */
    struct spi_buf tx_buf = { .buf = &spi_tx, .len = 1 };
    struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };

    struct spi_buf rx_buf = { .buf = &spi_rx, .len = 1 };
    struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1 };

    // Assert CS (low)
    gpio_pin_set_dt(&epson_gpio_cs, 1);
    //gpio_pin_set_dt(&epson_gpio_cs, 0); 
    
    /* Optimization: Use busy_wait (microseconds) instead of sleep (milliseconds) for SPI */
    k_msleep(10); 
    
    /* Transfer */
    spi_transceive(spi_dev, &epson_spi_cfg, &tx_set, &rx_set);

    // Assert CS (high)
    gpio_pin_set_dt(&epson_gpio_cs, 0);
    //gpio_pin_set_dt(&epson_gpio_cs, 1); 

    ucReceivedData = spi_rx;
    
    /* Safety Check: Update RX array and wrap 'i' */
    if (i < 30) {
        check_spi_rx[i] = spi_rx;
        i++;
    } else {
        i = 0; // Reset index to prevent crash
    }
    printk("SPI Received data: 0x%02X ucReceivedData");
    return ucReceivedData;
}

/* -------------------------------------------------------------------------- */
/* SPI_SendMessage / Receive (Generic Helpers)                                */
/* -------------------------------------------------------------------------- */

int SPI_SendMessage_simple(unsigned char *pucSendMessage, int iSendMessageLength)
{
    int j;
    if (pucSendMessage == NULL) return SPIERR_NULL_PTR;

    for (j = 0; j < iSendMessageLength; j++) {
        SPI_SendReceiveByte(*pucSendMessage++);
    }
    return SPIERR_SUCCESS;
}


int SPI_SendMessage(unsigned char *pucSendMessage, unsigned short *pusReceivedMessageID)
{
    unsigned char ucReceivedData;
    unsigned short usSendLength;
    int iReceivedCounts = 0;
    unsigned short usReceiveLength = 0;

    if (pucSendMessage == NULL || pusReceivedMessageID == NULL) return SPIERR_NULL_PTR;

    *pusReceivedMessageID = 0xFFFF;
    usSendLength = pucSendMessage[3];
    usSendLength = (usSendLength << 8) | pucSendMessage[2];
    usSendLength += HEADER_LEN; 

    while (usSendLength > 0 || usReceiveLength > 0)
    {
        if (usSendLength == 0) {
            ucReceivedData = SPI_SendReceiveByte(0);
        } else {
            ucReceivedData = SPI_SendReceiveByte(*pucSendMessage++);
            usSendLength--;
        }

        if (usReceiveLength == 0 && ucReceivedData == 0xAA) {
            usReceiveLength = 2;
        } else if (usReceiveLength > 0) {
            if (iReceivedCounts < MAX_RECEIVED_DATA_LEN) {
                aucReceivedData[iReceivedCounts] = ucReceivedData;
            }
            if (iReceivedCounts == 1) {
                usReceiveLength = ucReceivedData;
                usReceiveLength = (usReceiveLength << 8) | aucReceivedData[iReceivedCounts-1];
                usReceiveLength -= 2;
            }
            iReceivedCounts++;
            usReceiveLength--;
        }
    }

    if (iReceivedCounts > 0) {
        unsigned short usId;
        usId = aucReceivedData[3];
        usId = (usId << 8) | aucReceivedData[2];
        *pusReceivedMessageID = usId;
    }
    return SPIERR_SUCCESS;
}


int SPI_ReceiveMessage_simple(unsigned short *pusReceivedMessageID)
{
    int j;
    unsigned char aucHeader[2];
    unsigned char usTmp;
    unsigned short usLen = 0;
    unsigned short usId = 0x0;

    if (pusReceivedMessageID == NULL) return SPIERR_NULL_PTR;

    *pusReceivedMessageID = 0xFFFF;
    aucHeader[0] = SPI_SendReceiveByte(0);
    aucHeader[1] = SPI_SendReceiveByte(0);

    while (1) {
        if (aucHeader[0] == 0x00 && aucHeader[1] == 0xaa) {
            usTmp = SPI_SendReceiveByte(0);
            usLen = SPI_SendReceiveByte(0);
            usLen = (usLen << 8) + usTmp;
            usTmp = SPI_SendReceiveByte(0);
            usId  = SPI_SendReceiveByte(0);
            usId  = (usId << 8) + usTmp;
            for (j = 4; j < usLen; j++) {
                SPI_SendReceiveByte(0);
            }
            break;
        }
        aucHeader[0] = aucHeader[1];
        aucHeader[1] = SPI_SendReceiveByte(0);
    }
    *pusReceivedMessageID = usId;
    return SPIERR_SUCCESS;
}

const unsigned char aucIscVersionResp[LEN_ISC_VERSION_RESP] = {
    0x14, 0x00, 0x06, 0x00, 0x01, 0x00, 0x01, 0x00, 
    0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 
};

int SPI_ReceiveMessage(unsigned short	*pusReceivedMessageID)
{
	unsigned short	i;
	unsigned char	aucHeader[2];
	unsigned char	usTmp;
	unsigned short	usLen = 0;
	unsigned short	usId = 0x0;
	int				iReceivedCounts = 0;
	int				iTimeOut = 0;

	if (pusReceivedMessageID == NULL)
	{
		return SPIERR_NULL_PTR;
	}

	*pusReceivedMessageID = 0xFFFF;
	usMessageErrorCode = 0;
	usBlockedMessageID = 0;

	aucHeader[0] = SPI_SendReceiveByte(0);
	aucHeader[1] = SPI_SendReceiveByte(0);

	while (1)
	{

		if (aucHeader[0] == 0x00 && aucHeader[1] == 0xaa)
		{
			usTmp = aucReceivedData[iReceivedCounts++] = SPI_SendReceiveByte(0);
			usLen = aucReceivedData[iReceivedCounts++] = SPI_SendReceiveByte(0);
			usLen = (usLen << 8) + usTmp;
			usTmp = aucReceivedData[iReceivedCounts++] = SPI_SendReceiveByte(0);
			usId  = aucReceivedData[iReceivedCounts++] = SPI_SendReceiveByte(0);
			usId  = (usId << 8) + usTmp;

			for (i = 4; i < usLen; i++)
			{
				if (iReceivedCounts < MAX_RECEIVED_DATA_LEN)
				{
					aucReceivedData[iReceivedCounts++] = SPI_SendReceiveByte(0);
				}

				else
				{
					SPI_SendReceiveByte(0);
				}
			}

			break;
		}

		aucHeader[0] = aucHeader[1];
		aucHeader[1] = SPI_SendReceiveByte(0);

		if (iTimeOut >=  SPI_MSGRDY_TIMEOUT)
		{
			return SPIERR_TIMEOUT;
		}

		iTimeOut++;
	}

	*pusReceivedMessageID = usId;

	// check RESP or IND message.

	switch (usId)
	{
	case ID_ISC_RESET_RESP://4
	case ID_ISC_AUDIO_PAUSE_IND://4
	case ID_ISC_PMAN_STANDBY_EXIT_IND://4
	case ID_ISC_UART_CONFIG_RESP://4
	case ID_ISC_UART_RCVRDY_IND://4
	case ID_ISC_AUDIODEC_READY_IND://0x11->17
		break;
	case ID_ISC_TEST_RESP://6
	case ID_ISC_ERROR_IND://6
	case ID_ISC_AUDIO_CONFIG_RESP://6
	case ID_ISC_AUDIO_VOLUME_RESP://6
	case ID_ISC_AUDIO_MUTE_RESP://6
	case ID_ISC_PMAN_STANDBY_ENTRY_RESP://6
	case ID_ISC_AUDIODEC_CONFIG_RESP://6
	case ID_ISC_AUDIODEC_DECODE_RESP://6
	case ID_ISC_AUDIODEC_PAUSE_RESP://6
	case ID_ISC_AUDIODEC_ERROR_IND://6
	case ID_ISC_SEQUENCER_CONFIG_RESP://6
	case ID_ISC_SEQUENCER_START_RESP://6
	case ID_ISC_SEQUENCER_STOP_RESP://6
	case ID_ISC_SEQUENCER_PAUSE_RESP://6
	case ID_ISC_SEQUENCER_ERROR_IND://6
	case ID_ISC_AUDIODEC_STOP_RESP://20
		usMessageErrorCode = aucReceivedData[5];
		usMessageErrorCode = (usMessageErrorCode << 8) + aucReceivedData[4];
		break;

	case ID_ISC_SEQUENCER_STATUS_IND://6
		usSequenceStatus = aucReceivedData[5];
		usSequenceStatus = (usSequenceStatus << 8) + aucReceivedData[4];
		break;

	case ID_ISC_VERSION_RESP://20
		for (i = 0; i < LEN_ISC_VERSION_RESP; i++)
		{
			if (aucReceivedData [i] != aucIscVersionResp[i])
			{
				return SPIERR_ISC_VERSION_RESP;
				break;
			}
		}
		break;

	case ID_ISC_MSG_BLOCKED_RESP://8
		usBlockedMessageID = aucReceivedData[5];
		usBlockedMessageID = (usBlockedMessageID << 8) + aucReceivedData[4];
		usMessageErrorCode = aucReceivedData[7];
		usMessageErrorCode = (usMessageErrorCode << 8) + aucReceivedData[6];
		break;

	default:
		return SPIERR_RESERVED_MESSAGE_ID;
		break;

	}

	if (usMessageErrorCode != 0)
	{
		return SPIERR_GET_ERROR_CODE;
	}

	return SPIERR_SUCCESS;
}

/* -------------------------------------------------------------------------- */
/* GPIO Controls                                                              */
/* -------------------------------------------------------------------------- */
void GPIO_S1V30340_Reset(int iValue) {
    gpio_pin_set_dt(&epson_gpio_reset, iValue);
}

void GPIO_ControlStandby(int iValue) {
    gpio_pin_set_dt(&epson_gpio_stbyexit, iValue);
}

void GPIO_ControlMute(int iValue) {
    gpio_pin_set_dt(&epson_gpio_mute, iValue);
}


unsigned short GetMessageErrorCode(void) { return usMessageErrorCode; }
unsigned short GetBlockedMessageID(void) { return usBlockedMessageID; }
unsigned short GetSequenceStatus(void)   { return usSequenceStatus; }