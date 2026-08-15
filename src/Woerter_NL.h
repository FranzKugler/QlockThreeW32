/**
 * Woerter_NL
 * Definition of the Dutch words for the spoken time.
 * The words are bit masks for the matrix.
 *
 * @mc       ESP32S3
 * @autor    Rudolf Klimesch (Vorlage: Christian Aschoff)
 * @version  2.0
 * @created  17.1.2013
 * @updated  15.8.2026
 * @update   29.9.2014
 *
 * Historie:
 * V 1.01 - Falsches O bei ZEVEN behoben.
 *
 */
#ifndef WOERTER_NL_H
#define WOERTER_NL_H

/*
 * TEMPLATE FOR THE MATRIX
 *
 *  H E T K I S A V I J F     HET=IT, IS=IS, VIJF=FIVE
 *  T I E N B T Z V O O R     TIEN=TEN, VOOR=TO
 *  O V E R M E K W A R T     OVER=PAST, KWART=QUARTER
 *  H A L F S P W O V E R     HALF=HALF, OVER=PAST
 *  V O O R T H G E E N S     VOOR=TO, EENS=ONE
 *  T W E E P V C D R I E     TWEE=TWO, DRIE=THREE
 *  V I E R V I J F Z E S     VIER=FOUR, VIJF=FIVE, ZES=SIX
 *  Z E V E N O N E G E N     ZEVEN=SEVEN, NEGEN=NINE
 *  A C H T T I E N E L F     ACHT=EIGHT, TIEN=TEN, ELF=ELEVEN
 *  T W A A L F B F U U R     TWAALF=TWELVE, UUR=OCLOCK
 */

/**
 * Definition of the words
 */
#define NL_VOOR         matrix[1] |= 0b0000000111100000 // VOR
#define NL_OVER         matrix[2] |= 0b1111000000000000 // NACH
#define NL_VOOR2        matrix[4] |= 0b1111000000000000 // VOR2
#define NL_OVER2        matrix[3] |= 0b0000000111100000 // NACH2
#define NL_HETIS        matrix[0] |= 0b1110110000000000 // ESIST
#define NL_UUR          matrix[9] |= 0b0000000011100000 // UHR

#define NL_VIJF         matrix[0] |= 0b0000000111100000 // FUENF
#define NL_TIEN         matrix[1] |= 0b1111000000000000 // ZEHN
#define NL_KWART        matrix[2] |= 0b0000001111100000 // VIERTEL
#define NL_ZWANZIG      matrix[1] |= 0b0000111111100000 // ZWANZIG
#define NL_HALF         matrix[3] |= 0b1111000000000000 // HALB

#define NL_H_EEN        matrix[4] |= 0b0000000111000000 // H_EIN
#define NL_H_EENS       matrix[4] |= 0b0000000111100000 // H_EINS
#define NL_H_TWEE       matrix[5] |= 0b1111000000000000 // H_ZWEI
#define NL_H_DRIE       matrix[5] |= 0b0000000111100000 // H_DREI
#define NL_H_VIER       matrix[6] |= 0b1111000000000000 // H_VIER
#define NL_H_VIJF       matrix[6] |= 0b0000111100000000 // H_FUENF
#define NL_H_ZES        matrix[6] |= 0b0000000011100000 // H_SECHS
#define NL_H_ZEVEN      matrix[7] |= 0b1111100000000000 // H_SIEBEN
#define NL_H_ACHT       matrix[8] |= 0b1111000000000000 // H_ACHT
#define NL_H_NEGEN      matrix[7] |= 0b0000001111100000 // H_NEUN
#define NL_H_TIEN       matrix[8] |= 0b0000111100000000 // H_ZEHN
#define NL_H_ELF        matrix[8] |= 0b0000000011100000 // H_ELF
#define NL_H_TWAALF     matrix[9] |= 0b1111110000000000 // H_ZWOELF

#endif
