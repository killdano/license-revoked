#include "wr.h"

// ---------------------------------------------------------------------------
// Software synthesiser, second attempt.
//
// The first version got the technology right - one continuous 8 kHz PCM
// stream, mixed every frame - and the sound design wrong: it built every
// sound out of retriggered oscillator blips with a fixed timbre, so the
// engine was a stutter, the siren a chirp and every impact a click.
//
// Two ideas fix nearly all of it.
//
// 1. Continuous things are continuous. The engine, the siren, the rotor
//    and the flamethrower are stateful generators that run every sample
//    for as long as their source exists. Nothing is retriggered.
//
// 2. Every one-shot goes through a low-pass filter whose cutoff DECAYS
//    along with the amplitude. A bang is bright for a few milliseconds and
//    dull for the rest of its life - that single fact is most of what
//    separates a gunshot, an explosion or a slammed door from a beep.
//
// 8000 Hz / 25 fps = 320 samples a frame, signed 8-bit. Six one-shot voices
// plus the four generators is a few thousand integer ops per frame.
// ---------------------------------------------------------------------------

#define SR          8000
#define FRAME_SAMP  (SR / 25)          // 320
#define PRIME_FRAMES 3                 // latency cushion, ~120 ms
#define MAX_VOICE   6

enum { WV_NOISE = 0, WV_SINE, WV_SAW, WV_TRI, WV_SQUARE };

// A recipe for one layer of a sound. Eight bytes.
typedef struct {
  uint8_t wave;
  uint8_t f0;       // start pitch, in 16 Hz units (noise: ignored)
  int8_t  glide;    // pitch change per 8 samples, in phase-step units
  uint8_t len;      // length, in 8 ms units
  uint8_t amp;      // 0..255
  uint8_t decay;    // amplitude multiplier per sample, Q8 (255 = long tail)
  uint8_t cut;      // low-pass coefficient, Q8: 255 = wide open
  uint8_t cutdec;   // cutoff multiplier per 8 samples, Q8
} VSpec;

typedef struct {
  uint8_t  wave;
  uint8_t  cut, cutdec, decay;
  uint16_t phase, step, len;
  int16_t  slide, amp, lp;
} Voice;

static Voice s_v[MAX_VOICE];
static int8_t s_buf[FRAME_SAMP];
static bool s_stream_ok;
static uint8_t s_muted, s_off;
static uint8_t s_vol_scale = 80;

// Its own PRNG, so audio can never perturb the simulation.
static uint32_t s_arng = 0x9e3779b9u;
static inline int32_t anoise(void) {
  s_arng ^= s_arng << 13; s_arng ^= s_arng >> 17; s_arng ^= s_arng << 5;
  return (int32_t)((s_arng >> 8) & 0xFF) - 128;
}

static int32_t osc(uint8_t wave, uint16_t ph) {
  switch (wave) {
    case WV_NOISE:  return anoise();
    case WV_SAW:    return (int32_t)(ph >> 8) - 128;
    case WV_TRI:    return (ph < 32768) ? (int32_t)(ph >> 7) - 128 : 383 - (int32_t)(ph >> 7);
    case WV_SQUARE: return (ph & 0x8000) ? 100 : -100;
    default:        return (fsin(ph) * 120) >> 10;
  }
}

// ---------------------------------------------------------------------------
// One-shots
// ---------------------------------------------------------------------------

static void voice_on(const VSpec *s) {
  if (s_muted || !s_stream_ok || s_off) return;
  // The quietest slot loses.
  Voice *v = &s_v[0];
  for (int i = 1; i < MAX_VOICE; i++) if (s_v[i].amp < v->amp) v = &s_v[i];
  v->wave = s->wave;
  v->phase = 0; v->lp = 0;
  v->step = (uint16_t)(((uint32_t)s->f0 * 16 * 65536) / SR);
  v->slide = s->glide;
  v->len = (uint16_t)(s->len * (SR / 125));
  v->amp = (int16_t)(s->amp << 4);           // Q4, so slow tails do not step
  v->decay = s->decay; v->cut = s->cut; v->cutdec = s->cutdec;
}

