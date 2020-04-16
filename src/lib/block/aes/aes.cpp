/*
* AES
* (C) 1999-2010,2015,2017,2018 Jack Lloyd
*
* Based on the public domain reference implementation by Paulo Baretto
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <botan/aes.h>
#include <botan/loadstor.h>
#include <botan/cpuid.h>
#include <botan/rotate.h>
#include <type_traits>

/*


*/

namespace Botan {

namespace {

alignas(64)
const uint8_t SE[256] = {
   0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B,
   0xFE, 0xD7, 0xAB, 0x76, 0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0,
   0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0, 0xB7, 0xFD, 0x93, 0x26,
   0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
   0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2,
   0xEB, 0x27, 0xB2, 0x75, 0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0,
   0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84, 0x53, 0xD1, 0x00, 0xED,
   0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
   0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F,
   0x50, 0x3C, 0x9F, 0xA8, 0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5,
   0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2, 0xCD, 0x0C, 0x13, 0xEC,
   0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
   0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14,
   0xDE, 0x5E, 0x0B, 0xDB, 0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C,
   0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79, 0xE7, 0xC8, 0x37, 0x6D,
   0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
   0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F,
   0x4B, 0xBD, 0x8B, 0x8A, 0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E,
   0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E, 0xE1, 0xF8, 0x98, 0x11,
   0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
   0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F,
   0xB0, 0x54, 0xBB, 0x16 };

alignas(64)
const uint8_t SD[256] = {
   0x52, 0x09, 0x6A, 0xD5, 0x30, 0x36, 0xA5, 0x38, 0xBF, 0x40, 0xA3, 0x9E,
   0x81, 0xF3, 0xD7, 0xFB, 0x7C, 0xE3, 0x39, 0x82, 0x9B, 0x2F, 0xFF, 0x87,
   0x34, 0x8E, 0x43, 0x44, 0xC4, 0xDE, 0xE9, 0xCB, 0x54, 0x7B, 0x94, 0x32,
   0xA6, 0xC2, 0x23, 0x3D, 0xEE, 0x4C, 0x95, 0x0B, 0x42, 0xFA, 0xC3, 0x4E,
   0x08, 0x2E, 0xA1, 0x66, 0x28, 0xD9, 0x24, 0xB2, 0x76, 0x5B, 0xA2, 0x49,
   0x6D, 0x8B, 0xD1, 0x25, 0x72, 0xF8, 0xF6, 0x64, 0x86, 0x68, 0x98, 0x16,
   0xD4, 0xA4, 0x5C, 0xCC, 0x5D, 0x65, 0xB6, 0x92, 0x6C, 0x70, 0x48, 0x50,
   0xFD, 0xED, 0xB9, 0xDA, 0x5E, 0x15, 0x46, 0x57, 0xA7, 0x8D, 0x9D, 0x84,
   0x90, 0xD8, 0xAB, 0x00, 0x8C, 0xBC, 0xD3, 0x0A, 0xF7, 0xE4, 0x58, 0x05,
   0xB8, 0xB3, 0x45, 0x06, 0xD0, 0x2C, 0x1E, 0x8F, 0xCA, 0x3F, 0x0F, 0x02,
   0xC1, 0xAF, 0xBD, 0x03, 0x01, 0x13, 0x8A, 0x6B, 0x3A, 0x91, 0x11, 0x41,
   0x4F, 0x67, 0xDC, 0xEA, 0x97, 0xF2, 0xCF, 0xCE, 0xF0, 0xB4, 0xE6, 0x73,
   0x96, 0xAC, 0x74, 0x22, 0xE7, 0xAD, 0x35, 0x85, 0xE2, 0xF9, 0x37, 0xE8,
   0x1C, 0x75, 0xDF, 0x6E, 0x47, 0xF1, 0x1A, 0x71, 0x1D, 0x29, 0xC5, 0x89,
   0x6F, 0xB7, 0x62, 0x0E, 0xAA, 0x18, 0xBE, 0x1B, 0xFC, 0x56, 0x3E, 0x4B,
   0xC6, 0xD2, 0x79, 0x20, 0x9A, 0xDB, 0xC0, 0xFE, 0x78, 0xCD, 0x5A, 0xF4,
   0x1F, 0xDD, 0xA8, 0x33, 0x88, 0x07, 0xC7, 0x31, 0xB1, 0x12, 0x10, 0x59,
   0x27, 0x80, 0xEC, 0x5F, 0x60, 0x51, 0x7F, 0xA9, 0x19, 0xB5, 0x4A, 0x0D,
   0x2D, 0xE5, 0x7A, 0x9F, 0x93, 0xC9, 0x9C, 0xEF, 0xA0, 0xE0, 0x3B, 0x4D,
   0xAE, 0x2A, 0xF5, 0xB0, 0xC8, 0xEB, 0xBB, 0x3C, 0x83, 0x53, 0x99, 0x61,
   0x17, 0x2B, 0x04, 0x7E, 0xBA, 0x77, 0xD6, 0x26, 0xE1, 0x69, 0x14, 0x63,
   0x55, 0x21, 0x0C, 0x7D };

inline constexpr uint8_t xtime(uint8_t s) { return static_cast<uint8_t>(s << 1) ^ ((s >> 7) * 0x1B); }
inline constexpr uint8_t xtime4(uint8_t s) { return xtime(xtime(s)); }
inline constexpr uint8_t xtime8(uint8_t s) { return xtime(xtime(xtime(s))); }

inline constexpr uint8_t xtime3(uint8_t s) { return xtime(s) ^ s; }
inline constexpr uint8_t xtime9(uint8_t s) { return xtime8(s) ^ s; }
inline constexpr uint8_t xtime11(uint8_t s) { return xtime8(s) ^ xtime(s) ^ s; }
inline constexpr uint8_t xtime13(uint8_t s) { return xtime8(s) ^ xtime4(s) ^ s; }
inline constexpr uint8_t xtime14(uint8_t s) { return xtime8(s) ^ xtime4(s) ^ xtime(s); }

inline void AES_SBOX(word V[8])
   {
   const word I0 = V[0];
   const word I1 = V[1];
   const word I2 = V[2];
   const word I3 = V[3];
   const word I4 = V[4];
   const word I5 = V[5];
   const word I6 = V[6];
   const word I7 = V[7];

   /*
   Circuit for AES S-box from https://eprint.iacr.org/2011/332.pdf
   */
   // Figure 5:  Top linear transform in forward direction.
   const word T1 = I0 ^ I3;
   const word T2 = I0 ^ I5;
   const word T3 = I0 ^ I6;
   const word T4 = I3 ^ I5;
   const word T5 = I4 ^ I6;
   const word T6 = T1 ^ T5;
   const word T7 = I1 ^ I2;

   const word T8 = I7 ^ T6;
   const word T9 = I7 ^ T7;
   const word T10 = T6 ^ T7;
   const word T11 = I1 ^ I5;
   const word T12 = I2 ^ I5;
   const word T13 = T3 ^ T4;
   const word T14 = T6 ^ T11;

   const word T15 = T5 ^ T11;
   const word T16 = T5 ^ T12;
   const word T17 = T9 ^ T16;
   const word T18 = I3 ^ I7;
   const word T19 = T7 ^ T18;
   const word T20 = T1 ^ T19;
   const word T21 = I6 ^ I7;

   const word T22 = T7 ^ T21;
   const word T23 = T2 ^ T22;
   const word T24 = T2 ^ T10;
   const word T25 = T20 ^ T17;
   const word T26 = T3 ^ T16;
   const word T27 = T1 ^ T12;

   const word D = I7;

   // Figure 7:  Shared part of AES S-box circuit
   const word M1 = T13 & T6;
   const word M2 = T23 & T8;
   const word M3 = T14 ^ M1;
   const word M4 = T19 & D;
   const word M5 = M4 ^ M1;
   const word M6 = T3 & T16;
   const word M7 = T22 & T9;
   const word M8 = T26 ^ M6;
   const word M9 = T20 & T17;
   const word M10 = M9 ^ M6;
   const word M11 = T1 & T15;
   const word M12 = T4 & T27;
   const word M13 = M12 ^ M11;
   const word M14 = T2 & T10;
   const word M15 = M14 ^ M11;
   const word M16 = M3 ^ M2;

   const word M17 = M5 ^ T24;
   const word M18 = M8 ^ M7;
   const word M19 = M10 ^ M15;
   const word M20 = M16 ^ M13;
   const word M21 = M17 ^ M15;
   const word M22 = M18 ^ M13;
   const word M23 = M19 ^ T25;
   const word M24 = M22 ^ M23;
   const word M25 = M22 & M20;
   const word M26 = M21 ^ M25;
   const word M27 = M20 ^ M21;
   const word M28 = M23 ^ M25;
   const word M29 = M28 & M27;
   const word M30 = M26 & M24;
   const word M31 = M20 & M23;
   const word M32 = M27 & M31;

   const word M33 = M27 ^ M25;
   const word M34 = M21 & M22;
   const word M35 = M24 & M34;
   const word M36 = M24 ^ M25;
   const word M37 = M21 ^ M29;
   const word M38 = M32 ^ M33;
   const word M39 = M23 ^ M30;
   const word M40 = M35 ^ M36;
   const word M41 = M38 ^ M40;
   const word M42 = M37 ^ M39;
   const word M43 = M37 ^ M38;
   const word M44 = M39 ^ M40;
   const word M45 = M42 ^ M41;
   const word M46 = M44 & T6;
   const word M47 = M40 & T8;
   const word M48 = M39 & D;

   const word M49 = M43 & T16;
   const word M50 = M38 & T9;
   const word M51 = M37 & T17;
   const word M52 = M42 & T15;
   const word M53 = M45 & T27;
   const word M54 = M41 & T10;
   const word M55 = M44 & T13;
   const word M56 = M40 & T23;
   const word M57 = M39 & T19;
   const word M58 = M43 & T3;
   const word M59 = M38 & T22;
   const word M60 = M37 & T20;
   const word M61 = M42 & T1;
   const word M62 = M45 & T4;
   const word M63 = M41 & T2;

      // Figure 8:  Bottom linear transform in forward direction.
   const word L0 = M61 ^ M62;
   const word L1 = M50 ^ M56;
   const word L2 = M46 ^ M48;
   const word L3 = M47 ^ M55;
   const word L4 = M54 ^ M58;
   const word L5 = M49 ^ M61;
   const word L6 = M62 ^ L5;
   const word L7 = M46 ^ L3;
   const word L8 = M51 ^ M59;
   const word L9 = M52 ^ M53;
   const word L10 = M53 ^ L4;
   const word L11 = M60 ^ L2;
   const word L12 = M48 ^ M51;
   const word L13 = M50 ^ L0;
   const word L14 = M52 ^ M61;
   const word L15 = M55 ^ L1;
   const word L16 = M56 ^ L0;
   const word L17 = M57 ^ L1;
   const word L18 = M58 ^ L8;
   const word L19 = M63 ^ L4;

   const word L20 = L0 ^ L1;
   const word L21 = L1 ^ L7;
   const word L22 = L3 ^ L12;
   const word L23 = L18 ^ L2;
   const word L24 = L15 ^ L9;
   const word L25 = L6 ^ L10;
   const word L26 = L7 ^ L9;
   const word L27 = L8 ^ L10;
   const word L28 = L11 ^ L14;
   const word L29 = L11 ^ L17;

   const word S0 = L6 ^ L24;
   const word S1 = ~(L16 ^ L26);
   const word S2 = ~(L19 ^ L28);
   const word S3 = L6 ^ L21;
   const word S4 = L20 ^ L22;
   const word S5 = L25 ^ L29;
   const word S6 = ~(L13 ^ L27);
   const word S7 = ~(L6 ^ L23);

   V[0] = S0;
   V[1] = S1;
   V[2] = S2;
   V[3] = S3;
   V[4] = S4;
   V[5] = S5;
   V[6] = S6;
   V[7] = S7;
   }

inline uint32_t bit_permute_step(uint32_t x, uint32_t m, size_t shift)
   {
   /*
   See https://reflectionsonsecurity.wordpress.com/2014/05/11/efficient-bit-permutation-using-delta-swaps/
   and http://programming.sirrida.de/bit_perm.html
   */
   const uint32_t t = ((x >> shift) ^ x) & m;
   return (x ^ t) ^ (t << shift);
   }

inline uint32_t SE_word_ref(uint32_t x)
   {
   return make_uint32(SE[get_byte(0, x)],
                      SE[get_byte(1, x)],
                      SE[get_byte(2, x)],
                      SE[get_byte(3, x)]);
   }

inline uint32_t SE_word(uint32_t x)
   {
   //const uint32_t ref = SE_word_ref(x);
   //const uint32_t input = x;

   word I[8] = { 0 };

   // 0 8 16 24 1 9 17 25 2 10 18 26 3 11 19 27 4 12 20 28 5 13 21 29 6 14 22 30 7 15 23 31
   x = bit_permute_step(x, 0x00aa00aa, 7);  // Bit index swap 0,3
   x = bit_permute_step(x, 0x0000cccc, 14); // Bit index swap 1,4
   x = bit_permute_step(x, 0x00f000f0, 4);  // Bit index swap 2,3
   x = bit_permute_step(x, 0x0000ff00, 8);  // Bit index swap 3,4

   for(size_t i = 0; i != 8; ++i)
      I[i] = (x >> (28-4*i)) & 0xF;

   AES_SBOX(I);

   x = 0;

   for(size_t i = 0; i != 8; ++i)
      x = (x << 4) + (I[i] & 0xF);

   // 0 4 8 12 16 20 24 28 1 5 9 13 17 21 25 29 2 6 10 14 18 22 26 30 3 7 11 15 19 23 27 31
   x = bit_permute_step(x, 0x0a0a0a0a, 3);  // Bit index swap 0,2
   x = bit_permute_step(x, 0x00cc00cc, 6);  // Bit index swap 1,3
   x = bit_permute_step(x, 0x0000f0f0, 12);  // Bit index swap 2,4
   x = bit_permute_step(x, 0x0000ff00, 8);  // Bit index swap 3,4

   /*
if(x != ref)
      {
      printf("SE_word %08X ref=%08X ct=%08X\n", input, ref, x);
      std::exit(1);
      }
   */

   return x;
   }

void SE_word_x4(uint32_t& B0, uint32_t& B1, uint32_t& B2, uint32_t& B3)
   {
   word I[8] = { 0 };

   // 0_0 0_8 0_16 0_24 1_0 1_8 1_16 1_24 2_0 2_8 2_16 2_24 3_0 3_8 3_16 3_24 ...
   // 0_1 0_9 0_17 0_25 1_1 1_9 1_17 0_25 
   // 0_2 0_10 0_18 0_26 1_1 1_9 1_17 0_25 
   // 0_3 0_11 0_19 0_27 1_1 1_9 1_17 0_25 
   // 0_4 0_12 0_20 0_28 1_1 1_9 1_17 0_25 
   // 0_5 0_13 0_21 0_29 1_1 1_9 1_17 0_25 
   // 0_6 0_14 0_22 0_30 1_1 1_9 1_17 0_25 
   // 0_7 0_15 0_23 0_31 1_1 1_9 1_17 0_25 

   // FIXME specialize this:
   for(size_t i = 0; i != 8*sizeof(uint32_t); i += 8)
      {
      I[0] = (I[0] << 1) | ((B0 >> (7+i)) & 1);
      I[1] = (I[1] << 1) | ((B0 >> (6+i)) & 1);
      I[2] = (I[2] << 1) | ((B0 >> (5+i)) & 1);
      I[3] = (I[3] << 1) | ((B0 >> (4+i)) & 1);
      I[4] = (I[4] << 1) | ((B0 >> (3+i)) & 1);
      I[5] = (I[5] << 1) | ((B0 >> (2+i)) & 1);
      I[6] = (I[6] << 1) | ((B0 >> (1+i)) & 1);
      I[7] = (I[7] << 1) | ((B0 >> (0+i)) & 1);
      }
   for(size_t i = 0; i != 8*sizeof(uint32_t); i += 8)
      {
      I[0] = (I[0] << 1) | ((B1 >> (7+i)) & 1);
      I[1] = (I[1] << 1) | ((B1 >> (6+i)) & 1);
      I[2] = (I[2] << 1) | ((B1 >> (5+i)) & 1);
      I[3] = (I[3] << 1) | ((B1 >> (4+i)) & 1);
      I[4] = (I[4] << 1) | ((B1 >> (3+i)) & 1);
      I[5] = (I[5] << 1) | ((B1 >> (2+i)) & 1);
      I[6] = (I[6] << 1) | ((B1 >> (1+i)) & 1);
      I[7] = (I[7] << 1) | ((B1 >> (0+i)) & 1);
      }
   for(size_t i = 0; i != 8*sizeof(uint32_t); i += 8)
      {
      I[0] = (I[0] << 1) | ((B2 >> (7+i)) & 1);
      I[1] = (I[1] << 1) | ((B2 >> (6+i)) & 1);
      I[2] = (I[2] << 1) | ((B2 >> (5+i)) & 1);
      I[3] = (I[3] << 1) | ((B2 >> (4+i)) & 1);
      I[4] = (I[4] << 1) | ((B2 >> (3+i)) & 1);
      I[5] = (I[5] << 1) | ((B2 >> (2+i)) & 1);
      I[6] = (I[6] << 1) | ((B2 >> (1+i)) & 1);
      I[7] = (I[7] << 1) | ((B2 >> (0+i)) & 1);
      }
   for(size_t i = 0; i != 8*sizeof(uint32_t); i += 8)
      {
      I[0] = (I[0] << 1) | ((B3 >> (7+i)) & 1);
      I[1] = (I[1] << 1) | ((B3 >> (6+i)) & 1);
      I[2] = (I[2] << 1) | ((B3 >> (5+i)) & 1);
      I[3] = (I[3] << 1) | ((B3 >> (4+i)) & 1);
      I[4] = (I[4] << 1) | ((B3 >> (3+i)) & 1);
      I[5] = (I[5] << 1) | ((B3 >> (2+i)) & 1);
      I[6] = (I[6] << 1) | ((B3 >> (1+i)) & 1);
      I[7] = (I[7] << 1) | ((B3 >> (0+i)) & 1);
      }

   AES_SBOX(I);

   uint32_t Z0 = 0, Z1 = 0, Z2 = 0, Z3 = 0;

   // FIXME specialize this:
   for(size_t i = 0; i != sizeof(uint32_t); i += 1)
      {
      Z3 = (Z3 << 1) | ((I[0] >> i) & 1);
      Z3 = (Z3 << 1) | ((I[1] >> i) & 1);
      Z3 = (Z3 << 1) | ((I[2] >> i) & 1);
      Z3 = (Z3 << 1) | ((I[3] >> i) & 1);
      Z3 = (Z3 << 1) | ((I[4] >> i) & 1);
      Z3 = (Z3 << 1) | ((I[5] >> i) & 1);
      Z3 = (Z3 << 1) | ((I[6] >> i) & 1);
      Z3 = (Z3 << 1) | ((I[7] >> i) & 1);
      }
   for(size_t i = 0; i != sizeof(uint32_t); i += 1)
      {
      Z2 = (Z2 << 1) | ((I[0] >> (4+i)) & 1);
      Z2 = (Z2 << 1) | ((I[1] >> (4+i)) & 1);
      Z2 = (Z2 << 1) | ((I[2] >> (4+i)) & 1);
      Z2 = (Z2 << 1) | ((I[3] >> (4+i)) & 1);
      Z2 = (Z2 << 1) | ((I[4] >> (4+i)) & 1);
      Z2 = (Z2 << 1) | ((I[5] >> (4+i)) & 1);
      Z2 = (Z2 << 1) | ((I[6] >> (4+i)) & 1);
      Z2 = (Z2 << 1) | ((I[7] >> (4+i)) & 1);
      }
   for(size_t i = 0; i != sizeof(uint32_t); i += 1)
      {
      Z1 = (Z1 << 1) | ((I[0] >> (8+i)) & 1);
      Z1 = (Z1 << 1) | ((I[1] >> (8+i)) & 1);
      Z1 = (Z1 << 1) | ((I[2] >> (8+i)) & 1);
      Z1 = (Z1 << 1) | ((I[3] >> (8+i)) & 1);
      Z1 = (Z1 << 1) | ((I[4] >> (8+i)) & 1);
      Z1 = (Z1 << 1) | ((I[5] >> (8+i)) & 1);
      Z1 = (Z1 << 1) | ((I[6] >> (8+i)) & 1);
      Z1 = (Z1 << 1) | ((I[7] >> (8+i)) & 1);
      }
   for(size_t i = 0; i != sizeof(uint32_t); i += 1)
      {
      Z0 = (Z0 << 1) | ((I[0] >> (12+i)) & 1);
      Z0 = (Z0 << 1) | ((I[1] >> (12+i)) & 1);
      Z0 = (Z0 << 1) | ((I[2] >> (12+i)) & 1);
      Z0 = (Z0 << 1) | ((I[3] >> (12+i)) & 1);
      Z0 = (Z0 << 1) | ((I[4] >> (12+i)) & 1);
      Z0 = (Z0 << 1) | ((I[5] >> (12+i)) & 1);
      Z0 = (Z0 << 1) | ((I[6] >> (12+i)) & 1);
      Z0 = (Z0 << 1) | ((I[7] >> (12+i)) & 1);
      }

   B0 = Z0;
   B1 = Z1;
   B2 = Z2;
   B3 = Z3;
   }

void SE_word_x8(uint32_t& B0, uint32_t& B1, uint32_t& B2, uint32_t& B3,
                uint32_t& B4, uint32_t& B5, uint32_t& B6, uint32_t& B7)
   {
   word I[8] = { 0 };

   // 0 8 16 24 1 9 17 25 2 10 18 26 3 11 19 27 4 12 20 28 5 13 21 29 6 14 22 30 7 15 23 31

   // 0 .. 8 .. 16 .. 24 ..

   /*

   We are moving around nibbles in 32-bit words: 0 1 2 3 4 5 6 7.
   Starting position:

   B0: B0{0,1,2,3,4,5,6,7}
   B1: B1{0,1,2,3,4,5,6,7}
   B2: B2{0,1,2,3,4,5,6,7}
   B3: B3{0,1,2,3,4,5,6,7}
   B4: B4{0,1,2,3,4,5,6,7}
   B5: B5{0,1,2,3,4,5,6,7}
   B6: B6{0,1,2,3,4,5,6,7}
   B7: B7{0,1,2,3,4,5,6,7}

   End goal is:

   B0: B0{0},B1{0},B2{0},B3{0},B4{0},B5{0},B6{0},B7{0}
   ..
   B7: B0{7},B1{7},B2{7},B3{7},B4{7},B5{7},B6{7},B7{7}

   Or something like it, after which we can shuffle each word as
   needed...


   First move, unpack

   B0: B0{0,1,2,3},B1{0,1,2,3}
   B1: B0{4,5,6,7},B1{4,5,6,7}
   B2: B2{0,1,2,3},B3{0,1,2,3}
   B3: B2{4,5,6,7},B3{4,5,6,7}
   B4: B4{0,1,2,3},B5{0,1,2,3}
   B5: B4{4,5,6,7},B5{4,5,6,7}
   B6: B6{0,1,2,3},B7{0,1,2,3}
   B7: B6{4,5,6,7},B7{4,5,6,7}

   B0: B0{0,1,2,3},B1{0,1,2,3}
   B2: B2{0,1,2,3},B3{0,1,2,3}
=>
   B0: B0{0,1},B1{0,1},B2{0,1},B3{0,1}
   B4: B4{0,1},B5{0,1},B6{0,1},B7{0,1}

=>

   B0: B0{0},B1{0},B2{0},B3{0},B0{1},B1{1},B2{1},B3{1}
   B4: B4{0},B5{0},B6{0},B7{0},B4{1},B5{1},B6{1},B7{1}



   B0: B0{0..16},B1{0..16}
   B1: B0{16..32},B1{16..32}
   B2: B2{0..16},B3{0..16}
   B3: B2{16..32},B3{16..32}
=>
   B0: B0{0..16},B1{0..16}
   B1: B2{0..16},B3{0..16}
   B2: B0{16..32},B1{16..32}
   B3: B2{16..32},B3{16..32}

   B0: B0{0..8},B1{0..8},B2{0..8},B3{0..8}
   B1: B0{8..16},B1{8..16},B2{8..16},B3{8..16}
   B2: B0{16..32},B1{16..32}
   B3: B2{16..32},B3{16..32}

   =>

   B0: B0{0},B1{0},B2{0},B3{0}, ...

   */

   // B0{0} B1{0} B2{0} B3{0} B4{0} B5{0} B6{0} B7{0}
   // B0{1}
   // B0{2}
   // B0{3} B1{3} B2{3} ...                     B7{3}
   // new word:
   // B0{4}...
   // ...
   // B0{7} B1{7} B2{7}
   // new word:
   // B0{8}
   // new word:
   // B0{12}...
   // B0{31} B1{31} B2{31}

   // 0_0 0_8 0_16 0_24 1_0 1_8 1_16 1_24 2_0 2_8 2_16 2_24 3_0 3_8 3_16 3_24 4_0 4_8 4_16 3_24
   // 0_1 0_9 0_17 0_25 1_1 1_9 1_17 0_25 
   // 0_2 0_10 0_18 0_26 1_1 1_9 1_17 0_25 
   // 0_3 0_11 0_19 0_27 1_1 1_9 1_17 0_25 
   // 0_4 0_12 0_20 0_28 1_1 1_9 1_17 0_25 
   // 0_5 0_13 0_21 0_29 1_1 1_9 1_17 0_25 
   // 0_6 0_14 0_22 0_30 1_1 1_9 1_17 0_25 
   // 0_7 0_15 0_23 0_31 1_1 1_9 1_17 0_25 

   // FIXME specialize this:
   for(size_t i = 0; i != 8*sizeof(uint32_t); i += 8)
      {
      I[0] = (I[0] << 1) | ((B0 >> (7+i)) & 1);
      I[1] = (I[1] << 1) | ((B0 >> (6+i)) & 1);
      I[2] = (I[2] << 1) | ((B0 >> (5+i)) & 1);
      I[3] = (I[3] << 1) | ((B0 >> (4+i)) & 1);
      I[4] = (I[4] << 1) | ((B0 >> (3+i)) & 1);
      I[5] = (I[5] << 1) | ((B0 >> (2+i)) & 1);
      I[6] = (I[6] << 1) | ((B0 >> (1+i)) & 1);
      I[7] = (I[7] << 1) | ((B0 >> (0+i)) & 1);
      }
   for(size_t i = 0; i != 8*sizeof(uint32_t); i += 8)
      {
      I[0] = (I[0] << 1) | ((B1 >> (7+i)) & 1);
      I[1] = (I[1] << 1) | ((B1 >> (6+i)) & 1);
      I[2] = (I[2] << 1) | ((B1 >> (5+i)) & 1);
      I[3] = (I[3] << 1) | ((B1 >> (4+i)) & 1);
      I[4] = (I[4] << 1) | ((B1 >> (3+i)) & 1);
      I[5] = (I[5] << 1) | ((B1 >> (2+i)) & 1);
      I[6] = (I[6] << 1) | ((B1 >> (1+i)) & 1);
      I[7] = (I[7] << 1) | ((B1 >> (0+i)) & 1);
      }
   for(size_t i = 0; i != 8*sizeof(uint32_t); i += 8)
      {
      I[0] = (I[0] << 1) | ((B2 >> (7+i)) & 1);
      I[1] = (I[1] << 1) | ((B2 >> (6+i)) & 1);
      I[2] = (I[2] << 1) | ((B2 >> (5+i)) & 1);
      I[3] = (I[3] << 1) | ((B2 >> (4+i)) & 1);
      I[4] = (I[4] << 1) | ((B2 >> (3+i)) & 1);
      I[5] = (I[5] << 1) | ((B2 >> (2+i)) & 1);
      I[6] = (I[6] << 1) | ((B2 >> (1+i)) & 1);
      I[7] = (I[7] << 1) | ((B2 >> (0+i)) & 1);
      }
   for(size_t i = 0; i != 8*sizeof(uint32_t); i += 8)
      {
      I[0] = (I[0] << 1) | ((B3 >> (7+i)) & 1);
      I[1] = (I[1] << 1) | ((B3 >> (6+i)) & 1);
      I[2] = (I[2] << 1) | ((B3 >> (5+i)) & 1);
      I[3] = (I[3] << 1) | ((B3 >> (4+i)) & 1);
      I[4] = (I[4] << 1) | ((B3 >> (3+i)) & 1);
      I[5] = (I[5] << 1) | ((B3 >> (2+i)) & 1);
      I[6] = (I[6] << 1) | ((B3 >> (1+i)) & 1);
      I[7] = (I[7] << 1) | ((B3 >> (0+i)) & 1);
      }
   for(size_t i = 0; i != 8*sizeof(uint32_t); i += 8)
      {
      I[0] = (I[0] << 1) | ((B4 >> (7+i)) & 1);
      I[1] = (I[1] << 1) | ((B4 >> (6+i)) & 1);
      I[2] = (I[2] << 1) | ((B4 >> (5+i)) & 1);
      I[3] = (I[3] << 1) | ((B4 >> (4+i)) & 1);
      I[4] = (I[4] << 1) | ((B4 >> (3+i)) & 1);
      I[5] = (I[5] << 1) | ((B4 >> (2+i)) & 1);
      I[6] = (I[6] << 1) | ((B4 >> (1+i)) & 1);
      I[7] = (I[7] << 1) | ((B4 >> (0+i)) & 1);
      }
   for(size_t i = 0; i != 8*sizeof(uint32_t); i += 8)
      {
      I[0] = (I[0] << 1) | ((B5 >> (7+i)) & 1);
      I[1] = (I[1] << 1) | ((B5 >> (6+i)) & 1);
      I[2] = (I[2] << 1) | ((B5 >> (5+i)) & 1);
      I[3] = (I[3] << 1) | ((B5 >> (4+i)) & 1);
      I[4] = (I[4] << 1) | ((B5 >> (3+i)) & 1);
      I[5] = (I[5] << 1) | ((B5 >> (2+i)) & 1);
      I[6] = (I[6] << 1) | ((B5 >> (1+i)) & 1);
      I[7] = (I[7] << 1) | ((B5 >> (0+i)) & 1);
      }
   for(size_t i = 0; i != 8*sizeof(uint32_t); i += 8)
      {
      I[0] = (I[0] << 1) | ((B6 >> (7+i)) & 1);
      I[1] = (I[1] << 1) | ((B6 >> (6+i)) & 1);
      I[2] = (I[2] << 1) | ((B6 >> (5+i)) & 1);
      I[3] = (I[3] << 1) | ((B6 >> (4+i)) & 1);
      I[4] = (I[4] << 1) | ((B6 >> (3+i)) & 1);
      I[5] = (I[5] << 1) | ((B6 >> (2+i)) & 1);
      I[6] = (I[6] << 1) | ((B6 >> (1+i)) & 1);
      I[7] = (I[7] << 1) | ((B6 >> (0+i)) & 1);
      }
   for(size_t i = 0; i != 8*sizeof(uint32_t); i += 8)
      {
      I[0] = (I[0] << 1) | ((B7 >> (7+i)) & 1);
      I[1] = (I[1] << 1) | ((B7 >> (6+i)) & 1);
      I[2] = (I[2] << 1) | ((B7 >> (5+i)) & 1);
      I[3] = (I[3] << 1) | ((B7 >> (4+i)) & 1);
      I[4] = (I[4] << 1) | ((B7 >> (3+i)) & 1);
      I[5] = (I[5] << 1) | ((B7 >> (2+i)) & 1);
      I[6] = (I[6] << 1) | ((B7 >> (1+i)) & 1);
      I[7] = (I[7] << 1) | ((B7 >> (0+i)) & 1);
      }

   AES_SBOX(I);

   uint32_t Z0 = 0, Z1 = 0, Z2 = 0, Z3 = 0;
   uint32_t Z4 = 0, Z5 = 0, Z6 = 0, Z7 = 0;

   // FIXME specialize this:
   for(size_t i = 0; i != sizeof(uint32_t); i += 1)
      {
      Z7 = (Z7 << 1) | ((I[0] >> i) & 1);
      Z7 = (Z7 << 1) | ((I[1] >> i) & 1);
      Z7 = (Z7 << 1) | ((I[2] >> i) & 1);
      Z7 = (Z7 << 1) | ((I[3] >> i) & 1);
      Z7 = (Z7 << 1) | ((I[4] >> i) & 1);
      Z7 = (Z7 << 1) | ((I[5] >> i) & 1);
      Z7 = (Z7 << 1) | ((I[6] >> i) & 1);
      Z7 = (Z7 << 1) | ((I[7] >> i) & 1);
      }
   for(size_t i = 0; i != sizeof(uint32_t); i += 1)
      {
      Z6 = (Z6 << 1) | ((I[0] >> (4+i)) & 1);
      Z6 = (Z6 << 1) | ((I[1] >> (4+i)) & 1);
      Z6 = (Z6 << 1) | ((I[2] >> (4+i)) & 1);
      Z6 = (Z6 << 1) | ((I[3] >> (4+i)) & 1);
      Z6 = (Z6 << 1) | ((I[4] >> (4+i)) & 1);
      Z6 = (Z6 << 1) | ((I[5] >> (4+i)) & 1);
      Z6 = (Z6 << 1) | ((I[6] >> (4+i)) & 1);
      Z6 = (Z6 << 1) | ((I[7] >> (4+i)) & 1);
      }
   for(size_t i = 0; i != sizeof(uint32_t); i += 1)
      {
      Z5 = (Z5 << 1) | ((I[0] >> (8+i)) & 1);
      Z5 = (Z5 << 1) | ((I[1] >> (8+i)) & 1);
      Z5 = (Z5 << 1) | ((I[2] >> (8+i)) & 1);
      Z5 = (Z5 << 1) | ((I[3] >> (8+i)) & 1);
      Z5 = (Z5 << 1) | ((I[4] >> (8+i)) & 1);
      Z5 = (Z5 << 1) | ((I[5] >> (8+i)) & 1);
      Z5 = (Z5 << 1) | ((I[6] >> (8+i)) & 1);
      Z5 = (Z5 << 1) | ((I[7] >> (8+i)) & 1);
      }
   for(size_t i = 0; i != sizeof(uint32_t); i += 1)
      {
      Z4 = (Z4 << 1) | ((I[0] >> (12+i)) & 1);
      Z4 = (Z4 << 1) | ((I[1] >> (12+i)) & 1);
      Z4 = (Z4 << 1) | ((I[2] >> (12+i)) & 1);
      Z4 = (Z4 << 1) | ((I[3] >> (12+i)) & 1);
      Z4 = (Z4 << 1) | ((I[4] >> (12+i)) & 1);
      Z4 = (Z4 << 1) | ((I[5] >> (12+i)) & 1);
      Z4 = (Z4 << 1) | ((I[6] >> (12+i)) & 1);
      Z4 = (Z4 << 1) | ((I[7] >> (12+i)) & 1);
      }
   for(size_t i = 0; i != sizeof(uint32_t); i += 1)
      {
      Z3 = (Z3 << 1) | ((I[0] >> (16+i)) & 1);
      Z3 = (Z3 << 1) | ((I[1] >> (16+i)) & 1);
      Z3 = (Z3 << 1) | ((I[2] >> (16+i)) & 1);
      Z3 = (Z3 << 1) | ((I[3] >> (16+i)) & 1);
      Z3 = (Z3 << 1) | ((I[4] >> (16+i)) & 1);
      Z3 = (Z3 << 1) | ((I[5] >> (16+i)) & 1);
      Z3 = (Z3 << 1) | ((I[6] >> (16+i)) & 1);
      Z3 = (Z3 << 1) | ((I[7] >> (16+i)) & 1);
      }
   for(size_t i = 0; i != sizeof(uint32_t); i += 1)
      {
      Z2 = (Z2 << 1) | ((I[0] >> (4+16+i)) & 1);
      Z2 = (Z2 << 1) | ((I[1] >> (4+16+i)) & 1);
      Z2 = (Z2 << 1) | ((I[2] >> (4+16+i)) & 1);
      Z2 = (Z2 << 1) | ((I[3] >> (4+16+i)) & 1);
      Z2 = (Z2 << 1) | ((I[4] >> (4+16+i)) & 1);
      Z2 = (Z2 << 1) | ((I[5] >> (4+16+i)) & 1);
      Z2 = (Z2 << 1) | ((I[6] >> (4+16+i)) & 1);
      Z2 = (Z2 << 1) | ((I[7] >> (4+16+i)) & 1);
      }
   for(size_t i = 0; i != sizeof(uint32_t); i += 1)
      {
      Z1 = (Z1 << 1) | ((I[0] >> (8+16+i)) & 1);
      Z1 = (Z1 << 1) | ((I[1] >> (8+16+i)) & 1);
      Z1 = (Z1 << 1) | ((I[2] >> (8+16+i)) & 1);
      Z1 = (Z1 << 1) | ((I[3] >> (8+16+i)) & 1);
      Z1 = (Z1 << 1) | ((I[4] >> (8+16+i)) & 1);
      Z1 = (Z1 << 1) | ((I[5] >> (8+16+i)) & 1);
      Z1 = (Z1 << 1) | ((I[6] >> (8+16+i)) & 1);
      Z1 = (Z1 << 1) | ((I[7] >> (8+16+i)) & 1);
      }
   for(size_t i = 0; i != sizeof(uint32_t); i += 1)
      {
      Z0 = (Z0 << 1) | ((I[0] >> (12+16+i)) & 1);
      Z0 = (Z0 << 1) | ((I[1] >> (12+16+i)) & 1);
      Z0 = (Z0 << 1) | ((I[2] >> (12+16+i)) & 1);
      Z0 = (Z0 << 1) | ((I[3] >> (12+16+i)) & 1);
      Z0 = (Z0 << 1) | ((I[4] >> (12+16+i)) & 1);
      Z0 = (Z0 << 1) | ((I[5] >> (12+16+i)) & 1);
      Z0 = (Z0 << 1) | ((I[6] >> (12+16+i)) & 1);
      Z0 = (Z0 << 1) | ((I[7] >> (12+16+i)) & 1);
      }

   B0 = Z0;
   B1 = Z1;
   B2 = Z2;
   B3 = Z3;
   B4 = Z4;
   B5 = Z5;
   B6 = Z6;
   B7 = Z7;
   }

/*
 This matrix decomposition was credited to Jussi Kivilinna in
 OpenSSL's bsaes. Notice that the first component is equal to the
 MixColumn matrix.

 | 0E 0B 0D 09 |   | 02 03 01 01 |   | 05 00 04 00 |
 | 09 0E 0B 0D | = | 01 02 03 01 | X | 00 05 00 04 |
 | 0D 09 0E 0B |   | 01 01 02 03 |   | 04 00 05 00 |
 | 0B 0D 09 0E |   | 03 01 01 02 |   | 00 04 00 05 |
*/

inline uint32_t xtime_32(uint32_t s)
   {
   const uint32_t hb = ((s >> 7) & 0x01010101);
   const uint32_t shifted = (s << 1) & 0xFEFEFEFE;

   //uint32_t carry = hb | (hb << 1);
   //carry |= (carry << 3);
   const uint32_t carry = (hb << 4) | (hb << 3) | (hb << 1) | hb; // 0x1B
   return (shifted ^ carry);
   }

inline uint32_t aes_enc_round(uint32_t V0, uint32_t V1, uint32_t V2, uint32_t V3)
   {
   const uint32_t s = make_uint32(get_byte(0, V0),
                                  get_byte(1, V1),
                                  get_byte(2, V2),
                                  get_byte(3, V3));

   const uint32_t xtime_s = xtime_32(s);
   const uint32_t xtime3_s = xtime_s ^ s;

   const uint32_t Z0 = make_uint32(get_byte(0, xtime_s),  get_byte(0, V0),       get_byte(0, V0),        get_byte(0, xtime3_s));
   const uint32_t Z1 = make_uint32(get_byte(1, xtime3_s), get_byte(1, xtime_s),  get_byte(1, V1),        get_byte(1, V1));
   const uint32_t Z2 = make_uint32(get_byte(2, V2),       get_byte(2, xtime3_s), get_byte(2, xtime_s),   get_byte(2, V2));
   const uint32_t Z3 = make_uint32(get_byte(3, V3),       get_byte(3, V3),       get_byte(3, xtime3_s),  get_byte(3, xtime_s));

   return (Z0 ^ Z1 ^ Z2 ^ Z3);
   }

inline void aes_enc_r(uint32_t& B0, uint32_t& B1, uint32_t& B2, uint32_t& B3,
                      uint32_t K0, uint32_t K1, uint32_t K2, uint32_t K3)
   {
   uint32_t S0 = B0, S1 = B1, S2 = B2, S3 = B3;
   SE_word_x4(S0, S1, S2, S3);

   const uint32_t T0 = aes_enc_round(S0, S1, S2, S3);
   const uint32_t T1 = aes_enc_round(S1, S2, S3, S0);
   const uint32_t T2 = aes_enc_round(S2, S3, S0, S1);
   const uint32_t T3 = aes_enc_round(S3, S0, S1, S2);

   B0 = T0 ^ K0;
   B1 = T1 ^ K1;
   B2 = T2 ^ K2;
   B3 = T3 ^ K3;
   }

inline void aes_enc_r(uint32_t& B0, uint32_t& B1, uint32_t& B2, uint32_t& B3,
                      uint32_t& B4, uint32_t& B5, uint32_t& B6, uint32_t& B7,
                      uint32_t K0, uint32_t K1, uint32_t K2, uint32_t K3)
   {
   uint32_t S0 = B0, S1 = B1, S2 = B2, S3 = B3,
            S4 = B4, S5 = B5, S6 = B6, S7 = B7;
   SE_word_x8(S0, S1, S2, S3, S4, S5, S6, S7);

   const uint32_t T0 = aes_enc_round(S0, S1, S2, S3);
   const uint32_t T1 = aes_enc_round(S1, S2, S3, S0);
   const uint32_t T2 = aes_enc_round(S2, S3, S0, S1);
   const uint32_t T3 = aes_enc_round(S3, S0, S1, S2);
   const uint32_t T4 = aes_enc_round(S4, S5, S6, S7);
   const uint32_t T5 = aes_enc_round(S5, S6, S7, S4);
   const uint32_t T6 = aes_enc_round(S6, S7, S4, S5);
   const uint32_t T7 = aes_enc_round(S7, S4, S5, S6);

   B0 = T0 ^ K0;
   B1 = T1 ^ K1;
   B2 = T2 ^ K2;
   B3 = T3 ^ K3;
   B4 = T4 ^ K0;
   B5 = T5 ^ K1;
   B6 = T6 ^ K2;
   B7 = T7 ^ K3;
   }

const uint32_t* AES_TD()
   {
   class TD_Table final
      {
      public:
         TD_Table()
            {
            uint32_t* p = reinterpret_cast<uint32_t*>(&data);
            for(size_t i = 0; i != 256; ++i)
               {
               const uint8_t s = SD[i];
               p[i] = make_uint32(xtime14(s), xtime9(s), xtime13(s), xtime11(s));
               }
            }

         const uint32_t* ptr() const
            {
            return reinterpret_cast<const uint32_t*>(&data);
            }
      private:
         std::aligned_storage<256*sizeof(uint32_t), 64>::type data;
      };

   static TD_Table table;
   return table.ptr();
   }

#define AES_T(T, K, V0, V1, V2, V3)                                     \
   (K ^ T[get_byte(0, V0)] ^                                            \
    rotr< 8>(T[get_byte(1, V1)]) ^                                      \
    rotr<16>(T[get_byte(2, V2)]) ^                                      \
    rotr<24>(T[get_byte(3, V3)]))

/*
* AES Encryption
*/
void aes_encrypt_n(const uint8_t in[], uint8_t out[],
                   size_t blocks,
                   const secure_vector<uint32_t>& EK,
                   const secure_vector<uint8_t>& ME)
   {
   BOTAN_ASSERT(EK.size() && ME.size() == 16, "Key was set");

   while(blocks >= 2)
      {
      uint32_t B0, B1, B2, B3, B4, B5, B6, B7;
      load_be(in, B0, B1, B2, B3, B4, B5, B6, B7);

      B0 ^= EK[0];
      B1 ^= EK[1];
      B2 ^= EK[2];
      B3 ^= EK[3];
      B4 ^= EK[0];
      B5 ^= EK[1];
      B6 ^= EK[2];
      B7 ^= EK[3];

      // First round:
      aes_enc_r(B0, B1, B2, B3, B4, B5, B6, B7, EK[4], EK[5], EK[6], EK[7]);

      for(size_t r = 2*4; r < EK.size(); r += 2*4)
         {
         aes_enc_r(B0, B1, B2, B3, B4, B5, B6, B7, EK[r  ], EK[r+1], EK[r+2], EK[r+3]);
         aes_enc_r(B0, B1, B2, B3, B4, B5, B6, B7, EK[r+4], EK[r+5], EK[r+6], EK[r+7]);
         }

      // Final round:
      SE_word_x8(B0, B1, B2, B3, B4, B5, B6, B7);
      // FIXME ME should be words!!
      // XXX still need to do shiftrows here <---- !!
      // B0 ^= EK[...]; B1 ^= EK[...+1]; ...
      // store_be(out, B0, B1, B2, B3);

      out[ 0] = get_byte(0, B0) ^ ME[0];
      out[ 1] = get_byte(1, B1) ^ ME[1];
      out[ 2] = get_byte(2, B2) ^ ME[2];
      out[ 3] = get_byte(3, B3) ^ ME[3];
      out[ 4] = get_byte(0, B1) ^ ME[4];
      out[ 5] = get_byte(1, B2) ^ ME[5];
      out[ 6] = get_byte(2, B3) ^ ME[6];
      out[ 7] = get_byte(3, B0) ^ ME[7];
      out[ 8] = get_byte(0, B2) ^ ME[8];
      out[ 9] = get_byte(1, B3) ^ ME[9];
      out[10] = get_byte(2, B0) ^ ME[10];
      out[11] = get_byte(3, B1) ^ ME[11];
      out[12] = get_byte(0, B3) ^ ME[12];
      out[13] = get_byte(1, B0) ^ ME[13];
      out[14] = get_byte(2, B1) ^ ME[14];
      out[15] = get_byte(3, B2) ^ ME[15];
      out[16] = get_byte(0, B4) ^ ME[0];
      out[17] = get_byte(1, B5) ^ ME[1];
      out[18] = get_byte(2, B6) ^ ME[2];
      out[19] = get_byte(3, B7) ^ ME[3];
      out[20] = get_byte(0, B5) ^ ME[4];
      out[21] = get_byte(1, B6) ^ ME[5];
      out[22] = get_byte(2, B7) ^ ME[6];
      out[23] = get_byte(3, B4) ^ ME[7];
      out[24] = get_byte(0, B6) ^ ME[8];
      out[25] = get_byte(1, B7) ^ ME[9];
      out[26] = get_byte(2, B4) ^ ME[10];
      out[27] = get_byte(3, B5) ^ ME[11];
      out[28] = get_byte(0, B7) ^ ME[12];
      out[29] = get_byte(1, B4) ^ ME[13];
      out[30] = get_byte(2, B5) ^ ME[14];
      out[31] = get_byte(3, B6) ^ ME[15];

      in += 32;
      out += 32;
      blocks -= 2;
      }

   for(size_t i = 0; i < blocks; ++i)
      {
      uint32_t B0, B1, B2, B3;
      load_be(in + 16*i, B0, B1, B2, B3);

      B0 ^= EK[0];
      B1 ^= EK[1];
      B2 ^= EK[2];
      B3 ^= EK[3];

      // First round:
      aes_enc_r(B0, B1, B2, B3, EK[4], EK[5], EK[6], EK[7]);

      for(size_t r = 2*4; r < EK.size(); r += 2*4)
         {
         aes_enc_r(B0, B1, B2, B3, EK[r  ], EK[r+1], EK[r+2], EK[r+3]);
         aes_enc_r(B0, B1, B2, B3, EK[r+4], EK[r+5], EK[r+6], EK[r+7]);
         }

      // Final round:
      SE_word_x4(B0, B1, B2, B3);
      // FIXME ME should be words!!
      // XXX still need to do shiftrows here <---- !!
      // B0 ^= EK[...]; B1 ^= EK[...+1]; ...
      // store_be(out, B0, B1, B2, B3);

      out[16*i+ 0] = get_byte(0, B0) ^ ME[0];
      out[16*i+ 1] = get_byte(1, B1) ^ ME[1];
      out[16*i+ 2] = get_byte(2, B2) ^ ME[2];
      out[16*i+ 3] = get_byte(3, B3) ^ ME[3];
      out[16*i+ 4] = get_byte(0, B1) ^ ME[4];
      out[16*i+ 5] = get_byte(1, B2) ^ ME[5];
      out[16*i+ 6] = get_byte(2, B3) ^ ME[6];
      out[16*i+ 7] = get_byte(3, B0) ^ ME[7];
      out[16*i+ 8] = get_byte(0, B2) ^ ME[8];
      out[16*i+ 9] = get_byte(1, B3) ^ ME[9];
      out[16*i+10] = get_byte(2, B0) ^ ME[10];
      out[16*i+11] = get_byte(3, B1) ^ ME[11];
      out[16*i+12] = get_byte(0, B3) ^ ME[12];
      out[16*i+13] = get_byte(1, B0) ^ ME[13];
      out[16*i+14] = get_byte(2, B1) ^ ME[14];
      out[16*i+15] = get_byte(3, B2) ^ ME[15];
      }
   }

/*
* AES Decryption
*/
void aes_decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks,
                   const secure_vector<uint32_t>& DK,
                   const secure_vector<uint8_t>& MD)
   {
   BOTAN_ASSERT(DK.size() && MD.size() == 16, "Key was set");

   const size_t cache_line_size = CPUID::cache_line_size();
   const uint32_t* TD = AES_TD();

   volatile uint32_t Z = 0;
   for(size_t i = 0; i < 256; i += cache_line_size / sizeof(uint32_t))
      {
      Z |= TD[i];
      }
   for(size_t i = 0; i < 256; i += cache_line_size)
      {
      Z |= SD[i];
      }
   Z &= TD[99]; // this is zero, which hopefully the compiler cannot deduce

   for(size_t i = 0; i != blocks; ++i)
      {
      uint32_t T0 = load_be<uint32_t>(in, 0) ^ DK[0];
      uint32_t T1 = load_be<uint32_t>(in, 1) ^ DK[1];
      uint32_t T2 = load_be<uint32_t>(in, 2) ^ DK[2];
      uint32_t T3 = load_be<uint32_t>(in, 3) ^ DK[3];

      T0 ^= Z;

      uint32_t B0 = AES_T(TD, DK[4], T0, T3, T2, T1);
      uint32_t B1 = AES_T(TD, DK[5], T1, T0, T3, T2);
      uint32_t B2 = AES_T(TD, DK[6], T2, T1, T0, T3);
      uint32_t B3 = AES_T(TD, DK[7], T3, T2, T1, T0);

      for(size_t r = 2*4; r < DK.size(); r += 2*4)
         {
         T0 = AES_T(TD, DK[r  ], B0, B3, B2, B1);
         T1 = AES_T(TD, DK[r+1], B1, B0, B3, B2);
         T2 = AES_T(TD, DK[r+2], B2, B1, B0, B3);
         T3 = AES_T(TD, DK[r+3], B3, B2, B1, B0);

         B0 = AES_T(TD, DK[r+4], T0, T3, T2, T1);
         B1 = AES_T(TD, DK[r+5], T1, T0, T3, T2);
         B2 = AES_T(TD, DK[r+6], T2, T1, T0, T3);
         B3 = AES_T(TD, DK[r+7], T3, T2, T1, T0);
         }

      out[ 0] = SD[get_byte(0, B0)] ^ MD[0];
      out[ 1] = SD[get_byte(1, B3)] ^ MD[1];
      out[ 2] = SD[get_byte(2, B2)] ^ MD[2];
      out[ 3] = SD[get_byte(3, B1)] ^ MD[3];
      out[ 4] = SD[get_byte(0, B1)] ^ MD[4];
      out[ 5] = SD[get_byte(1, B0)] ^ MD[5];
      out[ 6] = SD[get_byte(2, B3)] ^ MD[6];
      out[ 7] = SD[get_byte(3, B2)] ^ MD[7];
      out[ 8] = SD[get_byte(0, B2)] ^ MD[8];
      out[ 9] = SD[get_byte(1, B1)] ^ MD[9];
      out[10] = SD[get_byte(2, B0)] ^ MD[10];
      out[11] = SD[get_byte(3, B3)] ^ MD[11];
      out[12] = SD[get_byte(0, B3)] ^ MD[12];
      out[13] = SD[get_byte(1, B2)] ^ MD[13];
      out[14] = SD[get_byte(2, B1)] ^ MD[14];
      out[15] = SD[get_byte(3, B0)] ^ MD[15];

      in += 16;
      out += 16;
      }
   }

void aes_key_schedule(const uint8_t key[], size_t length,
                      secure_vector<uint32_t>& EK,
                      secure_vector<uint32_t>& DK,
                      secure_vector<uint8_t>& ME,
                      secure_vector<uint8_t>& MD)
   {
   static const uint32_t RC[10] = {
      0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000,
      0x20000000, 0x40000000, 0x80000000, 0x1B000000, 0x36000000 };

   const size_t rounds = (length / 4) + 6;

   secure_vector<uint32_t> XEK(length + 32), XDK(length + 32);

   const size_t X = length / 4;

   // Can't happen, but make static analyzers happy
   BOTAN_ARG_CHECK(X == 4 || X == 6 || X == 8, "Invalid AES key size");

   const uint32_t* TD = AES_TD();

   // Prefetch TD and SE which are used later on in this function
   volatile uint32_t Z = 0;
   const size_t cache_line_size = CPUID::cache_line_size();

   for(size_t i = 0; i < 256; i += cache_line_size / sizeof(uint32_t))
      {
      Z |= TD[i];
      }
   Z &= TD[99]; // this is zero, which hopefully the compiler cannot deduce

   for(size_t i = 0; i != X; ++i)
      XEK[i] = Z ^ load_be<uint32_t>(key, i);

   for(size_t i = X; i < 4*(rounds+1); i += X)
      {
      XEK[i] = XEK[i-X] ^ RC[(i-X)/X] ^ SE_word(rotl<8>(XEK[i-1]));

      for(size_t j = 1; j != X; ++j)
         {
         XEK[i+j] = XEK[i+j-X];

         if(X == 8 && j == 4)
            XEK[i+j] ^= SE_word(XEK[i+j-1]);
         else
            XEK[i+j] ^= XEK[i+j-1];
         }
      }

   for(size_t i = 0; i != 4*(rounds+1); i += 4)
      {
      XDK[i  ] = XEK[4*rounds-i  ];
      XDK[i+1] = XEK[4*rounds-i+1];
      XDK[i+2] = XEK[4*rounds-i+2];
      XDK[i+3] = XEK[4*rounds-i+3];
      }

   /// XXX the SE is just to counteract the SD in the T-table
   /// all we actually want here is the InvMixColumns!!! So do that.

   for(size_t i = 4; i != length + 24; i += 4)
      {
      SE_word_x4(XDK[i], XDK[i+1], XDK[i+2], XDK[i+3]);
      }

   for(size_t i = 4; i != length + 24; ++i)
      {
      XDK[i] = AES_T(TD, 0, XDK[i], XDK[i], XDK[i], XDK[i]);
      }

   ME.resize(16);
   MD.resize(16);

   for(size_t i = 0; i != 4; ++i)
      {
      store_be(XEK[i+4*rounds], &ME[4*i]);
      store_be(XEK[i], &MD[4*i]);
      }

   EK.resize(length + 24);
   DK.resize(length + 24);
   copy_mem(EK.data(), XEK.data(), EK.size());
   copy_mem(DK.data(), XDK.data(), DK.size());

#if defined(BOTAN_HAS_AES_ARMV8)
   if(CPUID::has_arm_aes())
      {
      // ARM needs the subkeys to be byte reversed

      for(size_t i = 0; i != EK.size(); ++i)
         EK[i] = reverse_bytes(EK[i]);
      for(size_t i = 0; i != DK.size(); ++i)
         DK[i] = reverse_bytes(DK[i]);
      }
#endif

   }

#undef AES_T

size_t aes_parallelism()
   {
#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return 4;
      }
#endif

#if defined(BOTAN_HAS_AES_POWER8)
   if(CPUID::has_power_crypto())
      {
      return 4;
      }
#endif

#if defined(BOTAN_HAS_AES_ARMV8)
   if(CPUID::has_arm_aes())
      {
      return 4;
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return 2;
      }
#endif

