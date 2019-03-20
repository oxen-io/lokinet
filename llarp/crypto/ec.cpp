#include "ec.hpp"
namespace llarp
{
  namespace sodium
  {
    static inline uint64_t
    load_3(const byte_t *in)
    {
      uint64_t result;
      result = (uint64_t)in[0];
      result |= ((uint64_t)in[1]) << 8;
      result |= ((uint64_t)in[2]) << 16;
      return result;
    }

    static inline uint64_t
    load_4(const byte_t *in)
    {
      uint64_t result;
      result = (uint64_t)in[0];
      result |= ((uint64_t)in[1]) << 8;
      result |= ((uint64_t)in[2]) << 16;
      result |= ((uint64_t)in[3]) << 24;
      return result;
    }

    /* From ge_msub.c */

    /*
    r = p - q
    */

    static void
    ge25519_msub(ge25519_p1p1 *r, const ge25519_p3 *p, const ge25519_precomp *q)
    {
      fe25519 t0;
      fe25519_add(r->X, p->Y, p->X);
      fe25519_sub(r->Y, p->Y, p->X);
      fe25519_mul(r->Z, r->X, q->yminusx);
      fe25519_mul(r->Y, r->Y, q->yplusx);
      fe25519_mul(r->T, q->xy2d, p->T);
      fe25519_add(t0, p->Z, p->Z);
      fe25519_sub(r->X, r->Z, r->Y);
      fe25519_add(r->Y, r->Z, r->Y);
      fe25519_sub(r->Z, t0, r->T);
      fe25519_add(r->T, t0, r->T);
    }

    typedef ge25519_cached ge25519_dsmp[8];

    static const ge25519_precomp ge_Bi[8] = {
        {{25967493, -14356035, 29566456, 3660896, -12694345, 4014787, 27544626,
          -11754271, -6079156, 2047605},
         {-12545711, 934262, -2722910, 3049990, -727428, 9406986, 12720692,
          5043384, 19500929, -15469378},
         {-8738181, 4489570, 9688441, -14785194, 10184609, -12363380, 29287919,
          11864899, -24514362, -4438546}},
        {{15636291, -9688557, 24204773, -7912398, 616977, -16685262, 27787600,
          -14772189, 28944400, -1550024},
         {16568933, 4717097, -11556148, -1102322, 15682896, -11807043, 16354577,
          -11775962, 7689662, 11199574},
         {30464156, -5976125, -11779434, -15670865, 23220365, 15915852, 7512774,
          10017326, -17749093, -9920357}},
        {{10861363, 11473154, 27284546, 1981175, -30064349, 12577861, 32867885,
          14515107, -15438304, 10819380},
         {4708026, 6336745, 20377586, 9066809, -11272109, 6594696, -25653668,
          12483688, -12668491, 5581306},
         {19563160, 16186464, -29386857, 4097519, 10237984, -4348115, 28542350,
          13850243, -23678021, -15815942}},
        {{5153746, 9909285, 1723747, -2777874, 30523605, 5516873, 19480852,
          5230134, -23952439, -15175766},
         {-30269007, -3463509, 7665486, 10083793, 28475525, 1649722, 20654025,
          16520125, 30598449, 7715701},
         {28881845, 14381568, 9657904, 3680757, -20181635, 7843316, -31400660,
          1370708, 29794553, -1409300}},
        {{-22518993, -6692182, 14201702, -8745502, -23510406, 8844726, 18474211,
          -1361450, -13062696, 13821877},
         {-6455177, -7839871, 3374702, -4740862, -27098617, -10571707, 31655028,
          -7212327, 18853322, -14220951},
         {4566830, -12963868, -28974889, -12240689, -7602672, -2830569,
          -8514358, -10431137, 2207753, -3209784}},
        {{-25154831, -4185821, 29681144, 7868801, -6854661, -9423865, -12437364,
          -663000, -31111463, -16132436},
         {25576264, -2703214, 7349804, -11814844, 16472782, 9300885, 3844789,
          15725684, 171356, 6466918},
         {23103977, 13316479, 9739013, -16149481, 817875, -15038942, 8965339,
          -14088058, -30714912, 16193877}},
        {{-33521811, 3180713, -2394130, 14003687, -16903474, -16270840,
          17238398, 4729455, -18074513, 9256800},
         {-25182317, -4174131, 32336398, 5036987, -21236817, 11360617, 22616405,
          9761698, -19827198, 630305},
         {-13720693, 2639453, -24237460, -7406481, 9494427, -5774029, -6554551,
          -15960994, -2449256, -14291300}},
        {{-3151181, -5046075, 9282714, 6866145, -31907062, -863023, -18940575,
          15033784, 25105118, -7894876},
         {-24326370, 15950226, -31801215, -14592823, -11662737, -5090925,
          1573892, -2625887, 2198790, -15804619},
         {-3099351, 10324967, -2241613, 7453183, -5446979, -2735503, -13812022,
          -16236442, -32461234, -12290683}}};

    /* From ge_p2_0.c */

    static void
    ge25519_p2_0(ge25519_p2 *h)
    {
      fe25519_0(h->X);
      fe25519_1(h->Y);
      fe25519_1(h->Z);
    }

    static void
    ge25519_madd(ge25519_p1p1 *r, const ge25519_p3 *p, const ge25519_precomp *q)
    {
      fe25519 t0;
      fe25519_add(r->X, p->Y, p->X);
      fe25519_sub(r->Y, p->Y, p->X);
      fe25519_mul(r->Z, r->X, q->yplusx);
      fe25519_mul(r->Y, r->Y, q->yminusx);
      fe25519_mul(r->T, q->xy2d, p->T);
      fe25519_add(t0, p->Z, p->Z);
      fe25519_sub(r->X, r->Z, r->Y);
      fe25519_add(r->Y, r->Z, r->Y);
      fe25519_add(r->Z, t0, r->T);
      fe25519_sub(r->T, t0, r->T);
    }

    /* From ge_p2_dbl.c */

    /*
    r = 2 * p
    */

    static void
    ge25519_p2_dbl(ge25519_p1p1 *r, const ge25519_p2 *p)
    {
      fe25519 t0;
      fe25519_sq(r->X, p->X);
      fe25519_sq(r->Z, p->Y);
      fe25519_sq2(r->T, p->Z);
      fe25519_add(r->Y, p->X, p->Y);
      fe25519_sq(t0, r->Y);
      fe25519_add(r->Y, r->Z, r->X);
      fe25519_sub(r->Z, r->Z, r->X);
      fe25519_sub(r->X, t0, r->Y);
      fe25519_sub(r->T, r->T, r->Z);
    }

    /* From ge_p3_to_p2.c */

    /*
    r = p
    */

    static void
    ge25519_p3_to_p2(ge25519_p2 *r, const ge25519_p3 *p)
    {
      fe25519_copy(r->X, p->X);
      fe25519_copy(r->Y, p->Y);
      fe25519_copy(r->Z, p->Z);
    }

    /* From ge_p3_dbl.c */

