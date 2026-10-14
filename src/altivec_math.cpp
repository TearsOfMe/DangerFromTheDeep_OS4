#include "altivec_math.h"
#include <altivec.h>
#undef vector
#undef pixel
#undef bool

#include <cstdint>
#include <cstring>

static inline __vector float load_vec4(const float *p) {
    if (((uintptr_t)p & 15) == 0) {
        return vec_ld(0, p);
    }
    __vector unsigned char perm = vec_lvsl(0, p);
    __vector float v0 = vec_ld(0, p);
    __vector float v1 = vec_ld(15, p);
    return vec_perm(v0, v1, perm);
}

static inline void store_vec4(__vector float v, float *p) {
    if (((uintptr_t)p & 15) == 0) {
        vec_st(v, 0, p);
    } else {
        float tmp[4] __attribute__((aligned(16)));
        vec_st(v, 0, tmp);
        p[0] = tmp[0];
        p[1] = tmp[1];
        p[2] = tmp[2];
        p[3] = tmp[3];
    }
}

static inline __vector float altivec_rsqrt_vec(__vector float v) {
    __vector float est = vec_rsqrte(v);
    __vector float half = (__vector float){0.5f, 0.5f, 0.5f, 0.5f};
    __vector float one_point_five = (__vector float){1.5f, 1.5f, 1.5f, 1.5f};
    __vector float zero = (__vector float){0.0f, 0.0f, 0.0f, 0.0f};

    __vector float est_sq = vec_madd(est, est, zero);
    __vector float v_est_sq = vec_madd(v, est_sq, zero);
    __vector float nr = vec_nmsub(half, one_point_five, v_est_sq);
    return vec_madd(est, nr, zero);
}

void altivec_matrix4_mul(float *out, const float *a, const float *b) {
    __vector float b0 = load_vec4(b);
    __vector float b1 = load_vec4(b + 4);
    __vector float b2 = load_vec4(b + 8);
    __vector float b3 = load_vec4(b + 12);
    __vector float zero = (__vector float){0.0f, 0.0f, 0.0f, 0.0f};

    for (int i = 0; i < 4; ++i) {
        float a0 = a[i * 4 + 0];
        float a1 = a[i * 4 + 1];
        float a2 = a[i * 4 + 2];
        float a3 = a[i * 4 + 3];

        __vector float va0 = (__vector float){a0, a0, a0, a0};
        __vector float va1 = (__vector float){a1, a1, a1, a1};
        __vector float va2 = (__vector float){a2, a2, a2, a2};
        __vector float va3 = (__vector float){a3, a3, a3, a3};

        __vector float r = vec_madd(va0, b0, zero);
        r = vec_madd(va1, b1, r);
        r = vec_madd(va2, b2, r);
        r = vec_madd(va3, b3, r);

        store_vec4(r, out + i * 4);
    }
}

void altivec_normalize_vec3(float *out_xyz, const float *in_xyz, size_t count) {
    __vector float zero = (__vector float){0.0f, 0.0f, 0.0f, 0.0f};
    for (size_t i = 0; i < count; ++i) {
        float x = in_xyz[i * 3 + 0];
        float y = in_xyz[i * 3 + 1];
        float z = in_xyz[i * 3 + 2];
        __vector float v = (__vector float){x, y, z, 0.0f};
        __vector float v_sq = vec_madd(v, v, zero);
        float len_sq = v_sq[0] + v_sq[1] + v_sq[2];
        if (len_sq > 1e-12f) {
            __vector float vl = (__vector float){len_sq, len_sq, len_sq, len_sq};
            __vector float inv = altivec_rsqrt_vec(vl);
            float inv_len = inv[0];
            out_xyz[i * 3 + 0] = x * inv_len;
            out_xyz[i * 3 + 1] = y * inv_len;
            out_xyz[i * 3 + 2] = z * inv_len;
        } else {
            out_xyz[i * 3 + 0] = 0.0f;
            out_xyz[i * 3 + 1] = 0.0f;
            out_xyz[i * 3 + 2] = 0.0f;
        }
    }
}

void altivec_compute_vertex_normals(
    float *normals_out,
    const float *facenormals_in,
    unsigned resolution) {
    __vector float zero = (__vector float){0.0f, 0.0f, 0.0f, 0.0f};
    const unsigned mask = resolution - 1;

    for (unsigned y = 0; y < resolution; ++y) {
        unsigned y1 = y * resolution;
        unsigned y2 = ((y + resolution - 1) & mask) * resolution;
        for (unsigned x = 0; x < resolution; ++x) {
            unsigned x1 = (x + resolution - 1) & mask;

            const float *p_f0 = &facenormals_in[(2 * (y1 + x)) * 3];
            const float *p_f1 = &facenormals_in[(2 * (y1 + x1) + 1) * 3];
            const float *p_f2 = &facenormals_in[(2 * (y2 + x1)) * 3];
            const float *p_f3 = &facenormals_in[(2 * (y2 + x1) + 1) * 3];
            const float *p_f4 = &facenormals_in[(2 * (y2 + x)) * 3];
            const float *p_f5 = &facenormals_in[(2 * (y1 + x) + 1) * 3];

            __vector float f0 = load_vec4(p_f0);
            __vector float f1 = load_vec4(p_f1);
            __vector float f2 = load_vec4(p_f2);
            __vector float f3 = load_vec4(p_f3);
            __vector float f4 = load_vec4(p_f4);
            __vector float f5 = load_vec4(p_f5);

            __vector float sum01 = vec_add(f0, f1);
            __vector float sum23 = vec_add(f2, f3);
            __vector float sum45 = vec_add(f4, f5);
            __vector float sum = vec_add(vec_add(sum01, sum23), sum45);

            __vector float sum_sq = vec_madd(sum, sum, zero);
            float lsq = sum_sq[0] + sum_sq[1] + sum_sq[2];

            float *out_v = &normals_out[(y1 + x) * 3];
            if (lsq > 1e-12f) {
                __vector float vl = (__vector float){lsq, lsq, lsq, lsq};
                __vector float inv = altivec_rsqrt_vec(vl);
                __vector float norm = vec_madd(sum, inv, zero);
                out_v[0] = norm[0];
                out_v[1] = norm[1];
                out_v[2] = norm[2];
            } else {
                out_v[0] = 0.0f;
                out_v[1] = 0.0f;
                out_v[2] = 1.0f;
            }
        }
    }
}

void altivec_generate_geoclipmap_row(
    float *dest,
    const float *wavedata_row,
    const float *normals_row,
    const float *foam_row,
    float coordxoff,
    float coordyoff,
    float viewpos_z,
    float coordxadd,
    unsigned count) {
    float cur_xoff = coordxoff;
    for (unsigned x = 0; x < count; ++x) {
        dest[0] = wavedata_row[x * 3 + 0] + cur_xoff;
        dest[1] = wavedata_row[x * 3 + 1] + coordyoff;
        dest[2] = wavedata_row[x * 3 + 2] - viewpos_z;
        dest[3] = normals_row[x * 3 + 0];
        dest[4] = normals_row[x * 3 + 1];
        dest[5] = normals_row[x * 3 + 2];
        dest[6] = foam_row[x];
        dest += 7;
        cur_xoff += coordxadd;
    }
}