   // bitsliced:
   return 4;
   //return sizeof(word);
   }

const char* aes_provider()
   {
#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return "aesni";
      }
#endif

#if defined(BOTAN_HAS_AES_POWER8)
   if(CPUID::has_power_crypto())
      {
      return "power8";
      }
#endif

#if defined(BOTAN_HAS_AES_ARMV8)
   if(CPUID::has_arm_aes())
      {
      return "armv8";
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return "vperm";
      }
#endif

   return "base";
   }

}

std::string AES_128::provider() const { return aes_provider(); }
std::string AES_192::provider() const { return aes_provider(); }
std::string AES_256::provider() const { return aes_provider(); }

size_t AES_128::parallelism() const { return aes_parallelism(); }
size_t AES_192::parallelism() const { return aes_parallelism(); }
size_t AES_256::parallelism() const { return aes_parallelism(); }

void AES_128::encrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_EK.empty() == false);

#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return aesni_encrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_ARMV8)
   if(CPUID::has_arm_aes())
      {
      return armv8_encrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_POWER8)
   if(CPUID::has_power_crypto())
      {
      return power8_encrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_encrypt_n(in, out, blocks);
      }
#endif

   aes_encrypt_n(in, out, blocks, m_EK, m_ME);
   }

void AES_128::decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_DK.empty() == false);