    /*
    r = 2 * p
    */

    static void
    ge25519_p3_dbl(ge25519_p1p1 *r, const ge25519_p3 *p)
    {
      ge25519_p2 q;
      ge25519_p3_to_p2(&q, p);
      ge25519_p2_dbl(r, &q);
    }

    /* From ge_double_scalarmult.c, modified */

    static void
    slide(signed char *r, const byte_t *a)
    {
      int i;
      int b;
      int k;

      for(i = 0; i < 256; ++i)
      {
        r[i] = 1 & (a[i >> 3] >> (i & 7));
      }

      for(i = 0; i < 256; ++i)
      {
        if(r[i])
        {
          for(b = 1; b <= 6 && i + b < 256; ++b)
          {
            if(r[i + b])
            {
              if(r[i] + (r[i + b] << b) <= 15)
              {
                r[i] += r[i + b] << b;
                r[i + b] = 0;
              }
              else if(r[i] - (r[i + b] << b) >= -15)
              {
                r[i] -= r[i + b] << b;
                for(k = i + b; k < 256; ++k)
                {
                  if(!r[k])
                  {
                    r[k] = 1;
                    break;
                  }
                  r[k] = 0;
                }
              }
              else
                break;
            }
          }
        }
      }
    }

    static void
    ge25519_dsm_precomp(ge25519_dsmp &r, const ge25519_p3 *s)
    {
      ge25519_p1p1 t;
      ge25519_p3 s2, u;
      ge25519_p3_to_cached(&r[0], s);
      ge25519_p3_dbl(&t, s);
      ge25519_p1p1_to_p3(&s2, &t);
      ge25519_add(&t, &s2, &r[0]);
      ge25519_p1p1_to_p3(&u, &t);
      ge25519_p3_to_cached(&r[1], &u);
      ge25519_add(&t, &s2, &r[1]);
      ge25519_p1p1_to_p3(&u, &t);
      ge25519_p3_to_cached(&r[2], &u);
      ge25519_add(&t, &s2, &r[2]);
      ge25519_p1p1_to_p3(&u, &t);
      ge25519_p3_to_cached(&r[3], &u);
      ge25519_add(&t, &s2, &r[3]);
      ge25519_p1p1_to_p3(&u, &t);
      ge25519_p3_to_cached(&r[4], &u);
      ge25519_add(&t, &s2, &r[4]);
      ge25519_p1p1_to_p3(&u, &t);
      ge25519_p3_to_cached(&r[5], &u);
      ge25519_add(&t, &s2, &r[5]);
      ge25519_p1p1_to_p3(&u, &t);
      ge25519_p3_to_cached(&r[6], &u);
      ge25519_add(&t, &s2, &r[6]);
      ge25519_p1p1_to_p3(&u, &t);
      ge25519_p3_to_cached(&r[7], &u);
    }

    void
    sc25519_reduce32(byte_t *s)
    {
      int64_t s0  = 2097151 & load_3(s);
      int64_t s1  = 2097151 & (load_4(s + 2) >> 5);
      int64_t s2  = 2097151 & (load_3(s + 5) >> 2);
      int64_t s3  = 2097151 & (load_4(s + 7) >> 7);
      int64_t s4  = 2097151 & (load_4(s + 10) >> 4);
      int64_t s5  = 2097151 & (load_3(s + 13) >> 1);
      int64_t s6  = 2097151 & (load_4(s + 15) >> 6);
      int64_t s7  = 2097151 & (load_3(s + 18) >> 3);
      int64_t s8  = 2097151 & load_3(s + 21);
      int64_t s9  = 2097151 & (load_4(s + 23) >> 5);
      int64_t s10 = 2097151 & (load_3(s + 26) >> 2);
      int64_t s11 = (load_4(s + 28) >> 7);
      int64_t s12 = 0;
      int64_t carry0;
      int64_t carry1;
      int64_t carry2;
      int64_t carry3;
      int64_t carry4;
      int64_t carry5;
      int64_t carry6;
      int64_t carry7;
      int64_t carry8;
      int64_t carry9;
      int64_t carry10;
      int64_t carry11;

      carry0 = (s0 + (1 << 20)) >> 21;
      s1 += carry0;
      s0 -= carry0 << 21;
      carry2 = (s2 + (1 << 20)) >> 21;
      s3 += carry2;
      s2 -= carry2 << 21;
      carry4 = (s4 + (1 << 20)) >> 21;
      s5 += carry4;
      s4 -= carry4 << 21;
      carry6 = (s6 + (1 << 20)) >> 21;
      s7 += carry6;
      s6 -= carry6 << 21;
      carry8 = (s8 + (1 << 20)) >> 21;
      s9 += carry8;
      s8 -= carry8 << 21;
      carry10 = (s10 + (1 << 20)) >> 21;
      s11 += carry10;
      s10 -= carry10 << 21;

      carry1 = (s1 + (1 << 20)) >> 21;
      s2 += carry1;
      s1 -= carry1 << 21;
      carry3 = (s3 + (1 << 20)) >> 21;
      s4 += carry3;
      s3 -= carry3 << 21;
      carry5 = (s5 + (1 << 20)) >> 21;
      s6 += carry5;
      s5 -= carry5 << 21;
      carry7 = (s7 + (1 << 20)) >> 21;
      s8 += carry7;
      s7 -= carry7 << 21;
      carry9 = (s9 + (1 << 20)) >> 21;
      s10 += carry9;
      s9 -= carry9 << 21;
      carry11 = (s11 + (1 << 20)) >> 21;
      s12 += carry11;
      s11 -= carry11 << 21;

      s0 += s12 * 666643;
      s1 += s12 * 470296;
      s2 += s12 * 654183;
      s3 -= s12 * 997805;
      s4 += s12 * 136657;
      s5 -= s12 * 683901;
      s12 = 0;

      carry0 = s0 >> 21;
      s1 += carry0;
      s0 -= carry0 << 21;
      carry1 = s1 >> 21;
      s2 += carry1;
      s1 -= carry1 << 21;
      carry2 = s2 >> 21;
      s3 += carry2;
      s2 -= carry2 << 21;
      carry3 = s3 >> 21;
      s4 += carry3;
      s3 -= carry3 << 21;
      carry4 = s4 >> 21;
      s5 += carry4;
      s4 -= carry4 << 21;
      carry5 = s5 >> 21;
      s6 += carry5;
      s5 -= carry5 << 21;
      carry6 = s6 >> 21;
      s7 += carry6;
      s6 -= carry6 << 21;
      carry7 = s7 >> 21;
      s8 += carry7;
      s7 -= carry7 << 21;
      carry8 = s8 >> 21;
      s9 += carry8;
      s8 -= carry8 << 21;
      carry9 = s9 >> 21;
      s10 += carry9;
      s9 -= carry9 << 21;
      carry10 = s10 >> 21;
      s11 += carry10;
      s10 -= carry10 << 21;
      carry11 = s11 >> 21;
      s12 += carry11;
      s11 -= carry11 << 21;

      s0 += s12 * 666643;
      s1 += s12 * 470296;
      s2 += s12 * 654183;
      s3 -= s12 * 997805;
      s4 += s12 * 136657;
      s5 -= s12 * 683901;

      carry0 = s0 >> 21;
      s1 += carry0;
      s0 -= carry0 << 21;
      carry1 = s1 >> 21;
      s2 += carry1;
      s1 -= carry1 << 21;
      carry2 = s2 >> 21;
      s3 += carry2;
      s2 -= carry2 << 21;
      carry3 = s3 >> 21;
      s4 += carry3;
      s3 -= carry3 << 21;
      carry4 = s4 >> 21;
      s5 += carry4;
      s4 -= carry4 << 21;
      carry5 = s5 >> 21;
      s6 += carry5;
      s5 -= carry5 << 21;
      carry6 = s6 >> 21;
      s7 += carry6;
      s6 -= carry6 << 21;
      carry7 = s7 >> 21;
      s8 += carry7;
      s7 -= carry7 << 21;
      carry8 = s8 >> 21;
      s9 += carry8;
      s8 -= carry8 << 21;
      carry9 = s9 >> 21;
      s10 += carry9;
      s9 -= carry9 << 21;
      carry10 = s10 >> 21;
      s11 += carry10;
      s10 -= carry10 << 21;

      s[0]  = s0 >> 0;
      s[1]  = s0 >> 8;
      s[2]  = (s0 >> 16) | (s1 << 5);
      s[3]  = s1 >> 3;
      s[4]  = s1 >> 11;
      s[5]  = (s1 >> 19) | (s2 << 2);
      s[6]  = s2 >> 6;
      s[7]  = (s2 >> 14) | (s3 << 7);
      s[8]  = s3 >> 1;
      s[9]  = s3 >> 9;
      s[10] = (s3 >> 17) | (s4 << 4);
      s[11] = s4 >> 4;
      s[12] = s4 >> 12;
      s[13] = (s4 >> 20) | (s5 << 1);
      s[14] = s5 >> 7;
      s[15] = (s5 >> 15) | (s6 << 6);
      s[16] = s6 >> 2;
      s[17] = s6 >> 10;
      s[18] = (s6 >> 18) | (s7 << 3);
      s[19] = s7 >> 5;
      s[20] = s7 >> 13;
      s[21] = s8 >> 0;
      s[22] = s8 >> 8;
      s[23] = (s8 >> 16) | (s9 << 5);
      s[24] = s9 >> 3;
      s[25] = s9 >> 11;
      s[26] = (s9 >> 19) | (s10 << 2);
      s[27] = s10 >> 6;
      s[28] = (s10 >> 14) | (s11 << 7);
      s[29] = s11 >> 1;
      s[30] = s11 >> 9;
      s[31] = s11 >> 17;
    }

