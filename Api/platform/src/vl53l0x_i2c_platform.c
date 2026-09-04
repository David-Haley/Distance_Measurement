/*
 * Raspberry Pi Pico platform I2C layer for the ST VL53L0X API.
 *
 * Replaces the original Windows/ranging_sensor_comms.dll backed
 * implementation with the Pico SDK hardware_i2c driver, using I2C0 on
 * GPIO0 (SDA) / GPIO1 (SCL) - physical header pins 1 and 2.
 */

#include <string.h>

#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#include "vl53l0x_i2c_platform.h"
#include "vl53l0x_def.h"

#define STATUS_OK    0x00
#define STATUS_FAIL  0x01

#define VL53L0X_I2C_INSTANCE  i2c0
#define VL53L0X_I2C_SDA_PIN   0
#define VL53L0X_I2C_SCL_PIN   1

int32_t VL53L0X_comms_initialise(uint8_t comms_type, uint16_t comms_speed_khz)
{
	if (comms_type != I2C)
		return STATUS_FAIL;

	i2c_init(VL53L0X_I2C_INSTANCE, (uint)comms_speed_khz * 1000);

	gpio_set_function(VL53L0X_I2C_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(VL53L0X_I2C_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(VL53L0X_I2C_SDA_PIN);
	gpio_pull_up(VL53L0X_I2C_SCL_PIN);

	return STATUS_OK;
}

int32_t VL53L0X_comms_close(void)
{
	i2c_deinit(VL53L0X_I2C_INSTANCE);
	return STATUS_OK;
}

int32_t VL53L0X_write_multi(uint8_t address, uint8_t index, uint8_t *pdata, int32_t count)
{
	uint8_t buffer[COMMS_BUFFER_SIZE + 1];

	if (count > COMMS_BUFFER_SIZE)
		return STATUS_FAIL;

	buffer[0] = index;
	memcpy(&buffer[1], pdata, (size_t)count);

	int written = i2c_write_blocking(VL53L0X_I2C_INSTANCE, address, buffer, (size_t)count + 1, false);

	return (written == count + 1) ? STATUS_OK : STATUS_FAIL;
}

int32_t VL53L0X_read_multi(uint8_t address, uint8_t index, uint8_t *pdata, int32_t count)
{
	int written = i2c_write_blocking(VL53L0X_I2C_INSTANCE, address, &index, 1, true);
	if (written != 1)
		return STATUS_FAIL;

	int read = i2c_read_blocking(VL53L0X_I2C_INSTANCE, address, pdata, (size_t)count, false);

	return (read == count) ? STATUS_OK : STATUS_FAIL;
}

int32_t VL53L0X_write_byte(uint8_t address, uint8_t index, uint8_t data)
{
	return VL53L0X_write_multi(address, index, &data, 1);
}

int32_t VL53L0X_write_word(uint8_t address, uint8_t index, uint16_t data)
{
	uint8_t buffer[BYTES_PER_WORD];

	buffer[0] = (uint8_t)(data >> 8);
	buffer[1] = (uint8_t)(data & 0x00FF);

	return VL53L0X_write_multi(address, index, buffer, BYTES_PER_WORD);
}

int32_t VL53L0X_write_dword(uint8_t address, uint8_t index, uint32_t data)
{
	uint8_t buffer[BYTES_PER_DWORD];

	buffer[0] = (uint8_t)(data >> 24);
	buffer[1] = (uint8_t)((data & 0x00FF0000) >> 16);
	buffer[2] = (uint8_t)((data & 0x0000FF00) >> 8);
	buffer[3] = (uint8_t)(data & 0x000000FF);

	return VL53L0X_write_multi(address, index, buffer, BYTES_PER_DWORD);
}

int32_t VL53L0X_read_byte(uint8_t address, uint8_t index, uint8_t *pdata)
{
	return VL53L0X_read_multi(address, index, pdata, 1);
}

int32_t VL53L0X_read_word(uint8_t address, uint8_t index, uint16_t *pdata)
{
	uint8_t buffer[BYTES_PER_WORD];
	int32_t status = VL53L0X_read_multi(address, index, buffer, BYTES_PER_WORD);

	*pdata = ((uint16_t)buffer[0] << 8) + (uint16_t)buffer[1];

	return status;
}

int32_t VL53L0X_read_dword(uint8_t address, uint8_t index, uint32_t *pdata)
{
	uint8_t buffer[BYTES_PER_DWORD];
	int32_t status = VL53L0X_read_multi(address, index, buffer, BYTES_PER_DWORD);

	*pdata = ((uint32_t)buffer[0] << 24) + ((uint32_t)buffer[1] << 16) +
		 ((uint32_t)buffer[2] << 8) + (uint32_t)buffer[3];

	return status;
}

int32_t VL53L0X_platform_wait_us(int32_t wait_us)
{
	sleep_us((uint64_t)wait_us);
	return STATUS_OK;
}

int32_t VL53L0X_wait_ms(int32_t wait_ms)
{
	sleep_ms((uint32_t)wait_ms);
	return STATUS_OK;
}

int32_t VL53L0X_set_gpio(uint8_t level)
{
	(void)level;
	return STATUS_OK;
}

int32_t VL53L0X_get_gpio(uint8_t *plevel)
{
	*plevel = 0;
	return STATUS_OK;
}

int32_t VL53L0X_release_gpio(void)
{
	return STATUS_OK;
}

int32_t VL53L0X_cycle_power(void)
{
	return STATUS_OK;
}

int32_t VL53L0X_get_timer_frequency(int32_t *ptimer_freq_hz)
{
	*ptimer_freq_hz = 1000000;
	return STATUS_OK;
}

int32_t VL53L0X_get_timer_value(int32_t *ptimer_count)
{
	*ptimer_count = (int32_t)time_us_64();
	return STATUS_OK;
}