// Layers per effect. Decay and cutoff fall every EIGHT samples, so a
// multiplier of 240 is a 16 ms tail, 250 is 43 ms, 254 is 128 ms and 255
// is a quarter of a second - per sample, an 8-bit multiplier cannot say
// anything slower than "over in 30 ms".
// wave, f0/16, glide, len/8ms, amp, decay, cut, cutdec
static const VSpec SFX_TAB[] = {
  // punch: a dull thud                                                    0
  { WV_NOISE, 0, 0, 8, 170, 240, 90, 240 },
  { WV_SINE, 7, -4, 8, 140, 240, 255, 255 },
  // bat: harder, woodier                                                  2
  { WV_NOISE, 0, 0, 10, 200, 244, 140, 236 },
  { WV_SINE, 12, -6, 8, 150, 242, 255, 255 },
  // pistol: click, body, thump                                            4
  { WV_NOISE, 0, 0, 2, 255, 220, 255, 255 },
  { WV_NOISE, 0, 0, 16, 220, 250, 200, 236 },
  { WV_SINE, 11, -5, 10, 150, 246, 255, 255 },
  // shotgun: bigger, longer, lower                                        8
  { WV_NOISE, 0, 0, 3, 255, 225, 255, 255 },
  { WV_NOISE, 0, 0, 32, 240, 253, 180, 244 },
  { WV_SINE, 6, -3, 18, 170, 250, 255, 255 },
  // scream (man): a shout that breaks and falls. A rising glide read as
  // a slide whistle, which is not the effect.                            11
  { WV_SAW, 12, -2, 26, 130, 253, 80, 252 },
  { WV_NOISE, 0, 0, 20, 70, 250, 50, 255 },
  // scream (woman)                                                       13
  { WV_SAW, 24, -3, 24, 120, 253, 100, 252 },
  { WV_NOISE, 0, 0, 20, 60, 250, 60, 255 },
  // car start: the starter, then the engine generator takes over         15
  { WV_SQUARE, 3, 0, 40, 60, 255, 60, 255 },
  { WV_NOISE, 0, 0, 40, 70, 255, 40, 255 },
  // crash: a metallic bang - noise, two ringing partials, a thud         17
  { WV_NOISE, 0, 0, 14, 240, 250, 220, 240 },
  { WV_SQUARE, 26, 0, 32, 90, 253, 120, 252 },
  { WV_SQUARE, 43, 0, 24, 70, 252, 120, 252 },
  { WV_SINE, 5, -2, 14, 160, 248, 255, 255 },
  // pickup: two bright notes                                             21
  { WV_SINE, 55, 0, 10, 150, 250, 255, 255 },
  { WV_SINE, 82, 0, 14, 120, 251, 255, 255 },
  // busted: a long, sinking horn                                         23
  { WV_SAW, 8, -1, 100, 170, 255, 60, 255 },
  { WV_SQUARE, 4, -1, 100, 100, 255, 40, 255 },
  // wanted up: a two-tone blip                                           25
  { WV_TRI, 41, 0, 12, 140, 252, 255, 255 },
  { WV_TRI, 62, 0, 18, 130, 252, 255, 255 },
  // escape: two notes falling                                            27
  { WV_SINE, 62, -20, 16, 140, 252, 255, 255 },
  { WV_SINE, 41, -10, 24, 130, 253, 255, 255 },
  // die (man): a low groan that chokes off                               29
  { WV_SAW, 9, -1, 40, 150, 253, 60, 251 },
  { WV_NOISE, 0, 0, 30, 80, 251, 40, 255 },
  // die (woman)                                                          31
  { WV_SAW, 18, -2, 36, 140, 253, 80, 251 },
  { WV_NOISE, 0, 0, 30, 70, 251, 50, 255 },
  // explosion: crack, long low roar, sub thump, late crackle             33
  { WV_NOISE, 0, 0, 5, 255, 225, 255, 255 },
  { WV_NOISE, 0, 0, 120, 250, 255, 120, 251 },
  { WV_SINE, 3, -1, 60, 180, 254, 255, 255 },
  { WV_NOISE, 0, 0, 70, 90, 254, 40, 255 },
  // door: a clunk                                                        37
  { WV_NOISE, 0, 0, 6, 200, 240, 110, 240 },
  { WV_SINE, 8, -3, 6, 120, 240, 255, 255 },
  // smash: bodywork                                                      39
  { WV_NOISE, 0, 0, 10, 220, 246, 180, 240 },
  { WV_SINE, 5, -2, 10, 140, 244, 255, 255 },
  // arrest: a hard whistle and a shout                                   41
  { WV_SQUARE, 120, 0, 24, 90, 253, 255, 255 },
  { WV_SAW, 12, 1, 18, 90, 252, 70, 255 },
  // footstep                                                             43
  { WV_NOISE, 0, 0, 3, 60, 228, 90, 236 },
  // flame: the ignition whoomp (the roar itself is a generator)          44
  { WV_NOISE, 0, 0, 20, 170, 252, 60, 255 },
  // rocket: a whoosh that opens up and fades                             45
  { WV_NOISE, 0, 0, 50, 220, 254, 140, 253 },
  { WV_SINE, 7, -1, 30, 120, 252, 255, 255 },
  // glass: a bright shatter and two high rings                           47
  { WV_NOISE, 0, 0, 22, 210, 250, 255, 250 },
  { WV_SINE, 160, -40, 12, 90, 244, 255, 255 },
  { WV_SINE, 205, -50, 10, 70, 242, 255, 255 },
  // run over: a wet crunch, a low thud, a snap, and a cry cut short       50
  { WV_NOISE, 0, 0, 22, 255, 250, 60, 253 },
  { WV_SINE, 4, -2, 14, 200, 246, 255, 255 },
  { WV_NOISE, 0, 0, 3, 255, 215, 255, 255 },
  { WV_SAW, 15, -8, 12, 110, 244, 90, 250 },
};
// First layer of each effect, in SFX_* order, with a terminator.
static const uint8_t SFX_OFF[SFX_COUNT + 1] = {
  0, 2, 4, 7, 10, 12, 14, 16, 20, 22, 24, 26, 28, 30, 32, 36, 38, 40, 42, 43, 44, 46, 49, 53
};