    /*
      Input:
      a[0]+256*a[1]+...+256^31*a[31] = a
      b[0]+256*b[1]+...+256^31*b[31] = b
      c[0]+256*c[1]+...+256^31*c[31] = c

      Output:
      s[0]+256*s[1]+...+256^31*s[31] = (c-ab) mod l
      where l = 2^252 + 27742317777372353535851937790883648493.
    */
    void
    sc25519_mulsub(byte_t *s, const byte_t *a, const byte_t *b, const byte_t *c)
    {
      int64_t a0  = 2097151 & load_3(a);
      int64_t a1  = 2097151 & (load_4(a + 2) >> 5);
      int64_t a2  = 2097151 & (load_3(a + 5) >> 2);
      int64_t a3  = 2097151 & (load_4(a + 7) >> 7);
      int64_t a4  = 2097151 & (load_4(a + 10) >> 4);
      int64_t a5  = 2097151 & (load_3(a + 13) >> 1);
      int64_t a6  = 2097151 & (load_4(a + 15) >> 6);
      int64_t a7  = 2097151 & (load_3(a + 18) >> 3);
      int64_t a8  = 2097151 & load_3(a + 21);
      int64_t a9  = 2097151 & (load_4(a + 23) >> 5);
      int64_t a10 = 2097151 & (load_3(a + 26) >> 2);
      int64_t a11 = (load_4(a + 28) >> 7);
      int64_t b0  = 2097151 & load_3(b);
      int64_t b1  = 2097151 & (load_4(b + 2) >> 5);
      int64_t b2  = 2097151 & (load_3(b + 5) >> 2);
      int64_t b3  = 2097151 & (load_4(b + 7) >> 7);
      int64_t b4  = 2097151 & (load_4(b + 10) >> 4);
      int64_t b5  = 2097151 & (load_3(b + 13) >> 1);
      int64_t b6  = 2097151 & (load_4(b + 15) >> 6);
      int64_t b7  = 2097151 & (load_3(b + 18) >> 3);
      int64_t b8  = 2097151 & load_3(b + 21);
      int64_t b9  = 2097151 & (load_4(b + 23) >> 5);
      int64_t b10 = 2097151 & (load_3(b + 26) >> 2);
      int64_t b11 = (load_4(b + 28) >> 7);
      int64_t c0  = 2097151 & load_3(c);
      int64_t c1  = 2097151 & (load_4(c + 2) >> 5);
      int64_t c2  = 2097151 & (load_3(c + 5) >> 2);
      int64_t c3  = 2097151 & (load_4(c + 7) >> 7);
      int64_t c4  = 2097151 & (load_4(c + 10) >> 4);
      int64_t c5  = 2097151 & (load_3(c + 13) >> 1);
      int64_t c6  = 2097151 & (load_4(c + 15) >> 6);
      int64_t c7  = 2097151 & (load_3(c + 18) >> 3);
      int64_t c8  = 2097151 & load_3(c + 21);
      int64_t c9  = 2097151 & (load_4(c + 23) >> 5);
      int64_t c10 = 2097151 & (load_3(c + 26) >> 2);
      int64_t c11 = (load_4(c + 28) >> 7);
      int64_t s0;
      int64_t s1;
      int64_t s2;
      int64_t s3;
      int64_t s4;
      int64_t s5;
      int64_t s6;
      int64_t s7;
      int64_t s8;
      int64_t s9;
      int64_t s10;
      int64_t s11;
      int64_t s12;
      int64_t s13;
      int64_t s14;
      int64_t s15;
      int64_t s16;
      int64_t s17;
      int64_t s18;
      int64_t s19;
      int64_t s20;
      int64_t s21;
      int64_t s22;
      int64_t s23;
      int64_t carry0;
      int64_t carry1;
      int64_t carry2;
      int64_t carry3;
      int64_t carry4;
      int64_t carry5;
      int64_t carry6;
      int64_t carry7;
      int64_t carry8;
      int64_t carry9;
      int64_t carry10;
      int64_t carry11;
      int64_t carry12;
      int64_t carry13;
      int64_t carry14;
      int64_t carry15;
      int64_t carry16;
      int64_t carry17;
      int64_t carry18;
      int64_t carry19;
      int64_t carry20;
      int64_t carry21;
      int64_t carry22;

      s0 = c0 - a0 * b0;
      s1 = c1 - (a0 * b1 + a1 * b0);
      s2 = c2 - (a0 * b2 + a1 * b1 + a2 * b0);
      s3 = c3 - (a0 * b3 + a1 * b2 + a2 * b1 + a3 * b0);
      s4 = c4 - (a0 * b4 + a1 * b3 + a2 * b2 + a3 * b1 + a4 * b0);
      s5 = c5 - (a0 * b5 + a1 * b4 + a2 * b3 + a3 * b2 + a4 * b1 + a5 * b0);
      s6 = c6
          - (a0 * b6 + a1 * b5 + a2 * b4 + a3 * b3 + a4 * b2 + a5 * b1
             + a6 * b0);
      s7 = c7
          - (a0 * b7 + a1 * b6 + a2 * b5 + a3 * b4 + a4 * b3 + a5 * b2 + a6 * b1
             + a7 * b0);
      s8 = c8
          - (a0 * b8 + a1 * b7 + a2 * b6 + a3 * b5 + a4 * b4 + a5 * b3 + a6 * b2
             + a7 * b1 + a8 * b0);
      s9 = c9
          - (a0 * b9 + a1 * b8 + a2 * b7 + a3 * b6 + a4 * b5 + a5 * b4 + a6 * b3
             + a7 * b2 + a8 * b1 + a9 * b0);
      s10 = c10
          - (a0 * b10 + a1 * b9 + a2 * b8 + a3 * b7 + a4 * b6 + a5 * b5
             + a6 * b4 + a7 * b3 + a8 * b2 + a9 * b1 + a10 * b0);
      s11 = c11
          - (a0 * b11 + a1 * b10 + a2 * b9 + a3 * b8 + a4 * b7 + a5 * b6
             + a6 * b5 + a7 * b4 + a8 * b3 + a9 * b2 + a10 * b1 + a11 * b0);
      s12 = -(a1 * b11 + a2 * b10 + a3 * b9 + a4 * b8 + a5 * b7 + a6 * b6
              + a7 * b5 + a8 * b4 + a9 * b3 + a10 * b2 + a11 * b1);
      s13 = -(a2 * b11 + a3 * b10 + a4 * b9 + a5 * b8 + a6 * b7 + a7 * b6
              + a8 * b5 + a9 * b4 + a10 * b3 + a11 * b2);
      s14 = -(a3 * b11 + a4 * b10 + a5 * b9 + a6 * b8 + a7 * b7 + a8 * b6
              + a9 * b5 + a10 * b4 + a11 * b3);
      s15 = -(a4 * b11 + a5 * b10 + a6 * b9 + a7 * b8 + a8 * b7 + a9 * b6
              + a10 * b5 + a11 * b4);
      s16 = -(a5 * b11 + a6 * b10 + a7 * b9 + a8 * b8 + a9 * b7 + a10 * b6
              + a11 * b5);
      s17 = -(a6 * b11 + a7 * b10 + a8 * b9 + a9 * b8 + a10 * b7 + a11 * b6);
      s18 = -(a7 * b11 + a8 * b10 + a9 * b9 + a10 * b8 + a11 * b7);
      s19 = -(a8 * b11 + a9 * b10 + a10 * b9 + a11 * b8);
      s20 = -(a9 * b11 + a10 * b10 + a11 * b9);
      s21 = -(a10 * b11 + a11 * b10);
      s22 = -a11 * b11;
      s23 = 0;

      carry0 = (s0 + (1 << 20)) >> 21;
      s1 += carry0;
      s0 -= carry0 << 21;
      carry2 = (s2 + (1 << 20)) >> 21;
      s3 += carry2;
      s2 -= carry2 << 21;
      carry4 = (s4 + (1 << 20)) >> 21;
      s5 += carry4;
      s4 -= carry4 << 21;
      carry6 = (s6 + (1 << 20)) >> 21;
      s7 += carry6;
      s6 -= carry6 << 21;
      carry8 = (s8 + (1 << 20)) >> 21;
      s9 += carry8;
      s8 -= carry8 << 21;
      carry10 = (s10 + (1 << 20)) >> 21;
      s11 += carry10;
      s10 -= carry10 << 21;
      carry12 = (s12 + (1 << 20)) >> 21;
      s13 += carry12;
      s12 -= carry12 << 21;
      carry14 = (s14 + (1 << 20)) >> 21;
      s15 += carry14;
      s14 -= carry14 << 21;
      carry16 = (s16 + (1 << 20)) >> 21;
      s17 += carry16;
      s16 -= carry16 << 21;
      carry18 = (s18 + (1 << 20)) >> 21;
      s19 += carry18;
      s18 -= carry18 << 21;
      carry20 = (s20 + (1 << 20)) >> 21;
      s21 += carry20;
      s20 -= carry20 << 21;
      carry22 = (s22 + (1 << 20)) >> 21;
      s23 += carry22;
      s22 -= carry22 << 21;

      carry1 = (s1 + (1 << 20)) >> 21;
      s2 += carry1;
      s1 -= carry1 << 21;
      carry3 = (s3 + (1 << 20)) >> 21;
      s4 += carry3;
      s3 -= carry3 << 21;
      carry5 = (s5 + (1 << 20)) >> 21;
      s6 += carry5;
      s5 -= carry5 << 21;
      carry7 = (s7 + (1 << 20)) >> 21;
      s8 += carry7;
      s7 -= carry7 << 21;
      carry9 = (s9 + (1 << 20)) >> 21;
      s10 += carry9;
      s9 -= carry9 << 21;
      carry11 = (s11 + (1 << 20)) >> 21;
      s12 += carry11;
      s11 -= carry11 << 21;
      carry13 = (s13 + (1 << 20)) >> 21;
      s14 += carry13;
      s13 -= carry13 << 21;
      carry15 = (s15 + (1 << 20)) >> 21;
      s16 += carry15;
      s15 -= carry15 << 21;
      carry17 = (s17 + (1 << 20)) >> 21;
      s18 += carry17;
      s17 -= carry17 << 21;
      carry19 = (s19 + (1 << 20)) >> 21;
      s20 += carry19;
      s19 -= carry19 << 21;
      carry21 = (s21 + (1 << 20)) >> 21;
      s22 += carry21;
      s21 -= carry21 << 21;

      s11 += s23 * 666643;
      s12 += s23 * 470296;
      s13 += s23 * 654183;
      s14 -= s23 * 997805;
      s15 += s23 * 136657;
      s16 -= s23 * 683901;

      s10 += s22 * 666643;
      s11 += s22 * 470296;
      s12 += s22 * 654183;
      s13 -= s22 * 997805;
      s14 += s22 * 136657;
      s15 -= s22 * 683901;

      s9 += s21 * 666643;
      s10 += s21 * 470296;
      s11 += s21 * 654183;
      s12 -= s21 * 997805;
      s13 += s21 * 136657;
      s14 -= s21 * 683901;

      s8 += s20 * 666643;
      s9 += s20 * 470296;
      s10 += s20 * 654183;
      s11 -= s20 * 997805;
      s12 += s20 * 136657;
      s13 -= s20 * 683901;

      s7 += s19 * 666643;
      s8 += s19 * 470296;
      s9 += s19 * 654183;
      s10 -= s19 * 997805;
      s11 += s19 * 136657;
      s12 -= s19 * 683901;

      s6 += s18 * 666643;
      s7 += s18 * 470296;
      s8 += s18 * 654183;
      s9 -= s18 * 997805;
      s10 += s18 * 136657;
      s11 -= s18 * 683901;

      carry6 = (s6 + (1 << 20)) >> 21;
      s7 += carry6;
      s6 -= carry6 << 21;
      carry8 = (s8 + (1 << 20)) >> 21;
      s9 += carry8;
      s8 -= carry8 << 21;
      carry10 = (s10 + (1 << 20)) >> 21;
      s11 += carry10;
      s10 -= carry10 << 21;
      carry12 = (s12 + (1 << 20)) >> 21;
      s13 += carry12;
      s12 -= carry12 << 21;
      carry14 = (s14 + (1 << 20)) >> 21;
      s15 += carry14;
      s14 -= carry14 << 21;
      carry16 = (s16 + (1 << 20)) >> 21;
      s17 += carry16;
      s16 -= carry16 << 21;

      carry7 = (s7 + (1 << 20)) >> 21;
      s8 += carry7;
      s7 -= carry7 << 21;
      carry9 = (s9 + (1 << 20)) >> 21;
      s10 += carry9;
      s9 -= carry9 << 21;
      carry11 = (s11 + (1 << 20)) >> 21;
      s12 += carry11;
      s11 -= carry11 << 21;
      carry13 = (s13 + (1 << 20)) >> 21;
      s14 += carry13;
      s13 -= carry13 << 21;
      carry15 = (s15 + (1 << 20)) >> 21;
      s16 += carry15;
      s15 -= carry15 << 21;

      s5 += s17 * 666643;
      s6 += s17 * 470296;
      s7 += s17 * 654183;
      s8 -= s17 * 997805;
      s9 += s17 * 136657;
      s10 -= s17 * 683901;

      s4 += s16 * 666643;
      s5 += s16 * 470296;
      s6 += s16 * 654183;
      s7 -= s16 * 997805;
      s8 += s16 * 136657;
      s9 -= s16 * 683901;

      s3 += s15 * 666643;
      s4 += s15 * 470296;
      s5 += s15 * 654183;
      s6 -= s15 * 997805;
      s7 += s15 * 136657;
      s8 -= s15 * 683901;

      s2 += s14 * 666643;
      s3 += s14 * 470296;
      s4 += s14 * 654183;
      s5 -= s14 * 997805;
      s6 += s14 * 136657;
      s7 -= s14 * 683901;

      s1 += s13 * 666643;
      s2 += s13 * 470296;
      s3 += s13 * 654183;
      s4 -= s13 * 997805;
      s5 += s13 * 136657;
      s6 -= s13 * 683901;

      s0 += s12 * 666643;
      s1 += s12 * 470296;
      s2 += s12 * 654183;
      s3 -= s12 * 997805;
      s4 += s12 * 136657;
      s5 -= s12 * 683901;
      s12 = 0;

      carry0 = (s0 + (1 << 20)) >> 21;
      s1 += carry0;
      s0 -= carry0 << 21;
      carry2 = (s2 + (1 << 20)) >> 21;
      s3 += carry2;
      s2 -= carry2 << 21;
      carry4 = (s4 + (1 << 20)) >> 21;
      s5 += carry4;
      s4 -= carry4 << 21;
      carry6 = (s6 + (1 << 20)) >> 21;
      s7 += carry6;
      s6 -= carry6 << 21;
      carry8 = (s8 + (1 << 20)) >> 21;
      s9 += carry8;
      s8 -= carry8 << 21;
      carry10 = (s10 + (1 << 20)) >> 21;
      s11 += carry10;
      s10 -= carry10 << 21;

      carry1 = (s1 + (1 << 20)) >> 21;
      s2 += carry1;
      s1 -= carry1 << 21;
      carry3 = (s3 + (1 << 20)) >> 21;
      s4 += carry3;
      s3 -= carry3 << 21;
      carry5 = (s5 + (1 << 20)) >> 21;
      s6 += carry5;
      s5 -= carry5 << 21;
      carry7 = (s7 + (1 << 20)) >> 21;
      s8 += carry7;
      s7 -= carry7 << 21;
      carry9 = (s9 + (1 << 20)) >> 21;
      s10 += carry9;
      s9 -= carry9 << 21;
      carry11 = (s11 + (1 << 20)) >> 21;
      s12 += carry11;
      s11 -= carry11 << 21;

      s0 += s12 * 666643;
      s1 += s12 * 470296;
      s2 += s12 * 654183;
      s3 -= s12 * 997805;
      s4 += s12 * 136657;
      s5 -= s12 * 683901;
      s12 = 0;

      carry0 = s0 >> 21;
      s1 += carry0;
      s0 -= carry0 << 21;
      carry1 = s1 >> 21;
      s2 += carry1;
      s1 -= carry1 << 21;
      carry2 = s2 >> 21;
      s3 += carry2;
      s2 -= carry2 << 21;
      carry3 = s3 >> 21;
      s4 += carry3;
      s3 -= carry3 << 21;
      carry4 = s4 >> 21;
      s5 += carry4;
      s4 -= carry4 << 21;
      carry5 = s5 >> 21;
      s6 += carry5;
      s5 -= carry5 << 21;
      carry6 = s6 >> 21;
      s7 += carry6;
      s6 -= carry6 << 21;
      carry7 = s7 >> 21;
      s8 += carry7;
      s7 -= carry7 << 21;
      carry8 = s8 >> 21;
      s9 += carry8;
      s8 -= carry8 << 21;
      carry9 = s9 >> 21;
      s10 += carry9;
      s9 -= carry9 << 21;
      carry10 = s10 >> 21;
      s11 += carry10;
      s10 -= carry10 << 21;
      carry11 = s11 >> 21;
      s12 += carry11;
      s11 -= carry11 << 21;

      s0 += s12 * 666643;
      s1 += s12 * 470296;
      s2 += s12 * 654183;
      s3 -= s12 * 997805;
      s4 += s12 * 136657;
      s5 -= s12 * 683901;

      carry0 = s0 >> 21;
      s1 += carry0;
      s0 -= carry0 << 21;
      carry1 = s1 >> 21;
      s2 += carry1;
      s1 -= carry1 << 21;
      carry2 = s2 >> 21;
      s3 += carry2;
      s2 -= carry2 << 21;
      carry3 = s3 >> 21;
      s4 += carry3;
      s3 -= carry3 << 21;
      carry4 = s4 >> 21;
      s5 += carry4;
      s4 -= carry4 << 21;
      carry5 = s5 >> 21;
      s6 += carry5;
      s5 -= carry5 << 21;
      carry6 = s6 >> 21;
      s7 += carry6;
      s6 -= carry6 << 21;
      carry7 = s7 >> 21;
      s8 += carry7;
      s7 -= carry7 << 21;
      carry8 = s8 >> 21;
      s9 += carry8;
      s8 -= carry8 << 21;
      carry9 = s9 >> 21;
      s10 += carry9;
      s9 -= carry9 << 21;
      carry10 = s10 >> 21;
      s11 += carry10;
      s10 -= carry10 << 21;

      s[0]  = s0 >> 0;
      s[1]  = s0 >> 8;
      s[2]  = (s0 >> 16) | (s1 << 5);
      s[3]  = s1 >> 3;
      s[4]  = s1 >> 11;
      s[5]  = (s1 >> 19) | (s2 << 2);
      s[6]  = s2 >> 6;
      s[7]  = (s2 >> 14) | (s3 << 7);
      s[8]  = s3 >> 1;
      s[9]  = s3 >> 9;
      s[10] = (s3 >> 17) | (s4 << 4);
      s[11] = s4 >> 4;
      s[12] = s4 >> 12;
      s[13] = (s4 >> 20) | (s5 << 1);
      s[14] = s5 >> 7;
      s[15] = (s5 >> 15) | (s6 << 6);
      s[16] = s6 >> 2;
      s[17] = s6 >> 10;
      s[18] = (s6 >> 18) | (s7 << 3);
      s[19] = s7 >> 5;
      s[20] = s7 >> 13;
      s[21] = s8 >> 0;
      s[22] = s8 >> 8;
      s[23] = (s8 >> 16) | (s9 << 5);
      s[24] = s9 >> 3;
      s[25] = s9 >> 11;
      s[26] = (s9 >> 19) | (s10 << 2);
      s[27] = s10 >> 6;
      s[28] = (s10 >> 14) | (s11 << 7);
      s[29] = s11 >> 1;
      s[30] = s11 >> 9;
      s[31] = s11 >> 17;
    }

