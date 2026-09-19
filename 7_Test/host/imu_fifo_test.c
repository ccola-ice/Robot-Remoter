#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
typedef uint8_t u8;
#define q30 1073741824.0f
#define INV_WXYZ_QUAT 0x100
static int reads, queued, read_error, resets, have_quat = 1;
static int dmp_read_fifo(short *gyro, short *accel, long *quat,
                         unsigned long *stamp, short *sensors, unsigned char *more)
{
    (void)gyro; (void)accel; (void)stamp;
    ++reads;
    if(reads == read_error) return 1;
    quat[0] = 1073741824; quat[1] = quat[2] = quat[3] = 0;
    *sensors = have_quat ? INV_WXYZ_QUAT : 0;
    *more = --queued > 0;
    return 0;
}
static void mpu_reset_fifo(void) { ++resets; }
#include "imu_fifo.inc"
int main(void)
{
    float pitch = 91, roll = 92, yaw = 93;
    queued = 3;
    assert(mpu_dmp_get_data(&pitch, &roll, &yaw) == 0 && reads == 3);
    assert(pitch == 0 && roll == 0 && yaw == 0);
    reads = 0; queued = 8;
    assert(mpu_dmp_get_data(&pitch, &roll, &yaw) == 0 && reads == 8);
    reads = 0; queued = 9; pitch = 91; roll = 92; yaw = 93;
    assert(mpu_dmp_get_data(&pitch, &roll, &yaw) != 0);
    assert(reads == 8 && resets == 1 && pitch == 91 && roll == 92 && yaw == 93);
    reads = 0; queued = 3; read_error = 2;
    assert(mpu_dmp_get_data(&pitch, &roll, &yaw) != 0);
    assert(reads == 2 && pitch == 91 && roll == 92 && yaw == 93);
    reads = 0; queued = 1; read_error = 0; have_quat = 0;
    assert(mpu_dmp_get_data(&pitch, &roll, &yaw) != 0 && yaw == 93);
    puts("IMU FIFO tests passed: drain latest, 8-packet bound, overload reset, error/no-quaternion never publishes.");
    return 0;
}
