/////////////////////////////////////////////////////////////////////////////////
// File Name: spi_api.h
// Description: Header file for API specification
// Author: SEIKO EPSON (Modified by Sharon Bikobo)
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SPI_API_H_
#define _SPI_API_H_

/* STM32F746G-DISCO + Zephyr headers */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Device Tree Externs                                                        */
/* -------------------------------------------------------------------------- */
/* Note: 'epson_spi' is removed because we use manual config in spi_api.c now */

/* These allows main.c to check pin states if needed for debugging */
extern const struct gpio_dt_spec  epson_gpio_msgrdy;
extern const struct gpio_dt_spec  epson_gpio_stbyexit;
extern const struct gpio_dt_spec  epson_gpio_reset;
extern const struct gpio_dt_spec  epson_gpio_mute; 

/* -------------------------------------------------------------------------- */
/* Definitions                                                                */
/* -------------------------------------------------------------------------- */
#define SPI_MSGRDY_TIMEOUT  1

// Error definition for API function
#define SPIERR_TIMEOUT              1
#define SPIERR_SUCCESS              0
#define SPIERR_NULL_PTR             -1
#define SPIERR_GET_ERROR_CODE       -2
#define SPIERR_RESERVED_MESSAGE_ID  -3
#define SPIERR_ISC_VERSION_RESP     -4

/* -------------------------------------------------------------------------- */
/* Function Prototypes                                                        */
/* -------------------------------------------------------------------------- */

// Initialization
int EPSON_Initialize(void); 

// RUTRONIK MODIFICATION START
int S1V30340_Initialize_Audio_Config(void);
int S1V30340_Play_Specific_Audio(unsigned char aucIscSequencer_element);
int S1V30340_Wait_For_Termination(void);
int S1V30340_Stop_Specific_Audio(void);
// RUTRONIK MODIFICATION END

// Low Level SPI
unsigned char SPI_SendReceiveByte(unsigned char ucSendData);

// Message Handling
int SPI_SendMessage(unsigned char *pucSendMessage, unsigned short *pusReceivedMessageID);
int SPI_ReceiveMessage(unsigned short   *pusReceivedMessageID);

// Simple versions
int SPI_SendMessage_simple(unsigned char *pucSendMessage, int iSendMessageLength);
int SPI_ReceiveMessage_simple(unsigned short *pusReceivedMessageID);

// GPIO Controls
void GPIO_S1V30340_Reset(int iValue);
void GPIO_ControlStandby(int iValue);
void GPIO_ControlMute(int iValue);

// Test/Debug functions
unsigned short GetMessageErrorCode(void);
unsigned short GetBlockedMessageID(void);
unsigned short GetSequenceStatus(void);

#ifdef __cplusplus
}
#endif

#endif //!_SPI_API_H_