    void
    sc25519_sub(byte_t *s, const byte_t *a, const byte_t *b)
    {
      int64_t a0  = 2097151 & load_3(a);
      int64_t a1  = 2097151 & (load_4(a + 2) >> 5);
      int64_t a2  = 2097151 & (load_3(a + 5) >> 2);
      int64_t a3  = 2097151 & (load_4(a + 7) >> 7);
      int64_t a4  = 2097151 & (load_4(a + 10) >> 4);
      int64_t a5  = 2097151 & (load_3(a + 13) >> 1);
      int64_t a6  = 2097151 & (load_4(a + 15) >> 6);
      int64_t a7  = 2097151 & (load_3(a + 18) >> 3);
      int64_t a8  = 2097151 & load_3(a + 21);
      int64_t a9  = 2097151 & (load_4(a + 23) >> 5);
      int64_t a10 = 2097151 & (load_3(a + 26) >> 2);
      int64_t a11 = (load_4(a + 28) >> 7);
      int64_t b0  = 2097151 & load_3(b);
      int64_t b1  = 2097151 & (load_4(b + 2) >> 5);
      int64_t b2  = 2097151 & (load_3(b + 5) >> 2);
      int64_t b3  = 2097151 & (load_4(b + 7) >> 7);
      int64_t b4  = 2097151 & (load_4(b + 10) >> 4);
      int64_t b5  = 2097151 & (load_3(b + 13) >> 1);
      int64_t b6  = 2097151 & (load_4(b + 15) >> 6);
      int64_t b7  = 2097151 & (load_3(b + 18) >> 3);
      int64_t b8  = 2097151 & load_3(b + 21);
      int64_t b9  = 2097151 & (load_4(b + 23) >> 5);
      int64_t b10 = 2097151 & (load_3(b + 26) >> 2);
      int64_t b11 = (load_4(b + 28) >> 7);
      int64_t s0  = a0 - b0;
      int64_t s1  = a1 - b1;
      int64_t s2  = a2 - b2;
      int64_t s3  = a3 - b3;
      int64_t s4  = a4 - b4;
      int64_t s5  = a5 - b5;
      int64_t s6  = a6 - b6;
      int64_t s7  = a7 - b7;
      int64_t s8  = a8 - b8;
      int64_t s9  = a9 - b9;
      int64_t s10 = a10 - b10;
      int64_t s11 = a11 - b11;
      int64_t s12 = 0;
      int64_t carry0;
      int64_t carry1;
      int64_t carry2;
      int64_t carry3;
      int64_t carry4;
      int64_t carry5;
      int64_t carry6;
      int64_t carry7;
      int64_t carry8;
      int64_t carry9;
      int64_t carry10;
      int64_t carry11;

      carry0 = (s0 + (1 << 20)) >> 21;
      s1 += carry0;
      s0 -= carry0 << 21;
      carry2 = (s2 + (1 << 20)) >> 21;
      s3 += carry2;
      s2 -= carry2 << 21;
      carry4 = (s4 + (1 << 20)) >> 21;
      s5 += carry4;
      s4 -= carry4 << 21;
      carry6 = (s6 + (1 << 20)) >> 21;
      s7 += carry6;
      s6 -= carry6 << 21;
      carry8 = (s8 + (1 << 20)) >> 21;
      s9 += carry8;
      s8 -= carry8 << 21;
      carry10 = (s10 + (1 << 20)) >> 21;
      s11 += carry10;
      s10 -= carry10 << 21;

      carry1 = (s1 + (1 << 20)) >> 21;
      s2 += carry1;
      s1 -= carry1 << 21;
      carry3 = (s3 + (1 << 20)) >> 21;
      s4 += carry3;
      s3 -= carry3 << 21;
      carry5 = (s5 + (1 << 20)) >> 21;
      s6 += carry5;
      s5 -= carry5 << 21;
      carry7 = (s7 + (1 << 20)) >> 21;
      s8 += carry7;
      s7 -= carry7 << 21;
      carry9 = (s9 + (1 << 20)) >> 21;
      s10 += carry9;
      s9 -= carry9 << 21;
      carry11 = (s11 + (1 << 20)) >> 21;
      s12 += carry11;
      s11 -= carry11 << 21;

      s0 += s12 * 666643;
      s1 += s12 * 470296;
      s2 += s12 * 654183;
      s3 -= s12 * 997805;
      s4 += s12 * 136657;
      s5 -= s12 * 683901;
      s12 = 0;

      carry0 = s0 >> 21;
      s1 += carry0;
      s0 -= carry0 << 21;
      carry1 = s1 >> 21;
      s2 += carry1;
      s1 -= carry1 << 21;
      carry2 = s2 >> 21;
      s3 += carry2;
      s2 -= carry2 << 21;
      carry3 = s3 >> 21;
      s4 += carry3;
      s3 -= carry3 << 21;
      carry4 = s4 >> 21;
      s5 += carry4;
      s4 -= carry4 << 21;
      carry5 = s5 >> 21;
      s6 += carry5;
      s5 -= carry5 << 21;
      carry6 = s6 >> 21;
      s7 += carry6;
      s6 -= carry6 << 21;
      carry7 = s7 >> 21;
      s8 += carry7;
      s7 -= carry7 << 21;
      carry8 = s8 >> 21;
      s9 += carry8;
      s8 -= carry8 << 21;
      carry9 = s9 >> 21;
      s10 += carry9;
      s9 -= carry9 << 21;
      carry10 = s10 >> 21;
      s11 += carry10;
      s10 -= carry10 << 21;
      carry11 = s11 >> 21;
      s12 += carry11;
      s11 -= carry11 << 21;

      s0 += s12 * 666643;
      s1 += s12 * 470296;
      s2 += s12 * 654183;
      s3 -= s12 * 997805;
      s4 += s12 * 136657;
      s5 -= s12 * 683901;

      carry0 = s0 >> 21;
      s1 += carry0;
      s0 -= carry0 << 21;
      carry1 = s1 >> 21;
      s2 += carry1;
      s1 -= carry1 << 21;
      carry2 = s2 >> 21;
      s3 += carry2;
      s2 -= carry2 << 21;
      carry3 = s3 >> 21;
      s4 += carry3;
      s3 -= carry3 << 21;
      carry4 = s4 >> 21;
      s5 += carry4;
      s4 -= carry4 << 21;
      carry5 = s5 >> 21;
      s6 += carry5;
      s5 -= carry5 << 21;
      carry6 = s6 >> 21;
      s7 += carry6;
      s6 -= carry6 << 21;
      carry7 = s7 >> 21;
      s8 += carry7;
      s7 -= carry7 << 21;
      carry8 = s8 >> 21;
      s9 += carry8;
      s8 -= carry8 << 21;
      carry9 = s9 >> 21;
      s10 += carry9;
      s9 -= carry9 << 21;
      carry10 = s10 >> 21;
      s11 += carry10;
      s10 -= carry10 << 21;

      s[0]  = s0 >> 0;
      s[1]  = s0 >> 8;
      s[2]  = (s0 >> 16) | (s1 << 5);
      s[3]  = s1 >> 3;
      s[4]  = s1 >> 11;
      s[5]  = (s1 >> 19) | (s2 << 2);
      s[6]  = s2 >> 6;
      s[7]  = (s2 >> 14) | (s3 << 7);
      s[8]  = s3 >> 1;
      s[9]  = s3 >> 9;
      s[10] = (s3 >> 17) | (s4 << 4);
      s[11] = s4 >> 4;
      s[12] = s4 >> 12;
      s[13] = (s4 >> 20) | (s5 << 1);
      s[14] = s5 >> 7;
      s[15] = (s5 >> 15) | (s6 << 6);
      s[16] = s6 >> 2;
      s[17] = s6 >> 10;
      s[18] = (s6 >> 18) | (s7 << 3);
      s[19] = s7 >> 5;
      s[20] = s7 >> 13;
      s[21] = s8 >> 0;
      s[22] = s8 >> 8;
      s[23] = (s8 >> 16) | (s9 << 5);
      s[24] = s9 >> 3;
      s[25] = s9 >> 11;
      s[26] = (s9 >> 19) | (s10 << 2);
      s[27] = s10 >> 6;
      s[28] = (s10 >> 14) | (s11 << 7);
      s[29] = s11 >> 1;
      s[30] = s11 >> 9;
      s[31] = s11 >> 17;
    }

