/* boards/16.h */


#define BRICKPICO_MODEL     "16"

#define OUTPUT_COUNT     16   /* Number of PWM outputs on the board */

#ifdef LIB_PICO_CYW43_ARCH
#define LED_PIN -1
#else
#define LED_PIN 25
#endif


/* Pins for PWM outputs. */

#define PWM1_PIN       16  /* PWM0A */
#define PWM2_PIN       17  /* PWM0B */
#define PWM3_PIN       18  /* PWM1A */
#define PWM4_PIN       19  /* PWM1B */
#define PWM5_PIN       20  /* PWM2A */
#define PWM6_PIN       21  /* PWM2B */
#define PWM7_PIN        6  /* PWM3A */
#define PWM8_PIN        7  /* PWM3B */
#define PWM9_PIN        8  /* PWM4A */
#define PWM10_PIN       9  /* PWM4B */
#define PWM11_PIN      10  /* PWM5A */
#define PWM12_PIN      11  /* PWM5B */
#define PWM13_PIN      12  /* PWM6A */
#define PWM14_PIN      13  /* PWM6B */
#define PWM15_PIN      14  /* PWM7A */
#define PWM16_PIN      15  /* PWM7B */


/* Interface Pins */

/* I2C */
#define I2C_HW          2  /* 1=i2c0, 2=i2c1, 0=bitbang... */
#define SDA_PIN        26
#define SCL_PIN        27

/* Serial */
#define TX_PIN          0
#define RX_PIN          1

/* SPI */
#define SCK_PIN         2
#define MOSI_PIN        3
#define MISO_PIN        4
#define CS_PIN          5

#define DC_PIN         22
#define LCD_RESET_PIN  28
#define LCD_LIGHT_PIN  -1


#define OLED_DISPLAY 1
#define LCD_DISPLAY 0

#define TTL_SERIAL   1
#define TTL_SERIAL_UART uart0
#define TTL_SERIAL_SPEED 115200
