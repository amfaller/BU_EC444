/* EC444 Quest01 Skill10 
*  RTOS TASKs – Free RTOS
*  September 15, 2020
*  Author: Tony Faller */

/* Note: This program reuses code from Skill07 and Skill 08 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "sdkconfig.h"

/* Define macro for button push pin */
#define BUTTON_GPIO 25

/* Define macros for LED output pins */
#define LED3_GPIO 27
#define LED2_GPIO 33
#define LED1_GPIO 15
#define LED0_GPIO 32

/* Define macros for alphanumeric display */ 
// 14-Segment Display
#define SLAVE_ADDR                         0x70 // alphanumeric address
#define OSC                                0x21 // oscillator cmd
#define HT16K33_BLINK_DISPLAYON            0x01 // Display on cmd
#define HT16K33_BLINK_OFF                  0    // Blink off cmd
#define HT16K33_BLINK_CMD                  0x80 // Blink cmd
#define HT16K33_CMD_BRIGHTNESS             0xE0 // Brightness cmd

// Master I2C
#define I2C_EXAMPLE_MASTER_SCL_IO          22   // gpio number for i2c clk
#define I2C_EXAMPLE_MASTER_SDA_IO          23   // gpio number for i2c data
#define I2C_EXAMPLE_MASTER_NUM             I2C_NUM_0  // i2c port
#define I2C_EXAMPLE_MASTER_TX_BUF_DISABLE  0    // i2c master no buffer needed
#define I2C_EXAMPLE_MASTER_RX_BUF_DISABLE  0    // i2c master no buffer needed
#define I2C_EXAMPLE_MASTER_FREQ_HZ         100000     // i2c master clock freq
#define WRITE_BIT                          I2C_MASTER_WRITE // i2c master write
#define READ_BIT                           I2C_MASTER_READ  // i2c master read
#define ACK_CHECK_EN                       true // i2c master will check ack
#define ACK_CHECK_DIS                      false// i2c master will not check ack
#define ACK_VAL                            0x00 // i2c ack value
#define NACK_VAL                           0xFF // i2c nack value

////////////////////////////////////////////////////////////////////////////////
// I2C Functions ///////////////////////////////////////////////////////////////

// Function to initiate i2c -- note the MSB declaration!
static void i2c_example_master_init(){
    // Debug
    printf("\n>> i2c Config\n");
    int err;

    // Port configuration
    int i2c_master_port = I2C_EXAMPLE_MASTER_NUM;

    /// Define I2C configurations
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;                              // Master mode
    conf.sda_io_num = I2C_EXAMPLE_MASTER_SDA_IO;              // Default SDA pin
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;                  // Internal pullup
    conf.scl_io_num = I2C_EXAMPLE_MASTER_SCL_IO;              // Default SCL pin
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;                  // Internal pullup
    conf.master.clk_speed = I2C_EXAMPLE_MASTER_FREQ_HZ;       // CLK frequency
    err = i2c_param_config(i2c_master_port, &conf);           // Configure
    if (err == ESP_OK) {printf("- parameters: ok\n");}

    // Install I2C driver
    err = i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_EXAMPLE_MASTER_RX_BUF_DISABLE,
                       I2C_EXAMPLE_MASTER_TX_BUF_DISABLE, 0);
	// i2c_set_data_mode(i2c_master_port,I2C_DATA_MODE_LSB_FIRST,I2C_DATA_MODE_LSB_FIRST);
	if (err == ESP_OK) {printf("- initialized: yes\n\n");}

	// Dat in MSB mode
	i2c_set_data_mode(i2c_master_port, I2C_DATA_MODE_MSB_FIRST, I2C_DATA_MODE_MSB_FIRST);
}

// Utility function to test for I2C device address -- not used in deploy
int testConnection(uint8_t devAddr, int32_t timeout) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int err = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return err;
}

// Utility function to scan for i2c device
static void i2c_scanner() {
    int32_t scanTimeout = 1000;
    printf("\n>> I2C scanning ..."  "\n");
    uint8_t count = 0;
    for (uint8_t i = 1; i < 127; i++) {
        // printf("0x%X%s",i,"\n");
        if (testConnection(i, scanTimeout) == ESP_OK) {
            printf( "- Device found at address: 0x%X%s", i, "\n");
            count++;
        }
    }
    if (count == 0)
        printf("- No I2C devices found!" "\n");
    printf("\n");
}

////////////////////////////////////////////////////////////////////////////////
// Alphanumeric Functions //////////////////////////////////////////////////////

