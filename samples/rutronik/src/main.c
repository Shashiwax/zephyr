/*
 * Simple Zephyr main for Epson S1V3G340 TTS
 * - Initializes the Epson chip
 * - Configures audio
 * - Plays one pre-registered phrase (song)
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/* --- Include Epson driver API (using your absolute paths) --- */
#include "C:\Users\sharo\zephyrproject\zephyr\samples\drivers\audio\RutEpsonDriver\s1v3g340\spi_api.h"
#include "C:\Users\sharo\zephyrproject\zephyr\samples\drivers\audio\RutEpsonDriver\s1v3g340\isc_msgs.h"

/* Index of the phrase/song stored in the Epson flash */
#define SONG_PHRASE_INDEX    2

int main(void)
{
    int result;
    int phrase = SONG_PHRASE_INDEX;

    printk("=== Epson S1V3G340 TTS Demo (Zephyr) ===\n");

    /* 1. Initialize SPI + GPIO for the Epson board */
    printk("[1] EPSON_Initialize...\n");
    result = EPSON_Initialize();
    if (result != 0) {
        printk("ERROR: EPSON_Initialize failed: %d\n", result);
        return 0;
    }
    printk("EPSON_Initialize OK.\n");

    /* 2. Hardware reset sequence (same as original main.c) */
    printk("[2] Hardware reset sequence...\n");
    GPIO_S1V30340_Reset(1);
    k_msleep(100);
    GPIO_S1V30340_Reset(0);
    k_msleep(100);
    GPIO_S1V30340_Reset(1);
    k_msleep(500);

    /* 3. Mute / standby signals like the original code */
    printk("[3] Configure MUTE and STBY...\n");
    GPIO_ControlMute(0);      /* MUTE = ON (silence) */
    GPIO_ControlStandby(0);   /* exit standby */
    k_msleep(100);

    /* 4. Send ISC init sequence: reset / test / version / audio config */
    printk("[4] S1V30340_Initialize_Audio_Config...\n");
    result = S1V30340_Initialize_Audio_Config();
    while (result != 0) {
        printk("S1V30340_Initialize_Audio_Config failed: %d, retrying...\n", result);
        k_msleep(1000);
        result = S1V30340_Initialize_Audio_Config();
    }
    printk("Audio configuration OK.\n");

    /* 5. Play one pre-registered phrase (“song”) */
    printk("[5] Playing phrase index %d...\n", phrase);
    GPIO_ControlMute(1);  /* MUTE OFF => audio enabled */

    S1V30340_Play_Specific_Audio((unsigned char)phrase);
    S1V30340_Wait_For_Termination();

    GPIO_ControlMute(0);  /* MUTE ON => silence again */
    printk("Playback finished.\n");

    /* 6. Idle forever */
    while (1) {
        k_msleep(1000);
    }

    return 0;
}
