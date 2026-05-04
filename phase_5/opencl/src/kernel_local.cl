#ifndef ZNCC_MAX_DISPARITY
#define ZNCC_MAX_DISPARITY 256
#endif

#ifndef ZNCC_WINDOW_RADIUS
#define ZNCC_WINDOW_RADIUS 8
#endif

#define WG_X 16
#define WG_Y 16
#define TILE_W (WG_X + 2*ZNCC_WINDOW_RADIUS + 2*ZNCC_MAX_DISPARITY)
#define TILE_H (WG_Y + 2*ZNCC_WINDOW_RADIUS)

__attribute__((reqd_work_group_size(WG_X, WG_Y, 1)))
__kernel void zncc_local(
    __global uchar* left,
    __global uchar* right,
    __global uchar* disparity,
    int disp_sign,
    int width,
    int height)
{
    int gx = get_global_id(0);
    int gy = get_global_id(1);
    int lx = get_local_id(0);
    int ly = get_local_id(1);
    int group_x = get_group_id(0);
    int group_y = get_group_id(1);

    __local uchar tileL[TILE_H][TILE_W];
    __local uchar tileR[TILE_H][TILE_W];

    int tile_origin_x = group_x * WG_X - ZNCC_WINDOW_RADIUS - ZNCC_MAX_DISPARITY;
    int tile_origin_y = group_y * WG_Y - ZNCC_WINDOW_RADIUS;

    for (int local_y = ly; local_y < TILE_H; local_y += WG_Y) {
        for (int local_x = lx; local_x < TILE_W; local_x += WG_X) {
            int src_x = tile_origin_x + local_x;
            int src_y = tile_origin_y + local_y;
            uchar valueL = 0;
            uchar valueR = 0;
            if (src_x >= 0 && src_x < width && src_y >= 0 && src_y < height) {
                int idx = src_y * width + src_x;
                valueL = left[idx*4];
                valueR = right[idx*4];
            }
            tileL[local_y][local_x] = valueL;
            tileR[local_y][local_x] = valueR;
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    if (gx >= width || gy >= height) {
        return;
    }

    if (gx < ZNCC_WINDOW_RADIUS || gy < ZNCC_WINDOW_RADIUS ||
        gx >= width - ZNCC_WINDOW_RADIUS || gy >= height - ZNCC_WINDOW_RADIUS) {
        disparity[gy*width + gx] = 0;
        return;
    }

    int center_x = lx + ZNCC_WINDOW_RADIUS + ZNCC_MAX_DISPARITY;
    int center_y = ly + ZNCC_WINDOW_RADIUS;

    int best_d = 0;
    float best_score = -1.0f;

    for (int d = 0; d < ZNCC_MAX_DISPARITY && gx + d < width - ZNCC_WINDOW_RADIUS; d++) {
        float sumL = 0, sumR = 0, sumLR = 0, sumL2 = 0, sumR2 = 0;
        int count = 0;

        for (int wy = -ZNCC_WINDOW_RADIUS; wy <= ZNCC_WINDOW_RADIUS; wy++) {
            for (int wx = -ZNCC_WINDOW_RADIUS; wx <= ZNCC_WINDOW_RADIUS; wx++) {
                int lx2 = center_x + wx;
                int ly2 = center_y + wy;
                int lxR = lx2 - (d * disp_sign);

                float L = tileL[ly2][lx2];
                float R = (lxR >= 0 && lxR < TILE_W) ? tileR[ly2][lxR] : 0;

                sumL += L;
                sumR += R;
                sumLR += L * R;
                sumL2 += L * L;
                sumR2 += R * R;
                count++;
            }
        }

        float numerator = count * sumLR - sumL * sumR;
        float denomL = count * sumL2 - sumL * sumL;
        float denomR = count * sumR2 - sumR * sumR;

        if (denomL < 1e-5f || denomR < 1e-5f)
            continue;

        float score = numerator / sqrt(denomL * denomR);

        if (score > best_score) {
            best_score = score;
            best_d = d;
        }
    }

    disparity[gy*width + gx] = abs(best_d) * 255 / ZNCC_MAX_DISPARITY;
}