static uint8_t s_sfx_cd[SFX_COUNT];
static uint8_t s_flame;                // frames of flamethrower roar left

void audio_sfx(uint8_t id) {
  if (id >= SFX_COUNT) return;
  if (id == SFX_FLAME) {
    // Held fire: the roar is a generator; only the first pull whoomps.
    if (!s_flame) voice_on(&SFX_TAB[SFX_OFF[id]]);
    s_flame = 5;
    return;
  }
  // The same sound cannot pile onto itself every frame.
  if (s_sfx_cd[id]) return;
  s_sfx_cd[id] = (id == SFX_EXPLODE || id == SFX_BUSTED) ? 20 : 4;
  for (int k = SFX_OFF[id]; k < SFX_OFF[id + 1]; k++) voice_on(&SFX_TAB[k]);
}

// ---------------------------------------------------------------------------
// Generators: engine, rotor, siren, flame. Each is a small stateful
// function called per sample with a level, and its own phase.
// ---------------------------------------------------------------------------

static uint16_t s_eng_ph;   static int16_t s_eng_lp;   static uint16_t s_eng_step; static int16_t s_eng_lvl;
static uint16_t s_rot_ph;   static int16_t s_rot_lp;   static int16_t s_rot_lvl;  static uint16_t s_rot_whine;
static uint16_t s_sir_ph;   static uint16_t s_sir_lfo; static int16_t s_sir_lvl;
static int16_t  s_flm_lp;   static uint16_t s_flm_lfo;

// A four-stroke: a pulse per cycle, low-passed into a putter at idle and a
// buzz at speed, with a burble of noise gated by the pulse.
static inline int32_t gen_engine(void) {
  s_eng_ph = (uint16_t)(s_eng_ph + s_eng_step);
  int32_t pulse = (s_eng_ph < 20000) ? 110 : -40;
  if (s_eng_ph < 20000) pulse += anoise() >> 2;
  s_eng_lp += (int16_t)(((pulse - s_eng_lp) * 60) >> 8);
  return ((int32_t)s_eng_lp * s_eng_lvl) >> 8;
}

// Blade slap: noise chopped at the blade-pass rate, low-passed, plus a
// faint turbine whine. The chop is what the little speaker reproduces.
static inline int32_t gen_rotor(void) {
  s_rot_ph = (uint16_t)(s_rot_ph + 110);              // ~13.4 Hz
  int32_t env = (s_rot_ph < 18000) ? 255 : 70;
  int32_t n = (anoise() * env) >> 8;
  s_rot_lp += (int16_t)(((n - s_rot_lp) * 40) >> 8);
  s_rot_whine = (uint16_t)(s_rot_whine + 11470);       // 1400 Hz
  int32_t out = s_rot_lp * 3 + (fsin(s_rot_whine) * 14 >> 10);
  return (out * s_rot_lvl) >> 8;
}

// A wail: a triangle sweeping 650 to 1300 Hz and back over 1.6 seconds.
static inline int32_t gen_siren(void) {
  s_sir_lfo = (uint16_t)(s_sir_lfo + 5);               // 0.6 Hz
  int32_t t = (s_sir_lfo < 32768) ? s_sir_lfo : 65535 - s_sir_lfo;   // 0..32767
  uint32_t f = 650 + ((uint32_t)t * 650 >> 15);
  s_sir_ph = (uint16_t)(s_sir_ph + (f * 65536) / SR);
  int32_t x = osc(WV_TRI, s_sir_ph) + (osc(WV_SQUARE, s_sir_ph) >> 2);
  return (x * s_sir_lvl) >> 8;
}