#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return aesni_decrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_ARMV8)
   if(CPUID::has_arm_aes())
      {
      return armv8_decrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_POWER8)
   if(CPUID::has_power_crypto())
      {
      return power8_decrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_decrypt_n(in, out, blocks);
      }
#endif

   aes_decrypt_n(in, out, blocks, m_DK, m_MD);
   }

void AES_128::key_schedule(const uint8_t key[], size_t length)
   {
#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return aesni_key_schedule(key, length);
      }
#endif

#if defined(BOTAN_HAS_AES_ARMV8)
   if(CPUID::has_arm_aes())
      {
      return aes_key_schedule(key, length, m_EK, m_DK, m_ME, m_MD);
      }
#endif

#if defined(BOTAN_HAS_AES_POWER8)
   if(CPUID::has_power_crypto())
      {
      return aes_key_schedule(key, length, m_EK, m_DK, m_ME, m_MD);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_key_schedule(key, length);
      }
#endif

   aes_key_schedule(key, length, m_EK, m_DK, m_ME, m_MD);
   }

void AES_128::clear()
   {
   zap(m_EK);
   zap(m_DK);
   zap(m_ME);
   zap(m_MD);
   }

void AES_192::encrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_EK.empty() == false);

#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return aesni_encrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_ARMV8)
   if(CPUID::has_arm_aes())
      {
      return armv8_encrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_POWER8)
   if(CPUID::has_power_crypto())
      {
      return power8_encrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_encrypt_n(in, out, blocks);
      }