// Turn on oscillator for alpha display
int alpha_oscillator() {
  int ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( SLAVE_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, OSC, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  vTaskDelay(200 / portTICK_RATE_MS);
  return ret;
}

// Set blink rate to off
int no_blink() {
  int ret;
  i2c_cmd_handle_t cmd2 = i2c_cmd_link_create();
  i2c_master_start(cmd2);
  i2c_master_write_byte(cmd2, ( SLAVE_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd2, HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON | (HT16K33_BLINK_OFF << 1), ACK_CHECK_EN);
  i2c_master_stop(cmd2);
  ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd2, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd2);
  vTaskDelay(200 / portTICK_RATE_MS);
  return ret;
}

// Set Brightness
int set_brightness_max(uint8_t val) {
  int ret;
  i2c_cmd_handle_t cmd3 = i2c_cmd_link_create();
  i2c_master_start(cmd3);
  i2c_master_write_byte(cmd3, ( SLAVE_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd3, HT16K33_CMD_BRIGHTNESS | val, ACK_CHECK_EN);
  i2c_master_stop(cmd3);
  ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd3, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd3);
  vTaskDelay(200 / portTICK_RATE_MS);
  return ret;
}


////////////////////////////////////////////////////////////////////////////////
// ASCII Table /////////////////////////////////////////////////////////////////
static const uint16_t alphafonttable[] = {

	0b0000000000000001, 0b0000000000000010, 0b0000000000000100,
	0b0000000000001000, 0b0000000000010000, 0b0000000000100000,
	0b0000000001000000, 0b0000000010000000, 0b0000000100000000,
	0b0000001000000000, 0b0000010000000000, 0b0000100000000000,
	0b0001000000000000, 0b0010000000000000, 0b0100000000000000,
	0b1000000000000000, 0b0000000000000000, 0b0000000000000000,
	0b0000000000000000, 0b0000000000000000, 0b0000000000000000,
	0b0000000000000000, 0b0000000000000000, 0b0000000000000000,
	0b0001001011001001, 0b0001010111000000, 0b0001001011111001,
	0b0000000011100011, 0b0000010100110000, 0b0001001011001000,
	0b0011101000000000, 0b0001011100000000,
	0b0000000000000000, //
	0b0000000000000110, // !
	0b0000001000100000, // "
	0b0001001011001110, // #
	0b0001001011101101, // $
	0b0000110000100100, // %
	0b0010001101011101, // &
	0b0000010000000000, // '
	0b0010010000000000, // (
	0b0000100100000000, // )
	0b0011111111000000, // *
	0b0001001011000000, // +
	0b0000100000000000, // ,
	0b0000000011000000, // -
	0b0000000000000000, // .
	0b0000110000000000, // /
	0b0000110000111111, // 0
	0b0000000000000110, // 1
	0b0000000011011011, // 2
	0b0000000010001111, // 3
	0b0000000011100110, // 4
	0b0010000001101001, // 5
	0b0000000011111101, // 6
	0b0000000000000111, // 7
	0b0000000011111111, // 8
	0b0000000011101111, // 9
	0b0001001000000000, // :
	0b0000101000000000, // ;
	0b0010010000000000, // <
	0b0000000011001000, // =
	0b0000100100000000, // >
	0b0001000010000011, // ?
	0b0000001010111011, // @
	0b0000000011110111, // A
	0b0001001010001111, // B
	0b0000000000111001, // C
	0b0001001000001111, // D
	0b0000000011111001, // E
	0b0000000001110001, // F
	0b0000000010111101, // G
	0b0000000011110110, // H
	0b0001001000000000, // I
	0b0000000000011110, // J
	0b0010010001110000, // K
	0b0000000000111000, // L
	0b0000010100110110, // M
	0b0010000100110110, // N
	0b0000000000111111, // O
	0b0000000011110011, // P
	0b0010000000111111, // Q
	0b0010000011110011, // R
	0b0000000011101101, // S
	0b0001001000000001, // T
	0b0000000000111110, // U
	0b0000110000110000, // V
	0b0010100000110110, // W
	0b0010110100000000, // X
	0b0001010100000000, // Y
	0b0000110000001001, // Z
	0b0000000000111001, // [
	0b0010000100000000, //
	0b0000000000001111, // ]
	0b0000110000000011, // ^
	0b0000000000001000, // _
	0b0000000100000000, // `
	0b0001000001011000, // a
	0b0010000001111000, // b
	0b0000000011011000, // c
	0b0000100010001110, // d
	0b0000100001011000, // e
	0b0000000001110001, // f
	0b0000010010001110, // g
	0b0001000001110000, // h
	0b0001000000000000, // i
	0b0000000000001110, // j
	0b0011011000000000, // k
	0b0000000000110000, // l
	0b0001000011010100, // m
	0b0001000001010000, // n
	0b0000000011011100, // o
	0b0000000101110000, // p
	0b0000010010000110, // q
	0b0000000001010000, // r
	0b0010000010001000, // s
	0b0000000001111000, // t
	0b0000000000011100, // u
	0b0010000000000100, // v
	0b0010100000010100, // w
	0b0010100011000000, // x
	0b0010000000001100, // y
	0b0000100001001000, // z
	0b0000100101001001, // {
	0b0001001000000000, // |
	0b0010010010001001, // }
	0b0000010100100000, // ~
	0b0011111111111111,
};

////////////////////////////////////////////////////////////////////////////////
// LED Function ////////////////////////////////////////////////////////////////
int * toBin(int num){
	static int b[4];	// Array which holds 4-bit binary equivalent

	for(int i=0; i<4; i++){
		b[i] = num % 2;
		num = num / 2;
	}

	return b;
}

////////////////////////////////////////////////////////////////////////////////
// Initialization and Tasks ////////////////////////////////////////////////////

int direction = 0;		// 1 means up, 0 means down

void init(){
    i2c_example_master_init();
    i2c_scanner();
}

// Button push detection	NOTE: Button is built with some chatter, requires a debouncer or some sort of delay to counteract this
static void Button_Push(){
	/* Configure for GPIO input */
	gpio_pad_select_gpio(BUTTON_GPIO);
	gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);

	while(1){
		/* Read input level, set direction */
		if(gpio_get_level(BUTTON_GPIO))
			direction = ~direction;

		/* "Debounce" waiting period of 20ms*/
		vTaskDelay(50 / portTICK_PERIOD_MS);
	}
}