// A roar: brown noise with a flutter.
static inline int32_t gen_flame(void) {
  s_flm_lfo = (uint16_t)(s_flm_lfo + 200);             // ~24 Hz
  int32_t n = anoise();
  s_flm_lp += (int16_t)(((n - s_flm_lp) * 30) >> 8);
  int32_t env = 200 + (fsin(s_flm_lfo) * 50 >> 10);
  return (s_flm_lp * 3 * env) >> 8;
}

// The club: a pop track. 110 bpm over the four chords every pop song
// uses (C, G, Am, F), a kick every beat, a snare on two and four, hats on
// the eighths, a bass root, an arpeggio on the eighths and a two-bar
// hook on top. Original; the owner asked for something that sounds like
// a song rather than a mood.
static uint32_t s_mus_t;
static uint16_t s_bass_ph, s_arp_ph, s_mel_ph;
static inline int32_t gen_music(void) {
  static const uint8_t  ROOT[4] = { 131, 98, 110, 87 };                        // C3 G2 A2 F2
  static const uint16_t CHORD[4][3] = { {262,330,392}, {247,294,392}, {220,262,330}, {262,349,440} };
  static const uint16_t HOOK[16] = { 523,659,784,659, 587,659,587,523, 440,523,659,523, 587,523,494,523 };
  uint32_t t = s_mus_t++;
  const uint32_t BEAT = 4364;                                  // 110 bpm at 8 kHz
  uint32_t beat = t / BEAT, ph = t % BEAT, eighth = ph >= BEAT / 2;
  uint32_t bar = (beat / 4) & 3;
  int32_t out = 0;
  if (ph < 1200) {                                             // kick
    uint16_t kp = (uint16_t)(ph * 420 - (ph * ph) / 34);
    out += (fsin(kp) * (int32_t)(1200 - ph)) >> 13;
  }
  if ((beat & 1) && ph < 900) out += (anoise() * (int32_t)(900 - ph)) >> 12;          // snare
  uint32_t hph = ph % (BEAT / 2);
  if (hph < 220) out += (anoise() * (int32_t)(220 - hph)) >> 12;                      // hat
  s_bass_ph = (uint16_t)(s_bass_ph + ROOT[bar] * 8);
  out += (osc(WV_SQUARE, s_bass_ph) * (int32_t)(ph < 3000 ? 3000 - ph : 0)) >> 15;   // bass, plucked
  s_arp_ph = (uint16_t)(s_arp_ph + CHORD[bar][((beat * 2) + eighth) % 3] * 8);
  out += (fsin(s_arp_ph) * (int32_t)(BEAT / 2 - hph)) >> 15;                          // arpeggio
  uint32_t hi = ((beat * 2) + eighth) & 15;
  s_mel_ph = (uint16_t)(s_mel_ph + HOOK[hi] * 8);
  out += (osc(WV_TRI, s_mel_ph) * (int32_t)(BEAT / 2 - hph / 2)) >> 15;              // hook
  return out;
}

// The store's alarm: two tones, hard, a quarter of a second each.
static uint16_t s_alarm_ph;
static inline int32_t gen_alarm(void) {
  s_alarm_ph = (uint16_t)(s_alarm_ph + ((g_frame & 4) ? 7372 : 9011));    // 900 / 1100 Hz
  return osc(WV_SQUARE, s_alarm_ph) >> 2;
}

// ---------------------------------------------------------------------------

void audio_init(void) {
  memset(s_v, 0, sizeof(s_v));
  s_muted = speaker_is_muted() ? 1 : 0;
  s_stream_ok = false;
  if (!s_muted) {
    s_stream_ok = speaker_stream_open(SpeakerPcmFormat_8kHz_8bit, 80);
    if (s_stream_ok) {
      memset(s_buf, 0, sizeof(s_buf));
      for (int i = 0; i < PRIME_FRAMES; i++) speaker_stream_write(s_buf, sizeof(s_buf));
    }
  }
}

void audio_shutdown(void) {
  if (s_stream_ok) speaker_stream_close();
  s_stream_ok = false;
}

void audio_set_volume(uint8_t v) { s_vol_scale = v; }
void audio_set_enabled(bool on) { s_off = on ? 0 : 1; }

const int8_t *audio_peek_buf(int *len) { *len = FRAME_SAMP; return s_buf; }
bool audio_engine_active(void) { return s_eng_lvl > 0; }

