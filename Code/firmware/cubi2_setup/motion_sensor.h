// Cubi 2 - the IMU (6-axis, on the I2C cable to board Y) in the cube frame.
// The SAME file is in cubi2_setup and cubi2_brain.
//
// The cube frame: x, y, z along the three wheel axes (wheel X on face x ...),
// origin corner 0 = the corner between the three wheels.
// The IMU sits on the diagonal; its own frame is turned (see settings.h).
#pragma once
#include <Wire.h>
#include "SparkFun_BMI270_Arduino_Library.h"   // the IMU library - platformio.ini, lib_deps
#include "settings.h"

// Teensy 4.1: Wire2 = SDA2 pin 25, SCL2 pin 24 (board Y poles y4 SDA, y5 SCL)
#define IMU_WIRE Wire2

struct ImuCube {
  BMI270 imu;
  uint8_t address = 0;
  float accImu[3] = {0, 0, 0};     // g, imu frame
  float gyrImu[3] = {0, 0, 0};     // rad/s, imu frame (bias removed)
  float acc[3] = {0, 0, 0};        // g, cube frame
  float gyr[3] = {0, 0, 0};        // rad/s, cube frame
  const CubiSettings *s = nullptr;

  bool begin(const CubiSettings *set) {
    s = set;
    IMU_WIRE.begin();
    IMU_WIRE.setClock(400000);
    const uint8_t addr[2] = {BMI2_I2C_PRIM_ADDR, BMI2_I2C_SEC_ADDR};   // 0x68, 0x69
    for (int k = 0; k < 2; k++) {
      if (imu.beginI2C(addr[k], IMU_WIRE) == BMI2_OK) { address = addr[k]; break; }
    }
    if (!address) return false;
    // 400 Hz, low delay: accelerometer +-4 g, gyroscope +-1000 deg/s
    bmi2_sens_config a;
    a.type = BMI2_ACCEL;
    a.cfg.acc.odr = BMI2_ACC_ODR_400HZ;
    a.cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;
    a.cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;
    a.cfg.acc.range = BMI2_ACC_RANGE_4G;
    bmi2_sens_config g;
    g.type = BMI2_GYRO;
    g.cfg.gyr.odr = BMI2_GYR_ODR_400HZ;
    g.cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;
    g.cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;
    g.cfg.gyr.ois_range = BMI2_GYR_OIS_2000;
    g.cfg.gyr.range = BMI2_GYR_RANGE_1000;
    g.cfg.gyr.noise_perf = BMI2_PERF_OPT_MODE;
    if (imu.setConfig(a) != BMI2_OK) return false;
    if (imu.setConfig(g) != BMI2_OK) return false;
    return true;
  }

  static void rot(const float *R, const float *v, float *o) {
    for (int r = 0; r < 3; r++) o[r] = R[3 * r] * v[0] + R[3 * r + 1] * v[1] + R[3 * r + 2] * v[2];
  }

  // read once; false if the bus failed
  bool read() {
    if (imu.getSensorData() != BMI2_OK) return false;
    const float D2R = 0.017453293f;
    accImu[0] = imu.data.accelX; accImu[1] = imu.data.accelY; accImu[2] = imu.data.accelZ;
    gyrImu[0] = imu.data.gyroX * D2R - s->gyroBias[0];
    gyrImu[1] = imu.data.gyroY * D2R - s->gyroBias[1];
    gyrImu[2] = imu.data.gyroZ * D2R - s->gyroBias[2];
    rot(s->R, accImu, acc);
    rot(s->R, gyrImu, gyr);
    return true;
  }
};