    static int64_t
    signum(int64_t a)
    {
      return a > 0 ? 1 : a < 0 ? -1 : 0;
    }

    int
    sc25519_check(const byte_t *s)
    {
      int64_t s0 = load_4(s);
      int64_t s1 = load_4(s + 4);
      int64_t s2 = load_4(s + 8);
      int64_t s3 = load_4(s + 12);
      int64_t s4 = load_4(s + 16);
      int64_t s5 = load_4(s + 20);
      int64_t s6 = load_4(s + 24);
      int64_t s7 = load_4(s + 28);
      return (signum(1559614444 - s0) + (signum(1477600026 - s1) << 1)
              + (signum(2734136534 - s2) << 2) + (signum(350157278 - s3) << 3)
              + (signum(-s4) << 4) + (signum(-s5) << 5) + (signum(-s6) << 6)
              + (signum(268435456 - s7) << 7))
          >> 8;
    }

    static const fe25519 fe25519_d = {-10913610, 13857413, -15372611, 6949391,
                                      114729,    -8787816, -6275908,  -3247719,
                                      -18696448, -12055116}; /* d */

    const fe25519 fe25519_sqrtm1 = {-32595792, -7943725, 9377950,   3500415,
                                    12389472,  -272473,  -25146209, -2005654,
                                    326686,    11406482}; /* sqrt(-1) */
    static int
    fe25519_isnonzero(const fe25519 &f)
    {
      unsigned char s[32];
      fe25519_tobytes(s, f);
      return (((int)(s[0] | s[1] | s[2] | s[3] | s[4] | s[5] | s[6] | s[7]
                     | s[8] | s[9] | s[10] | s[11] | s[12] | s[13] | s[14]
                     | s[15] | s[16] | s[17] | s[18] | s[19] | s[20] | s[21]
                     | s[22] | s[23] | s[24] | s[25] | s[26] | s[27] | s[28]
                     | s[29] | s[30] | s[31])
               - 1)
              >> 8)
          + 1;
    }