#endif

   aes_encrypt_n(in, out, blocks, m_EK, m_ME);
   }

void AES_192::decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_DK.empty() == false);

#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return aesni_decrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_ARMV8)
   if(CPUID::has_arm_aes())
      {
      return armv8_decrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_POWER8)
   if(CPUID::has_power_crypto())
      {
      return power8_decrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_decrypt_n(in, out, blocks);
      }
#endif

   aes_decrypt_n(in, out, blocks, m_DK, m_MD);
   }

void AES_192::key_schedule(const uint8_t key[], size_t length)
   {
#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return aesni_key_schedule(key, length);
      }
#endif

#if defined(BOTAN_HAS_AES_ARMV8)
   if(CPUID::has_arm_aes())
      {
      return aes_key_schedule(key, length, m_EK, m_DK, m_ME, m_MD);
      }
#endif

#if defined(BOTAN_HAS_AES_POWER8)
   if(CPUID::has_power_crypto())
      {
      return aes_key_schedule(key, length, m_EK, m_DK, m_ME, m_MD);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_key_schedule(key, length);
      }
#endif

   aes_key_schedule(key, length, m_EK, m_DK, m_ME, m_MD);
   }

void AES_192::clear()
   {
   zap(m_EK);
   zap(m_DK);
   zap(m_ME);
   zap(m_MD);
   }

