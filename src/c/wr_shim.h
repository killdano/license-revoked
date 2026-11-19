#pragma once
// ---------------------------------------------------------------------------
// The shim.
//
// A domain may not name a host symbol, so every reference has to go through
// the context struct. Rewriting thirty call sites by hand would have made the
// domain sources unreadable and hard to keep in step with the rest of the
// game; instead this header maps the ordinary names onto the context, so a
// domain source looks like any other file in the project.
//
// The context pointer is a static inside the blob - blob-local data, which
// the loader relocates like any other - set once at entry.
//
// In the WR_MONOLITH build this header does nothing at all and the names
// resolve to the real globals, which is how the host test harness keeps
// working unchanged.
// ---------------------------------------------------------------------------

// Helpers shared between domains: one copy when compiled in, a private copy
// in each blob when built as overlays (a blob cannot call into another blob).
#if WR_MONOLITH
#define DOM_SHARED
#else
#define DOM_SHARED static
#endif

#if WR_MONOLITH

#define DOMAIN_ENTRY(name) \
  int name(const HostCtx *h_, int op, int arg) { (void)h_; return dom_dispatch(op, arg); }

#else   // built as a real overlay

static const HostCtx *H;

// State
#define g_p        (*H->p)
#define g_in       (*H->in)
#define g_ent      (H->ent)
#define g_prop     (H->prop)
#define g_lot      (H->lot)
#define g_gib      (H->gib)
#define g_pickup   (H->pickup)
#define g_nlot     (*H->nlot)
#define g_frame    (*H->frame)
#define PROP_HP    (H->prop_hp)

// Maths. These read the host's sine table, so they cannot be the inline
// versions from wr.h - that would be an absolute address into host memory.
#undef fsin
#undef fcos
#undef fmul
#undef wr_dist
#define fsin(a)         H->fsin(a)
#define fcos(a)         H->fcos(a)
#define fmul(a, b)      H->fmul((a), (b))
#define wr_dist(a, b)   H->dist((a), (b))
#define angle_to(a,b,c,d) H->angle_to((a), (b), (c), (d))
#define wr_rand_n(n)    H->rand_n(n)

// The world
#define cell_at_point(x, y)      H->cell_at_point((x), (y))
#define audio_sfx(id)            H->audio_sfx(id)
#define game_caption(c, f)       H->caption((c), (f))
#define pickup_drop(x, y, w)     H->pickup_drop((x), (y), (w))
#define gib_burst_debris(x, y, z, vx, vy, n, c) \
        H->gib_debris((x), (y), (z), (vx), (vy), (n), (c))
#define money_burst(x, y, n)     H->money_burst((x), (y), (n))
#define player_absorb(d)         H->player_absorb(d)
#define gfx_part(a,b,c,d,e,f,g,i,j,k,l,m,n)   H->part((a),(b),(c),(d),(e),(f),(g),(i),(j),(k),(l),(m),(n))
#define gfx_taper(a,b,c,d,e,f,g,i,j,k,l,m,n,o,p,q) \
        H->taper((a),(b),(c),(d),(e),(f),(g),(i),(j),(k),(l),(m),(n),(o),(p),(q))
