#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "robot_telemetry_protocol.h"
#include "robot_control_protocol.h"

/* Independently assembled with Python struct.pack and binascii.crc_hqx. */
static const uint8_t vectors[4][32] = {
    {0x52,0x54,1,1,0x12,0x34,0xab,0xcd,1,2,3,4,0x1f,1,
     0x2e,0xe0,75,3,0,0,0,20,0,2,0,3,0,0,0,0,0x54,0x4f},
    {0x52,0x54,1,2,0xff,0xff,0,0,0,0,0,7,7,0,
     0x80,0,0xf8,0xf8,7,8,0,0,0x80,0,0x7f,0xff,0xff,0xff,0,0,0xea,0x5f},
    {0x52,0x54,1,3,0xff,0xff,0,0,0,0,0,7,1,0,
     0x80,0,0,0,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,1,0,0,0,0x63,0xf6},
    {0x52,0x54,1,4,0xff,0xff,0,0,0,0,0,7,7,0,
     0xca,0x5b,0x17,0,0x6b,0x49,0xd2,0,0xff,0xfe,0x1d,0xc0,3,17,0,0,0x0a,0x55}
};

static void rejected(const uint8_t *bytes, size_t length)
{
    RobotTelemetryFrame output, before;
    memset(&output, 0xa5, sizeof(output));
    memcpy(&before, &output, sizeof(before));
    assert(!robot_telemetry_decode(bytes, length, &output));
    assert(memcmp(&output, &before, sizeof(output)) == 0);
}

static void encode_rejected(const RobotTelemetryFrame *frame)
{
    uint8_t output[32], before[32];
    memset(output, 0xa5, sizeof(output));
    memcpy(before, output, sizeof(before));
    assert(!robot_telemetry_encode(output, sizeof(output), frame));
    assert(memcmp(output, before, sizeof(output)) == 0);
}

static void repair_crc(uint8_t *bytes)
{
    robot_telemetry_write16(bytes + 30, robot_telemetry_crc(bytes, 30));
}

static void bad_byte(unsigned type_index, unsigned offset, uint8_t value)
{
    uint8_t bytes[32];
    memcpy(bytes, vectors[type_index], sizeof(bytes));
    bytes[offset] = value;
    repair_crc(bytes);
    rejected(bytes, sizeof(bytes));
}

static void golden_and_corruption(void)
{
    unsigned type, offset, bit;
    uint8_t bytes[32], encoded[32];
    RobotTelemetryFrame out;
    assert(robot_telemetry_crc((const uint8_t *)"123456789", 9) == 0x29b1U);
    for(type = 0; type < 4; ++type) {
        assert(robot_telemetry_decode(vectors[type], 32, &out));
        assert(out.type == type + 1);
        assert(robot_telemetry_encode(encoded, 32, &out));
        assert(memcmp(encoded, vectors[type], 32) == 0);
        for(offset = 0; offset < 32; ++offset)
            for(bit = 0; bit < 8; ++bit) {
                memcpy(bytes, vectors[type], 32);
                bytes[offset] ^= (uint8_t)(1U << bit);
                rejected(bytes, 32);
            }
    }
    assert(robot_telemetry_decode(vectors[0], 32, &out));
    assert(out.sequence == 0x1234U && out.accepted_control_sequence == 0xabcdU);
    assert(out.session == 0x01020304UL && out.flags == 1);
    assert(out.data.status.voltage_mv == 12000 && out.data.status.battery_percent == 75);
    assert(out.data.status.control_age_ms == 20 && out.data.status.missing_commands == 3);
    assert(robot_telemetry_decode(vectors[1], 32, &out));
    assert(out.data.motion.speed_mm_s == INT16_MIN);
    assert(out.data.motion.roll_ddeg == -1800 && out.data.motion.pitch_ddeg == 1800);
    assert(out.data.motion.acceleration_x_mm_s2 == INT16_MIN);
    assert(out.data.motion.acceleration_y_mm_s2 == INT16_MAX);
    assert(out.data.motion.acceleration_z_mm_s2 == -1);
    assert(robot_telemetry_decode(vectors[2], 32, &out));
    assert(out.data.position.x_mm == INT32_MIN && out.data.position.y_mm == INT32_MAX);
    assert(out.data.position.z_mm == -1 && out.data.position.frame == 1);
    assert(robot_telemetry_decode(vectors[3], 32, &out));
    assert(out.data.gnss.latitude_e7 == -900000000L);
    assert(out.data.gnss.longitude_e7 == 1800000000L);
    assert(out.data.gnss.altitude_mm == -123456 && out.data.gnss.fix == 3);
}