void AES_256::encrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_EK.empty() == false);

#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return aesni_encrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_ARMV8)
   if(CPUID::has_arm_aes())
      {
      return armv8_encrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_POWER8)
   if(CPUID::has_power_crypto())
      {
      return power8_encrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_encrypt_n(in, out, blocks);
      }
#endif

   aes_encrypt_n(in, out, blocks, m_EK, m_ME);
   }

void AES_256::decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_DK.empty() == false);

#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return aesni_decrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_ARMV8)
   if(CPUID::has_arm_aes())
      {
      return armv8_decrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_POWER8)
   if(CPUID::has_power_crypto())
      {
      return power8_decrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_decrypt_n(in, out, blocks);
      }
#endif

   aes_decrypt_n(in, out, blocks, m_DK, m_MD);
   }

void AES_256::key_schedule(const uint8_t key[], size_t length)
   {
#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return aesni_key_schedule(key, length);
      }
#endif

#if defined(BOTAN_HAS_AES_ARMV8)
   if(CPUID::has_arm_aes())
      {
      return aes_key_schedule(key, length, m_EK, m_DK, m_ME, m_MD);
      }
#endif

#if defined(BOTAN_HAS_AES_POWER8)
   if(CPUID::has_power_crypto())
      {
      return aes_key_schedule(key, length, m_EK, m_DK, m_ME, m_MD);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_key_schedule(key, length);
      }
#endif

   aes_key_schedule(key, length, m_EK, m_DK, m_ME, m_MD);
   }

void AES_256::clear()
   {
   zap(m_EK);
   zap(m_DK);
   zap(m_ME);
   zap(m_MD);
   }

}
