#ifndef ALTIVEC_MATH_H
#define ALTIVEC_MATH_H

#include <cstddef>

// 4x4 matrix multiplication: out = a * b (16 floats each, row-major)
void altivec_matrix4_mul(float *out, const float *a, const float *b);

// Array vector normalization (3 floats per vector)
void altivec_normalize_vec3(float *out_xyz, const float *in_xyz, size_t count);

// Fast vertex normal accumulation and normalization for water simulation
void altivec_compute_vertex_normals(
    float *normals_out,          // resolution * resolution * 3 floats
    const float *facenormals_in,  // resolution * resolution * 2 * 3 floats
    unsigned resolution);

// Fast geoclipmap vertex row generation for water::display
void altivec_generate_geoclipmap_row(
    float *dest,                 // count * 7 floats
    const float *wavedata_row,   // count * 3 floats
    const float *normals_row,    // count * 3 floats
    const float *foam_row,       // count floats
    float coordxoff,
    float coordyoff,
    float viewpos_z,
    float coordxadd,
    unsigned count);

#endif // ALTIVEC_MATH_H