// Level for a distant source: full inside `near`, fading to nothing at
// `far`, and zero if there is no source at all (d < 0).
static int16_t level_for(int32_t d, int32_t near, int32_t far) {
  if (d < 0 || d >= far) return 0;
  if (d <= near) return 255;
  return (int16_t)(255 - ((d - near) * 255) / (far - near));
}

static int32_t nearest_of(uint8_t type, bool flying_only) {
  int32_t best = -1;
  for (int i = 0; i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if (e->type != type || e->state == ST_DEAD) continue;
    if (flying_only && (e->state == ST_PARK)) continue;
    int32_t d = wr_dist(e->x - g_p.x, e->y - g_p.y);
    if (best < 0 || d < best) best = d;
  }
  return best;
}

// Silence: every voice off. The pause menu uses it - otherwise whatever
// was sounding when you paused holds its note for as long as you look at
// the menu, which is exactly as annoying as it sounds.
void audio_hush(void) {
  memset(s_v, 0, sizeof(s_v));
}

void audio_tick(void) {
  for (int i = 0; i < SFX_COUNT; i++) if (s_sfx_cd[i]) s_sfx_cd[i]--;
  if (s_flame) s_flame--;
  if (s_muted || !s_stream_ok) return;
  if (s_off) { memset(s_buf, 0, sizeof(s_buf)); speaker_stream_write(s_buf, sizeof(s_buf)); return; }

  // Where the generators are this frame. Levels ease so nothing clicks.
  bool heli = g_p.in_car && g_ent[g_p.in_car - 1].type == ENT_HELI;
  int32_t spd = wr_abs(g_p.spd);
  int16_t want_eng = (g_p.in_car && !heli) ? (int16_t)(60 + spd * 4) : 0;
  s_eng_step = (uint16_t)((26 + spd * 2) * 65536 / SR);
  int32_t hd = heli ? 0 : nearest_of(ENT_HELI, true);
  int16_t want_rot = level_for(hd, 2 * TILE, 12 * TILE);
  // The siren announces itself, then gets out of the way: half volume
  // after ten seconds of it, back to full once it has been gone a while.
  static uint16_t s_sir_on;
  int32_t cd = (g_p.wanted > 0) ? nearest_of(ENT_COPCAR, false) : -1;
  int16_t want_sir = level_for(cd, 2 * TILE, 11 * TILE) >> 1;
  if (want_sir) { if (s_sir_on < 500) s_sir_on++; } else if (s_sir_on) s_sir_on--;
  if (s_sir_on > 250) want_sir >>= 1;
  s_eng_lvl += (int16_t)((want_eng - s_eng_lvl) >> 3);
  s_rot_lvl += (int16_t)((want_rot - s_rot_lvl) >> 3);
  s_sir_lvl += (int16_t)((want_sir - s_sir_lvl) >> 3);

  for (int i = 0; i < FRAME_SAMP; i++) {
    int32_t acc = 0;
    for (int k = 0; k < MAX_VOICE; k++) {
      Voice *v = &s_v[k];
      if (!v->len) continue;
      int32_t x = osc(v->wave, v->phase);
      v->lp += (int16_t)(((x - v->lp) * v->cut) >> 8);
      acc += ((int32_t)v->lp * v->amp) >> 12;
      v->phase = (uint16_t)(v->phase + v->step);
      if ((i & 7) == 0) {
        int32_t ns = (int32_t)v->step + v->slide;
        v->step = (uint16_t)wr_clamp(ns, 1, 65535);
        v->cut = (uint8_t)(((int32_t)v->cut * v->cutdec) >> 8);
        if (v->cut < 4) v->cut = 4;
        v->amp = (int16_t)(((int32_t)v->amp * v->decay) >> 8);
      }
      if (--v->len == 0 || v->amp < 16) v->len = 0;
    }
    if (s_eng_lvl) acc += gen_engine();
    if (s_rot_lvl) acc += gen_rotor();
    if (s_sir_lvl) acc += gen_siren();
    if (s_flame)   acc += gen_flame();
    if (g_p.inside == IN_CLUB && !g_in.alarm) acc += gen_music();
    if (g_p.inside && g_in.alarm) acc += gen_alarm();

    // Gain, then a soft knee rather than a hard clip: bending the top of a
    // loud waveform sounds loud; slicing it sounds like a fault.
    acc = (acc * s_vol_scale) >> 7;
    if (acc >  96) acc =  96 + ((acc -  96) >> 2);
    if (acc < -96) acc = -96 + ((acc + 96) >> 2);
    s_buf[i] = (int8_t)wr_clamp(acc, -120, 120);
  }
  speaker_stream_write(s_buf, sizeof(s_buf));
}
