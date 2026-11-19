// Garage bay markings, painted with the ground so parked cars sit on top.
// Compiled into the scenery tenants of the islands that have garages.
static void draw_bays(void) {
  for (int i = 0; i < g_nlot; i++) {
    if (g_lot[i].kind != LOT_GARAGE) continue;
    int32_t slot[GARAGE_SLOTS][3];
    int ns = city_garage_slots(&g_lot[i], slot, GARAGE_SLOTS);
    for (int g2 = 0; g2 < ns; g2++) {
      int32_t c3 = fcos((uint16_t)slot[g2][2]), s3 = fsin((uint16_t)slot[g2][2]);
      const int32_t bl[4][2] = { { 74, 52 }, { 74, -52 }, { -74, -52 }, { -74, 52 } };
      int32_t bq[4][3];
      for (int q = 0; q < 4; q++) {
        bq[q][0] = slot[g2][0] + ((bl[q][0] * c3 - bl[q][1] * s3) >> 10);
        bq[q][1] = slot[g2][1] + ((bl[q][0] * s3 + bl[q][1] * c3) >> 10);
        bq[q][2] = 2;
      }
      gfx_world_poly((const int32_t (*)[3])bq, 4, ARGB(2,2,2));
    }
  }
}