// LED Count
static void LED_Count(){
	int LED3_state=0, LED2_state=0, LED1_state=0, LED0_state=0;   // LEDs initially off
	int num=0;  // Integer version of counter
	int* b;		// Pointer to binary equivalent of num

	/* Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    gpio_pad_select_gpio(LED3_GPIO);
    gpio_pad_select_gpio(LED2_GPIO);
    gpio_pad_select_gpio(LED1_GPIO);
    gpio_pad_select_gpio(LED0_GPIO);

    /* Set the GPIO as a push/pull output */
    gpio_set_direction(LED3_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED2_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED1_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED0_GPIO, GPIO_MODE_OUTPUT);

    /* Counting loop */
    while(1) {

		gpio_set_level(LED3_GPIO, LED3_state);
		gpio_set_level(LED2_GPIO, LED2_state);
		gpio_set_level(LED1_GPIO, LED1_state);
		gpio_set_level(LED0_GPIO, LED0_state);

		vTaskDelay(1000 / portTICK_PERIOD_MS);

		b = toBin(num);

		LED0_state = *b;
		LED1_state = *(b+1);
		LED2_state = *(b+2);
		LED3_state = *(b+3);

		// Count up
		if(direction){
			if(num == 15) num = 0;
			else num += 1;
		}

		// Count down
		else if(~direction){
			if(num == 0) num = 15;
			else num -= 1;
		}

	}
}

// Alnum Up/Down Display
static void Alnum_Disp(){
	int ret;

    char dirDisp[5] = "  UP";		// Initial counting direction is up

    while(1){
   		/* Set up routines */
		// Turn on alpha oscillator
		ret = alpha_oscillator();
		if(ret == ESP_OK) {printf("- oscillator: ok \n");}
		// Set display blink off
		ret = no_blink();
		if(ret == ESP_OK) {printf("- blink: off \n");}
		ret = set_brightness_max(0xF);
		if(ret == ESP_OK) {printf("- brightness: max \n");}

		// Read direction, write to dirDisp
		if(direction)
			strcpy(dirDisp, "  UP");
		else if(~direction)
			strcpy(dirDisp, "DOWN");
		else
			strcpy(dirDisp, "ERR ");

		// Write characters to displaybuffer
      	uint16_t displaybuffer[8];
      	displaybuffer[0] = alphafonttable[ (int)dirDisp[0] ];
      	displaybuffer[1] = alphafonttable[ (int)dirDisp[1] ];
      	displaybuffer[2] = alphafonttable[ (int)dirDisp[2] ];
      	displaybuffer[3] = alphafonttable[ (int)dirDisp[3] ];

      	// Send commands characters to display over I2C
		i2c_cmd_handle_t cmd4 = i2c_cmd_link_create();
		i2c_master_start(cmd4);
		i2c_master_write_byte(cmd4, ( SLAVE_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
		i2c_master_write_byte(cmd4, (uint8_t)0x00, ACK_CHECK_EN);
		for (uint8_t i=0; i<8; i++) {
			i2c_master_write_byte(cmd4, displaybuffer[i] & 0xFF, ACK_CHECK_EN);
			i2c_master_write_byte(cmd4, displaybuffer[i] >> 8, ACK_CHECK_EN);
		}
		i2c_master_stop(cmd4);
		ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd4, 1000 / portTICK_RATE_MS);
		i2c_cmd_link_delete(cmd4);

		printf("- display: %c%c%c%c\n\n", dirDisp[0], dirDisp[1], dirDisp[2], dirDisp[3]);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Main ////////////////////////////////////////////////////////////////////////

void app_main(){
	init();
	xTaskCreate(Button_Push, "Button_Push",1024*2, NULL, configMAX_PRIORITIES, NULL);
	xTaskCreate(LED_Count, "LED_Count",1024*2, NULL, configMAX_PRIORITIES-1, NULL);
	xTaskCreate(Alnum_Disp, "Alnum_Disp",1024*2, NULL, configMAX_PRIORITIES-2, NULL);
}