/**
 * Woerter_DE
 * Definition of the German words for the spoken time.
 * The words are bit masks for the matrix.
 *
 * The panel, as the masks below address it. The bit written leftmost in each
 * mask is column 0, so DE_ESIST = 0b1101110000000000 lights columns 0, 1 and
 * 3, 4, 5 of row 0 - "ES" and "IST".
 *
 *   01234567890
 * 0 ESKISTAFÜNF
 * 1 ZEHNZWANZIG
 * 2 DREIVIERTEL
 * 3 VORFUNKNACH
 * 4 HALBAELFÜNF
 * 5 EINSXAMZWEI
 * 6 DREIAUJVIER
 * 7 SECHSNLACHT
 * 8 SIEBENZWÖLF
 * 9 ZEHNEUNKUHR
 *
 *
 * @mc       ESP32S3
 * @author   Christian Aschoff / caschoff _AT_ mac _DOT_ com
 * @version  2.0
 * @created  18.3.2012
 * @updated  15.8.2026
 */
#ifndef WOERTER_DE_H
#define WOERTER_DE_H

/**
 * Definition of the words
 */
#define DE_VOR          matrix[3] |= 0b1110000000000000
#define DE_NACH         matrix[3] |= 0b0000000111100000
#define DE_ESIST        matrix[0] |= 0b1101110000000000
#define DE_UHR          matrix[9] |= 0b0000000011100000

#define DE_FUENF        matrix[0] |= 0b0000000111100000
#define DE_ZEHN         matrix[1] |= 0b1111000000000000
#define DE_VIERTEL      matrix[2] |= 0b0000111111100000
#define DE_ZWANZIG      matrix[1] |= 0b0000111111100000
#define DE_HALB         matrix[4] |= 0b1111000000000000
#define DE_DREIVIERTEL  matrix[2] |= 0b1111111111100000

#define DE_H_EIN        matrix[5] |= 0b1110000000000000
#define DE_H_EINS       matrix[5] |= 0b1111000000000000
#define DE_H_ZWEI       matrix[5] |= 0b0000000111100000
#define DE_H_DREI       matrix[6] |= 0b1111000000000000
#define DE_H_VIER       matrix[6] |= 0b0000000111100000
#define DE_H_FUENF      matrix[4] |= 0b0000000111100000
#define DE_H_SECHS      matrix[7] |= 0b1111100000000000
#define DE_H_SIEBEN     matrix[8] |= 0b1111110000000000
#define DE_H_ACHT       matrix[7] |= 0b0000000111100000
#define DE_H_NEUN       matrix[9] |= 0b0001111000000000
#define DE_H_ZEHN       matrix[9] |= 0b1111000000000000
#define DE_H_ELF        matrix[4] |= 0b0000011100000000
#define DE_H_ZWOELF     matrix[8] |= 0b0000001111100000

#define DE_FUNK         matrix[3] |= 0b0001111000000000

#endif