static void boundaries_and_invalid_fields(void)
{
    RobotTelemetryFrame f, out;
    uint8_t bytes[33], before[33], tiny = 0;
    unsigned type, length, bit;
    assert(robot_telemetry_decode(vectors[0], 32, &f));
    memset(bytes, 0x5a, sizeof(bytes)); memcpy(before, bytes, sizeof(bytes));
    for(length = 0; length < 34; ++length) {
        if(length == 32) continue;
        /* A one-byte source proves length is checked before header access. */
        rejected(&tiny, length);
        assert(!robot_telemetry_encode(bytes, length, &f));
        assert(memcmp(bytes, before, sizeof(bytes)) == 0);
    }
    rejected(NULL, 32);
    assert(!robot_telemetry_decode(vectors[0], 32, NULL));
    assert(!robot_telemetry_encode(NULL, 32, &f));
    encode_rejected(NULL);
    for(type = 0; type < 4; ++type) {
        bad_byte(type, 0, 'C'); bad_byte(type, 1, 'C');
        bad_byte(type, 2, 0); bad_byte(type, 2, 2);
        bad_byte(type, 3, 0); bad_byte(type, 3, 5); bad_byte(type, 3, 255);
        for(bit = 1; bit < 8; ++bit) bad_byte(type, 13, (uint8_t)(1U << bit));
        bad_byte(type, 12, 0x80);
        bad_byte(type, 28, 1); bad_byte(type, 29, 1);
        memcpy(bytes, vectors[type], 32); memset(bytes + 8, 0, 4); repair_crc(bytes);
        rejected(bytes, 32);
    }
    bad_byte(0, 26, 1); bad_byte(0, 27, 1); bad_byte(2, 27, 1);
    bad_byte(0, 16, 101); bad_byte(0, 17, 5); bad_byte(0, 18, 1);
    bad_byte(0, 19, 0x40); bad_byte(0, 12, 0); bad_byte(0, 13, 0);
    bad_byte(1, 16, 0x08); /* +2296 ddeg: CRC is valid, semantic range is not. */
    bad_byte(1, 12, 0); bad_byte(2, 26, 0); bad_byte(2, 26, 2); bad_byte(2, 12, 0);
    bad_byte(3, 26, 1); bad_byte(3, 26, 2); /* Altitude needs 3D fix. */
    bad_byte(3, 12, 6); bad_byte(3, 12, 1);
    bad_byte(3, 14, 0xc9); bad_byte(3, 18, 0x6c); /* Outside Earth coordinate range. */
    for(type = 1; type <= 4; ++type) {
        memset(&f, 0, sizeof(f)); f.type = (uint8_t)type; f.session = 1;
        assert(robot_telemetry_encode(bytes, 32, &f)); /* Explicitly unknown data. */
        assert(robot_telemetry_decode(bytes, 32, &out) && out.valid == 0);
        f.session = 0; encode_rejected(&f); f.session = 1;
        f.accepted_control_sequence = 1; encode_rejected(&f);
    }
    memset(&f, 0, sizeof(f)); f.type = 1; f.session = 1;
    f.data.status.voltage_mv = 1; encode_rejected(&f); f.data.status.voltage_mv = 0;
    f.valid = ROBOT_TELEMETRY_CONTROL_AGE_VALID; encode_rejected(&f);
    f.valid = ROBOT_TELEMETRY_STATE_VALID; f.data.status.state = ROBOT_TELEMETRY_ARMED;
    encode_rejected(&f); f.flags = ROBOT_TELEMETRY_CONTROL_VALID;
    assert(robot_telemetry_encode(bytes, 32, &f)); /* Accepted sequence zero is valid. */
    f.type = 4; f.flags = 0; f.valid = ROBOT_TELEMETRY_FIX_VALID;
    memset(&f.data, 0, sizeof(f.data)); f.data.gnss.satellites = 8;
    assert(robot_telemetry_encode(bytes, 32, &f)); /* Satellites, no fix, no coordinates. */
    f.valid |= ROBOT_TELEMETRY_COORD_VALID; encode_rejected(&f);
    f.data.gnss.fix = 2; assert(robot_telemetry_encode(bytes, 32, &f));
    f.valid |= ROBOT_TELEMETRY_ALTITUDE_VALID; encode_rejected(&f);
}

static void serials_freshness_and_rc_compatibility(void)
{
    uint8_t bytes[32], old[32];
    RobotControlCommand command, decoded;
    RobotTelemetryFrame telemetry;
    assert(robot_telemetry_sequence_newer(0, 65535));
    assert(robot_telemetry_sequence_newer(32767, 0));
    assert(!robot_telemetry_sequence_newer(32768, 0));
    assert(!robot_telemetry_sequence_newer(65535, 0));
    assert(!robot_telemetry_sequence_newer(7, 7));
    assert(robot_telemetry_session_newer(1, UINT32_MAX));
    assert(!robot_telemetry_session_newer(0, UINT32_MAX));
    assert(!robot_telemetry_session_newer(0x80000001UL, 1));
    assert(!robot_telemetry_session_newer(8, 9));
    assert(!robot_telemetry_session_newer(9, 9));
    assert(robot_telemetry_is_fresh(299, 0, ROBOT_TELEMETRY_MOTION_TIMEOUT_MS));
    assert(!robot_telemetry_is_fresh(300, 0, ROBOT_TELEMETRY_MOTION_TIMEOUT_MS));
    assert(robot_telemetry_is_fresh(8, UINT32_MAX - 10U, 20));
    assert(!robot_telemetry_is_fresh(9, UINT32_MAX - 10U, 20));
    assert(!robot_telemetry_is_fresh(0, 0, 0));
    assert(!robot_telemetry_is_fresh(0, 0, 0x80000000UL));
    memset(&command, 0, sizeof(command));
    command.sequence = 123; command.armed = 1; command.digital = 1;
    command.x = -1000; command.y = 1000; command.heading = -1800; command.limit = 1000;
    robot_packet_encode(bytes, &command); memcpy(old, bytes, 32);
    assert(robot_packet_decode(bytes, &decoded));
    rejected(bytes, 32); /* RC cannot be mistaken for RT. */
    assert(robot_telemetry_decode(vectors[0], 32, &telemetry));
    assert(robot_telemetry_encode(bytes, 32, &telemetry));
    assert(!robot_packet_decode(bytes, &decoded)); /* RT cannot command a v1 receiver. */
    robot_packet_encode(bytes, &command);
    assert(memcmp(bytes, old, 32) == 0 && robot_packet_decode(bytes, &decoded));
}

int main(void)
{
    golden_and_corruption();
    boundaries_and_invalid_fields();
    serials_freshness_and_rc_compatibility();
    puts("RT v1: 4 golden vectors, 1024 single-bit corruptions, strict length/semantic checks,");
    puts("unchanged output on failure, signed extrema, sequence/session/tick wrap and RC isolation passed.");
    return 0;
}