    static void
    fe25519_divpowm1(fe25519 &r, const fe25519 u, const fe25519 v)
    {
      fe25519 v3, uv7, t0, t1, t2;
      int i;

      fe25519_sq(v3, v);
      fe25519_mul(v3, v3, v); /* v3 = v^3 */
      fe25519_sq(uv7, v3);
      fe25519_mul(uv7, uv7, v);
      fe25519_mul(uv7, uv7, u); /* uv7 = uv^7 */

      /*fe_pow22523(uv7, uv7);*/

      /* From fe_pow22523.c */

      fe25519_sq(t0, uv7);
      fe25519_sq(t1, t0);
      fe25519_sq(t1, t1);
      fe25519_mul(t1, uv7, t1);
      fe25519_mul(t0, t0, t1);
      fe25519_sq(t0, t0);
      fe25519_mul(t0, t1, t0);
      fe25519_sq(t1, t0);
      for(i = 0; i < 4; ++i)
      {
        fe25519_sq(t1, t1);
      }
      fe25519_mul(t0, t1, t0);
      fe25519_sq(t1, t0);
      for(i = 0; i < 9; ++i)
      {
        fe25519_sq(t1, t1);
      }
      fe25519_mul(t1, t1, t0);
      fe25519_sq(t2, t1);
      for(i = 0; i < 19; ++i)
      {
        fe25519_sq(t2, t2);
      }
      fe25519_mul(t1, t2, t1);
      for(i = 0; i < 10; ++i)
      {
        fe25519_sq(t1, t1);
      }
      fe25519_mul(t0, t1, t0);
      fe25519_sq(t1, t0);
      for(i = 0; i < 49; ++i)
      {
        fe25519_sq(t1, t1);
      }
      fe25519_mul(t1, t1, t0);
      fe25519_sq(t2, t1);
      for(i = 0; i < 99; ++i)
      {
        fe25519_sq(t2, t2);
      }
      fe25519_mul(t1, t2, t1);
      for(i = 0; i < 50; ++i)
      {
        fe25519_sq(t1, t1);
      }
      fe25519_mul(t0, t1, t0);
      fe25519_sq(t0, t0);
      fe25519_sq(t0, t0);
      fe25519_mul(t0, t0, uv7);

      /* End fe_pow22523.c */
      /* t0 = (uv^7)^((q-5)/8) */
      fe25519_mul(t0, t0, v3);
      fe25519_mul(r, t0, u); /* u^(m+1)v^(-(m+1)) */
    }

