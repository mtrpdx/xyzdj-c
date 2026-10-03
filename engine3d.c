#include "engine3d.h"
#include <math.h>

#define PI 3.14159265f
#define DEG_TO_RAD(x) ((x) * PI / 180.0f)

void rotate_3d(vec3_t *point, float angle_x, float angle_y, float angle_z) {
    float rad_x = DEG_TO_RAD(angle_x);
    float rad_y = DEG_TO_RAD(angle_y);
    float rad_z = DEG_TO_RAD(angle_z);

    // Using sinf() and cosf() forces 32-bit hardware FPU math
    float cos_x = cosf(rad_x), sin_x = sinf(rad_x);
    float cos_y = cosf(rad_y), sin_y = sinf(rad_y);
    float cos_z = cosf(rad_z), sin_z = sinf(rad_z);

    // Rotate around x-axis
    float y1 = point->y * cos_x - point->z * sin_x;
    float z1 = point->y * sin_x + point->z * cos_x;
    point->y = y1;
    point->z = z1;

    // Rotate around x-axis
    float x2 = point->x * cos_y + point->z * sin_y;
    float z2 = -point->x * sin_y + point->z * cos_y;
    point->x = x2;
    point->z = z2;

    // Rotate around z-axis
    float x3 = point->x * cos_z - point->y * sin_z;
    float y3 = point->x * sin_z + point->y * cos_z;
    point->x = x3;
    point->y = y3;
}

vec2_t project_3d_to_2d(vec3_t point, int fov, int camera_distance) {
    vec2_t projected;

    // Prevent divide-by-zero if the point clips directly into the camera
    float z_depth = point.z + camera_distance;
    if (z_depth <= 0) z_depth = 0.1f;

    // Apply Perspective Math
    // We multiply by FOV to scale the cube, and divide by Z to create perspective
    projected.x = (int)((point.x * fov) / z_depth) + (128 / 2); // Center X on a 128px screen
    projected.y = (int)((point.y * fov) / z_depth) + (64 / 2);  // Center Y on a 64px screen

    return projected;
}

void translate_3d(vec3_t *point, float offset_x, float offset_y, float offset_z) {
    point->x += offset_x;
    point->y += offset_y;
    point->z += offset_z;
}
