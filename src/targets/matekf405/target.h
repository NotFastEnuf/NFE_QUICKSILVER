#include "config.h"
#include "config_helper.h"

#define MatekF405

#define F4
#define F405

//PORTS
#define SPI_PORTS   \
  SPI1_PA5PA6PA7    \
  SPI2_PB13PB14PB15 \
  SPI3_PB3PB4PB5

#define USART_PORTS \
  USART1_PA10PA9    \
  USART2_PA3PA2     \
  USART3_PC11PC10   \
  USART4_PA1PA0     \
  USART5_PD2PC12

#define USB_DETECT_PIN PIN_B12

//LEDS
#define LED_NUMBER 2
#define LED1PIN PIN_B9
#define LED2PIN PIN_A14
#define LED2_INVERT
#define BUZZER_PIN PIN_C13
#define FPV_PIN PIN_A13

//GYRO
#define ICM20602_SPI_PORT SPI_PORT1
#define ICM20602_NSS PIN_C2
#define ICM20602_INT PIN_C3
#define USE_DUMMY_I2C
#define SENSOR_ROTATE_90_CCW
#define GYRO_ID_1 0x68
#define GYRO_ID_2 0x12
#define GYRO_ID_3 0x69

//RADIO
#define SOFTSPI_NONE
#ifdef SERIAL_RX
#define RX_USART USART_PORT1
#endif

//OSD
#define ENABLE_OSD
#define MAX7456_SPI_PORT SPI_PORT2
#define MAX7456_NSS PIN_B10

//VOLTAGE DIVIDER
#define BATTERYPIN PIN_C5
#define BATTERY_ADC_CHANNEL LL_ADC_CHANNEL_15

#ifndef VOLTAGE_DIVIDER_R1
#define VOLTAGE_DIVIDER_R1 10000
#endif

#ifndef VOLTAGE_DIVIDER_R2
#define VOLTAGE_DIVIDER_R2 1000
#endif

#ifndef ADC_REF_VOLTAGE
#define ADC_REF_VOLTAGE 3.3
#endif

// MOTOR PINS
//S3_OUT
#define MOTOR_PIN0 MOTOR_PIN_PC8
//S4_OUT
#define MOTOR_PIN1 MOTOR_PIN_PC9
//S1_OUT
#define MOTOR_PIN2 MOTOR_PIN_PC6
//S2_OUT
#define MOTOR_PIN3 MOTOR_PIN_PC7
