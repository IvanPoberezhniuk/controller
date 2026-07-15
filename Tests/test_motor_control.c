/* Host-buildable, HAL-independent test for the pure math in
 * Application/Src/motor_math.c. Not part of the ARM CMake build --
 * compile and run directly on the host:
 *
 *   gcc -std=c11 -Wall -Wextra -IApplication/Inc \
 *       Tests/test_motor_control.c Application/Src/motor_math.c -lm \
 *       -o test_motor_control && ./test_motor_control
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "motor_math.h"

static int float_eq(float a, float b)
{
    return fabsf(a - b) < 1e-4f;
}

static void test_ramp_toward(void)
{
    assert(float_eq(mm_ramp_toward(0.0f, 10.0f, 1.0f), 1.0f));
    assert(float_eq(mm_ramp_toward(0.0f, 10.0f, 100.0f), 10.0f));
    assert(float_eq(mm_ramp_toward(5.0f, 0.0f, 1.0f), 4.0f));
    assert(float_eq(mm_ramp_toward(5.0f, 0.0f, 0.0f), 0.0f));
}

static void test_pid_step(void)
{
    float integral = 0.0f;
    float prev_error = 0.0f;

    float out = mm_pid_step(10.0f, 0.0f, 0.1f, 1.0f, 0.0f, 0.0f, &integral, &prev_error, 100.0f);
    assert(float_eq(out, 10.0f));

    integral = 0.0f;
    prev_error = 0.0f;
    for (int i = 0; i < 100; i++) {
        mm_pid_step(10.0f, 10.0f, 0.1f, 0.0f, 1.0f, 0.0f, &integral, &prev_error, 0.5f);
    }
    assert(integral <= 0.5f);
}

static void test_encoder_delta(void)
{
    assert(mm_encoder_delta(0u, 21u, 16u) == 21);
    assert(mm_encoder_delta(5u, 65530u, 16u) == -11);
    assert(mm_encoder_delta(65530u, 5u, 16u) == 11);

    assert(mm_encoder_delta(0u, 21u, 32u) == 21);
    assert(mm_encoder_delta(5u, 0xFFFFFFF0u, 32u) == -21);
    assert(mm_encoder_delta(0xFFFFFFF0u, 5u, 32u) == 21);
}

static void test_sign(void)
{
    assert(mm_sign(5.0f) == 1);
    assert(mm_sign(-5.0f) == -1);
    assert(mm_sign(0.0f) == 0);
}

int main(void)
{
    test_ramp_toward();
    test_pid_step();
    test_encoder_delta();
    test_sign();
    printf("all motor_math tests passed\n");
    return 0;
}
