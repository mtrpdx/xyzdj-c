#ifndef ENGINE3D_H_
#define ENGINE3D_H_

#include <stdint.h>

// Point in 3D
typedef struct {
    float x, y, z;
} vec3_t;

// Projected point in 2D
typedef struct {
    int x, y;
} vec2_t;

// Edge connects two vertices
typedef struct {
    int v1, v2;
} edge_t;

typedef struct {
    float pos_x, pos_y, pos_z;       // Where the cube lives
    float rot_x, rot_y, rot_z;       // Its current rotation
    float spin_x, spin_y, spin_z;    // How fast it spins per frame
} instance_t;

void rotate_3d(vec3_t *point, float angle_x, float angle_y, float angle_z);
vec2_t project_3d_to_2d(vec3_t point, int fov, int camera_distance);
void translate_3d(vec3_t *point, float offset_x, float offset_y, float offset_z);

#endif // ENGINE3D_H_