    int
    ge25519_frombytes_vartime(ge25519_p3 *h, const byte_t *s)
    {
      fe25519 u;
      fe25519 v;
      fe25519 vxx;
      fe25519 check;

      /* From fe_frombytes.c */

      int64_t h0 = load_4(s);
      int64_t h1 = load_3(s + 4) << 6;
      int64_t h2 = load_3(s + 7) << 5;
      int64_t h3 = load_3(s + 10) << 3;
      int64_t h4 = load_3(s + 13) << 2;
      int64_t h5 = load_4(s + 16);
      int64_t h6 = load_3(s + 20) << 7;
      int64_t h7 = load_3(s + 23) << 5;
      int64_t h8 = load_3(s + 26) << 4;
      int64_t h9 = (load_3(s + 29) & 8388607) << 2;
      int64_t carry0;
      int64_t carry1;
      int64_t carry2;
      int64_t carry3;
      int64_t carry4;
      int64_t carry5;
      int64_t carry6;
      int64_t carry7;
      int64_t carry8;
      int64_t carry9;

      /* Validate the number to be canonical */
      if(h9 == 33554428 && h8 == 268435440 && h7 == 536870880
         && h6 == 2147483520 && h5 == 4294967295 && h4 == 67108860
         && h3 == 134217720 && h2 == 536870880 && h1 == 1073741760
         && h0 >= 4294967277)
      {
        return -1;
      }

      carry9 = (h9 + (int64_t)(1 << 24)) >> 25;
      h0 += carry9 * 19;
      h9 -= carry9 << 25;
      carry1 = (h1 + (int64_t)(1 << 24)) >> 25;
      h2 += carry1;
      h1 -= carry1 << 25;
      carry3 = (h3 + (int64_t)(1 << 24)) >> 25;
      h4 += carry3;
      h3 -= carry3 << 25;
      carry5 = (h5 + (int64_t)(1 << 24)) >> 25;
      h6 += carry5;
      h5 -= carry5 << 25;
      carry7 = (h7 + (int64_t)(1 << 24)) >> 25;
      h8 += carry7;
      h7 -= carry7 << 25;

      carry0 = (h0 + (int64_t)(1 << 25)) >> 26;
      h1 += carry0;
      h0 -= carry0 << 26;
      carry2 = (h2 + (int64_t)(1 << 25)) >> 26;
      h3 += carry2;
      h2 -= carry2 << 26;
      carry4 = (h4 + (int64_t)(1 << 25)) >> 26;
      h5 += carry4;
      h4 -= carry4 << 26;
      carry6 = (h6 + (int64_t)(1 << 25)) >> 26;
      h7 += carry6;
      h6 -= carry6 << 26;
      carry8 = (h8 + (int64_t)(1 << 25)) >> 26;
      h9 += carry8;
      h8 -= carry8 << 26;

      h->Y[0] = h0;
      h->Y[1] = h1;
      h->Y[2] = h2;
      h->Y[3] = h3;
      h->Y[4] = h4;
      h->Y[5] = h5;
      h->Y[6] = h6;
      h->Y[7] = h7;
      h->Y[8] = h8;
      h->Y[9] = h9;

      /* End fe_frombytes.c */

      fe25519_1(h->Z);
      fe25519_sq(u, h->Y);
      fe25519_mul(v, u, fe25519_d);
      fe25519_sub(u, u, h->Z); /* u = y^2-1 */
      fe25519_add(v, v, h->Z); /* v = dy^2+1 */

      fe25519_divpowm1(h->X, u, v); /* x = uv^3(uv^7)^((q-5)/8) */

      fe25519_sq(vxx, h->X);
      fe25519_mul(vxx, vxx, v);
      fe25519_sub(check, vxx, u); /* vx^2-u */
      if(fe25519_isnonzero(check))
      {
        fe25519_add(check, vxx, u); /* vx^2+u */
        if(fe25519_isnonzero(check))
        {
          return -1;
        }
        fe25519_mul(h->X, h->X, fe25519_sqrtm1);
      }

      if(fe25519_isnegative(h->X) != (s[31] >> 7))
      {
        /* If x = 0, the sign must be positive */
        if(!fe25519_isnonzero(h->X))
        {
          return -1;
        }
        fe25519_neg(h->X, h->X);
      }

      fe25519_mul(h->T, h->X, h->Y);
      return 0;
    }

