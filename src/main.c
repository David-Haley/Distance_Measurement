/*
 * Reports distance in mm from a VL53L0X time-of-flight sensor connected
 * to a Raspberry Pi Pico W over I2C0 (GPIO0/GPIO1, header pins 1 and 2).
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "vl53l0x_api.h"
#include "vl53l0x_i2c_platform.h"

#define VL53L0X_I2C_ADDRESS   0x29
#define VL53L0X_I2C_SPEED_KHZ 400

static void die_on_error(const char *step, VL53L0X_Error status)
{
	if (status != VL53L0X_ERROR_NONE) {
		printf("%s failed: %d\n", step, (int)status);
		while (true)
			tight_loop_contents();
	}
}

int main(void)
{
	stdio_init_all();
	sleep_ms(2000); /* let USB CDC enumerate and the sensor finish booting */

	VL53L0X_Dev_t device;
	memset(&device, 0, sizeof(device));
	VL53L0X_DEV Dev = &device;

	Dev->I2cDevAddr = VL53L0X_I2C_ADDRESS;
	Dev->comms_type = I2C;
	Dev->comms_speed_khz = VL53L0X_I2C_SPEED_KHZ;

	die_on_error("comms init", VL53L0X_comms_initialise(I2C, VL53L0X_I2C_SPEED_KHZ));

	printf("Initialising VL53L0X...\n");

	die_on_error("WaitDeviceBooted", VL53L0X_WaitDeviceBooted(Dev));
	die_on_error("DataInit", VL53L0X_DataInit(Dev));
	die_on_error("StaticInit", VL53L0X_StaticInit(Dev));

	uint8_t vhv_settings, phase_cal;
	die_on_error("PerformRefCalibration",
		     VL53L0X_PerformRefCalibration(Dev, &vhv_settings, &phase_cal));

	uint32_t ref_spad_count;
	uint8_t is_aperture_spads;
	die_on_error("PerformRefSpadManagement",
		     VL53L0X_PerformRefSpadManagement(Dev, &ref_spad_count, &is_aperture_spads));

	die_on_error("SetDeviceMode",
		     VL53L0X_SetDeviceMode(Dev, VL53L0X_DEVICEMODE_SINGLE_RANGING));

	printf("VL53L0X ready, reporting distance...\n");

	while (true) {
		VL53L0X_RangingMeasurementData_t measurement;
		VL53L0X_Error status = VL53L0X_PerformSingleRangingMeasurement(Dev, &measurement);

		if (status == VL53L0X_ERROR_NONE) {
			if (measurement.RangeStatus == 0)
				printf("Distance: %u mm\n", measurement.RangeMilliMeter);
			else
				printf("Out of range (status %u)\n", measurement.RangeStatus);
		} else {
			printf("Measurement error: %d\n", (int)status);
		}

		sleep_ms(200);
	}
}