#define gfx_world_poly(p,n,c)      H->world_poly((p),(n),(c))
#define gfx_clip_world_poly(w,n,o) H->clip_world_poly((w),(n),(o))
#define gfx_fill_poly(p,n,c)       H->fill_poly((p),(n),(c))
#define gfx_all_ok(p,n)            H->all_ok((p),(n))
#define gfx_area2(p,n)             H->area2((p),(n))
#define gfx_project(x,y,z)         H->project((x),(y),(z))
#define gfx_parts(a,b,c,d,e,f,g,i,j) H->parts((a),(b),(c),(d),(e),(f),(g),(i),(j))
#define gfx_ground_band(a,b,c,d,e,f) H->ground_band((a),(b),(c),(d),(e),(f))
#define gfx_shade(n,b,d)           H->shade((n),(b),(d))
#define gfx_depth_at(x,y)          H->depth_at((x),(y))
#define gfx_set_stipple(on)        H->set_stipple(on)
#define wr_sqrt(v)                 H->sqrt_(v)
#define wr_muldiv(a,b,c)           H->muldiv((a),(b),(c))
#undef wr_clamp
#undef wr_abs
#define wr_clamp(v,lo,hi)          H->clamp((v),(lo),(hi))
#define wr_abs(v)                  H->abs_(v)
#define s_eye_x                    (*H->eye_x)
#define s_eye_y                    (*H->eye_y)
#define s_psx                      (*H->psx)
#define s_psy                      (*H->psy)
#define s_pdepth                   (*H->pdepth)
#define city_shop_door(L,a,b,c,d) H->shop_door((L),(a),(b),(c),(d))
#define bridge_find(x,y,l,t)     H->bridge_find((x),(y),(l),(t))
#define ent_fire(x,y,a,w,f)      H->fire((x),(y),(a),(w),(f))
#define ent_nearest_of(t,x,y,r,a) H->nearest_of((t),(x),(y),(r),(a))
#define island_at(x,y)           H->island_at((x),(y))
#define cop_shoot(a,b,c,d,e,f,g,i) H->cop_shoot((a),(b),(c),(d),(e),(f),(g),(i))
#define free_slot()              H->free_slot()
#define free_slot_evict()        H->free_slot_evict()
#define steer(e,t,r)             H->steer((e),(t),(r))
#define vehicle_destroy(i,x)     H->vehicle_destroy((i),(x))
#define money_on_death(e)        H->money_on_death(e)
#define gib_burst(a,b,c,d,e,f)   H->gib_burst((a),(b),(c),(d),(e),(f))
#define add_heat(a)              H->add_heat(a)
#define wr_vibe(s)               H->vibe(s)
#define city_blocked(x,y,c)      H->city_blocked((x),(y),(c))
#define gfx_ground_quad(x,y,h,c) H->ground_quad((x),(y),(h),(c))
#define s_seen_x                 (*H->seen_x)
#define s_seen_y                 (*H->seen_y)
#define g_heli_up                (*H->heli_up)
#define g_bullet                 (H->bullet)
#define road_snap(x,y,a)         H->road_snap((x),(y),(a))
#define wr_rand()                H->rand32()
#define ISLANDS                  (H->islands)
#define g_menu                   (*H->menu)
#define g_taxi                   (*H->taxi)
#define graphics_context_set_fill_color(c,k)   H->set_fill((c),(k))
#define graphics_context_set_stroke_color(c,k) H->set_stroke((c),(k))
#define graphics_fill_rect(c,r,q,m)            H->fill_rect((c),(r),(q),(m))
#define graphics_draw_round_rect(c,r,q)        H->round_rect((c),(r),(q))
#define graphics_fill_circle(c,p,q)            H->fill_circle((c),(p),(q))
#define hud_text(c,s,f,x,y,w,h,k)              H->hud_text((c),(s),(f),(x),(y),(w),(h),(k))
#define hud_marker(c,x,y,z,k)                  H->hud_marker((c),(x),(y),(z),(k))
#define gfx_utoa(d,n,v)                        H->utoa((d),(n),(v))
#define audio_set_volume(v)                    H->audio_set_volume(v)
#define audio_set_enabled(b)                   H->audio_set_enabled(b)
#define graphics_draw_line(c,a,b)              H->draw_line((c),(a),(b))
#define g_venue                                (H->venue)
#define g_nvenue                               (*H->nvenue)
#define BRIDGES                                (H->bridges)
#define lot_is_airport(L)                      H->lot_is_airport(L)
#define g_park_col                             (*H->park_col)
#define g_lot_col                              (*H->lot_col)
#define car_half_dims(e,l,w)                   H->car_half_dims((e),(l),(w))
#define city_garage_slots(L,o,m)               H->garage_slots((L),(o),(m))
#define crash_damage(i,d)                      H->crash_damage((i),(d))
#define g_blame                                (*H->blame)
#define city_set_interior(o)                   H->city_set_interior(o)
#define ent_reset()                            H->ent_reset()
#define props_refresh()                        H->props_refresh()
#define pickups_refresh()                      H->pickups_refresh()
#define city_refresh(x,y)                      H->city_refresh((x),(y))
#define cell_at(x,y)                           H->cell_at((x),(y))
#define drive_along(e,s,x,y,l,c,p)             H->drive_along((e),(s),(x),(y),(l),(c),(p))
#define blood_add(x,y,s)                       H->blood_add((x),(y),(s))
#define ent_spawn(t)                           H->ent_spawn(t)
#define model_car(c,d)                         H->model_car((c),(d))
#define fire_bullet_kind(a,d,k)  H->fire_bullet_kind((a),(d),(k))
#define fire_bullet(a,d)         H->fire_bullet((a),(d))
#define damage_entity(e,d)       H->damage_entity((e),(d))
#define prop_hit(i,x,y,f)        H->prop_hit((i),(x),(y),(f))
#define kill_entity(e)           H->kill_entity(e)
#define cell_at_world(x,y)       H->cell_at_world((x),(y))
#define lot_at(x,y)              H->lot_at((x),(y))
#define ent_nearest(t,r)         H->ent_nearest((t),(r))
#define water_blocked(x,y)       H->water_blocked((x),(y))
#define domain_call(o,a)         H->place_call((o),(a))
#define domain2_call(o,a)        H->threat_call((o),(a))
#define bridge_in_gap(b,t)       H->bridge_in_gap((b),(t))
#define inside_leave()           H->inside_exit()      // a REQUEST; the kernel leaves after the call
#define inside_violence(k)       H->violence(k)
#define memset(d, c, n)          H->mem_set((d), (c), (n))

// The entry point. It is placed first in the blob so the loader can find it
// at offset 0 without a symbol table, and it latches the context before
// anything else runs.
#define DOMAIN_ENTRY(name)                                   \
  __attribute__((section(".text.entry"), used))              \
  int name(const HostCtx *h_, int op, int arg) {             \
    H = h_;                                                  \
    return dom_dispatch(op, arg);                            \
  }

#endif