    /*
    r = a * A + b * B
    where a = a[0]+256*a[1]+...+256^31 a[31].
    and b = b[0]+256*b[1]+...+256^31 b[31].
    B is the Ed25519 base point (x,4/5) with x positive.
    */

    void
    ge25519_double_scalarmult_base_vartime(ge25519_p2 *r, const byte_t *a,
                                           const ge25519_p3 *A, const byte_t *b)
    {
      signed char aslide[256];
      signed char bslide[256];
      ge25519_dsmp Ai; /* A, 3A, 5A, 7A, 9A, 11A, 13A, 15A */
      ge25519_p1p1 t;
      ge25519_p3 u;
      int i;

      slide(aslide, a);
      slide(bslide, b);
      ge25519_dsm_precomp(Ai, A);

      ge25519_p2_0(r);

      for(i = 255; i >= 0; --i)
      {
        if(aslide[i] || bslide[i])
          break;
      }

      for(; i >= 0; --i)
      {
        ge25519_p2_dbl(&t, r);

        if(aslide[i] > 0)
        {
          ge25519_p1p1_to_p3(&u, &t);
          ge25519_add(&t, &u, &Ai[aslide[i] / 2]);
        }
        else if(aslide[i] < 0)
        {
          ge25519_p1p1_to_p3(&u, &t);
          ge25519_sub(&t, &u, &Ai[(-aslide[i]) / 2]);
        }

        if(bslide[i] > 0)
        {
          ge25519_p1p1_to_p3(&u, &t);
          ge25519_madd(&t, &u, &ge_Bi[bslide[i] / 2]);
        }
        else if(bslide[i] < 0)
        {
          ge25519_p1p1_to_p3(&u, &t);
          ge25519_msub(&t, &u, &ge_Bi[(-bslide[i]) / 2]);
        }

        ge25519_p1p1_to_p2(r, &t);
      }
    }

  }  // namespace sodium
}  // namespace llarp
