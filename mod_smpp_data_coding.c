/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
*
* Version: MPL 1.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
*
* The Initial Developer of the Original Code is
* Anthony Minessale II <anthm@freeswitch.org>
* Portions created by the Initial Developer are Copyright (C)
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
* William King <william.king@quentustech.com>
* Dimitar Kokov <dkokov75@gmail.com>
*
* using examples from http://www.lemoda.net/c/utf8-to-ucs2/ and 
*                     http://www.lemoda.net/c/ucs2-to-utf8/ 
* for to release UTF-8 to UCS-2 and UCS-2 to UTF-8 converting 
*
* packing(septets to octets) and unpacking(octets to septets): 
*  https://www.codeproject.com/Tips/470755/Encoding-Decoding-bit-User-Data-for-SMS-PDU-PDU
*
*/

#include <mod_smpp.h>

#define NRP '?'

static const int latin1_to_gsm[256] = {
/* 0x00 */ NRP, /* pc: NON PRINTABLE */
/* 0x01 */ NRP, /* pc: NON PRINTABLE */
/* 0x02 */ NRP, /* pc: NON PRINTABLE */
/* 0x03 */ NRP, /* pc: NON PRINTABLE */
/* 0x04 */ NRP, /* pc: NON PRINTABLE */
/* 0x05 */ NRP, /* pc: NON PRINTABLE */
/* 0x06 */ NRP, /* pc: NON PRINTABLE */
/* 0x07 */ NRP, /* pc: NON PRINTABLE */
/* 0x08 */ NRP, /* pc: NON PRINTABLE */
/* 0x09 */ NRP, /* pc: NON PRINTABLE */
/* 0x0a */ 0x0a, /* pc: NON PRINTABLE */
/* 0x0b */ NRP, /* pc: NON PRINTABLE */
/* 0x0c */ -0x0a, /* pc: NON PRINTABLE */
/* 0x0d */ 0x0d, /* pc: NON PRINTABLE */
/* 0x0e */ NRP, /* pc: NON PRINTABLE */
/* 0x0f */ NRP, /* pc: NON PRINTABLE */
/* 0x10 */ NRP, /* pc: NON PRINTABLE */
/* 0x11 */ NRP, /* pc: NON PRINTABLE */
/* 0x12 */ NRP, /* pc: NON PRINTABLE */
/* 0x13 */ NRP, /* pc: NON PRINTABLE */
/* 0x14 */ NRP, /* pc: NON PRINTABLE */
/* 0x15 */ NRP, /* pc: NON PRINTABLE */
/* 0x16 */ NRP, /* pc: NON PRINTABLE */
/* 0x17 */ NRP, /* pc: NON PRINTABLE */
/* 0x18 */ NRP, /* pc: NON PRINTABLE */
/* 0x19 */ NRP, /* pc: NON PRINTABLE */
/* 0x1a */ NRP, /* pc: NON PRINTABLE */
/* 0x1b */ NRP, /* pc: NON PRINTABLE */
/* 0x1c */ NRP, /* pc: NON PRINTABLE */
/* 0x1d */ NRP, /* pc: NON PRINTABLE */
/* 0x1e */ NRP, /* pc: NON PRINTABLE */
/* 0x1f */ NRP, /* pc: NON PRINTABLE */
/* 0x20 */ 0x20, /* pc:   */
/* 0x21 */ 0x21, /* pc: ! */
/* 0x22 */ 0x22, /* pc: " */
/* 0x23 */ 0x23, /* pc: # */
/* 0x24 */ 0x02, /* pc: $ */
/* 0x25 */ 0x25, /* pc: % */
/* 0x26 */ 0x26, /* pc: & */
/* 0x27 */ 0x27, /* pc: ' */
/* 0x28 */ 0x28, /* pc: ( */
/* 0x29 */ 0x29, /* pc: ) */
/* 0x2a */ 0x2a, /* pc: * */
/* 0x2b */ 0x2b, /* pc: + */
/* 0x2c */ 0x2c, /* pc: , */
/* 0x2d */ 0x2d, /* pc: - */
/* 0x2e */ 0x2e, /* pc: . */
/* 0x2f */ 0x2f, /* pc: / */
/* 0x30 */ 0x30, /* pc: 0 */
/* 0x31 */ 0x31, /* pc: 1 */
/* 0x32 */ 0x32, /* pc: 2 */
/* 0x33 */ 0x33, /* pc: 3 */
/* 0x34 */ 0x34, /* pc: 4 */
/* 0x35 */ 0x35, /* pc: 5 */
/* 0x36 */ 0x36, /* pc: 6 */
/* 0x37 */ 0x37, /* pc: 7 */
/* 0x38 */ 0x38, /* pc: 8 */
/* 0x39 */ 0x39, /* pc: 9 */
/* 0x3a */ 0x3a, /* pc: : */
/* 0x3b */ 0x3b, /* pc: ; */
/* 0x3c */ 0x3c, /* pc: < */
/* 0x3d */ 0x3d, /* pc: = */
/* 0x3e */ 0x3e, /* pc: > */
/* 0x3f */ 0x3f, /* pc: ? */
/* 0x40 */ 0x00, /* pc: @ */
/* 0x41 */ 0x41, /* pc: A */
/* 0x42 */ 0x42, /* pc: B */
/* 0x43 */ 0x43, /* pc: C */
/* 0x44 */ 0x44, /* pc: D */
/* 0x45 */ 0x45, /* pc: E */
/* 0x46 */ 0x46, /* pc: F */
/* 0x47 */ 0x47, /* pc: G */
/* 0x48 */ 0x48, /* pc: H */
/* 0x49 */ 0x49, /* pc: I */
/* 0x4a */ 0x4a, /* pc: J */
/* 0x4b */ 0x4b, /* pc: K */
/* 0x4c */ 0x4c, /* pc: L */
/* 0x4d */ 0x4d, /* pc: M */
/* 0x4e */ 0x4e, /* pc: N */
/* 0x4f */ 0x4f, /* pc: O */
/* 0x50 */ 0x50, /* pc: P */
/* 0x51 */ 0x51, /* pc: Q */
/* 0x52 */ 0x52, /* pc: R */
/* 0x53 */ 0x53, /* pc: S */
/* 0x54 */ 0x54, /* pc: T */
/* 0x55 */ 0x55, /* pc: U */
/* 0x56 */ 0x56, /* pc: V */
/* 0x57 */ 0x57, /* pc: W */
/* 0x58 */ 0x58, /* pc: X */
/* 0x59 */ 0x59, /* pc: Y */
/* 0x5a */ 0x5a, /* pc: Z */
/* 0x5b */ 0x1b3c, /* pc: [ */
/* 0x5c */ 0x1b2f, /* pc: \ */
/* 0x5d */ 0x1b3e, /* pc: ] */
/* 0x5e */ 0x1b14, /* pc: ^ */
/* 0x5f */ 0x11, /* pc: _ */
/* 0x60 */ NRP, /* pc: ` */
/* 0x61 */ 0x61, /* pc: a */
/* 0x62 */ 0x62, /* pc: b */
/* 0x63 */ 0x63, /* pc: c */
/* 0x64 */ 0x64, /* pc: d */
/* 0x65 */ 0x65, /* pc: e */
/* 0x66 */ 0x66, /* pc: f */
/* 0x67 */ 0x67, /* pc: g */
/* 0x68 */ 0x68, /* pc: h */
/* 0x69 */ 0x69, /* pc: i */
/* 0x6a */ 0x6a, /* pc: j */
/* 0x6b */ 0x6b, /* pc: k */
/* 0x6c */ 0x6c, /* pc: l */
/* 0x6d */ 0x6d, /* pc: m */
/* 0x6e */ 0x6e, /* pc: n */
/* 0x6f */ 0x6f, /* pc: o */
/* 0x70 */ 0x70, /* pc: p */
/* 0x71 */ 0x71, /* pc: q */
/* 0x72 */ 0x72, /* pc: r */
/* 0x73 */ 0x73, /* pc: s */
/* 0x74 */ 0x74, /* pc: t */
/* 0x75 */ 0x75, /* pc: u */
/* 0x76 */ 0x76, /* pc: v */
/* 0x77 */ 0x77, /* pc: w */
/* 0x78 */ 0x78, /* pc: x */
/* 0x79 */ 0x79, /* pc: y */
/* 0x7a */ 0x7a, /* pc: z */
/* 0x7b */ 0x1b28, /* pc: { */
/* 0x7c */ 0x1b40, /* pc: | */
/* 0x7d */ 0x1b29, /* pc: } */
/* 0x7e */ 0x1b3d, /* pc: ~ */
/* 0x7f */ NRP, /* pc: NON PRINTABLE */
/* 0x80 */ NRP, /* pc: NON PRINTABLE */
/* 0x81 */ NRP, /* pc: NON PRINTABLE */
/* 0x82 */ NRP, /* pc: NON PRINTABLE */
/* 0x83 */ NRP, /* pc: NON PRINTABLE */
/* 0x84 */ NRP, /* pc: NON PRINTABLE */
/* 0x85 */ NRP, /* pc: NON PRINTABLE */
/* 0x86 */ NRP, /* pc: NON PRINTABLE */
/* 0x87 */ NRP, /* pc: NON PRINTABLE */
/* 0x88 */ NRP, /* pc: NON PRINTABLE */
/* 0x89 */ NRP, /* pc: NON PRINTABLE */
/* 0x8a */ NRP, /* pc: NON PRINTABLE */
/* 0x8b */ NRP, /* pc: NON PRINTABLE */
/* 0x8c */ NRP, /* pc: NON PRINTABLE */
/* 0x8d */ NRP, /* pc: NON PRINTABLE */
/* 0x8e */ NRP, /* pc: NON PRINTABLE */
/* 0x8f */ NRP, /* pc: NON PRINTABLE */
/* 0x90 */ NRP, /* pc: NON PRINTABLE */
/* 0x91 */ NRP, /* pc: NON PRINTABLE */
/* 0x92 */ NRP, /* pc: NON PRINTABLE */
/* 0x93 */ NRP, /* pc: NON PRINTABLE */
/* 0x94 */ NRP, /* pc: NON PRINTABLE */
/* 0x95 */ NRP, /* pc: NON PRINTABLE */
/* 0x96 */ NRP, /* pc: NON PRINTABLE */
/* 0x97 */ NRP, /* pc: NON PRINTABLE */
/* 0x98 */ NRP, /* pc: NON PRINTABLE */
/* 0x99 */ NRP, /* pc: NON PRINTABLE */
/* 0x9a */ NRP, /* pc: NON PRINTABLE */
/* 0x9b */ NRP, /* pc: NON PRINTABLE */
/* 0x9c */ NRP, /* pc: NON PRINTABLE */
/* 0x9d */ NRP, /* pc: NON PRINTABLE */
/* 0x9e */ NRP, /* pc: NON PRINTABLE */
/* 0x9f */ NRP, /* pc: NON PRINTABLE */
/* 0xa0 */ NRP, /* pc: NON PRINTABLE */
/* 0xa1 */ 0x40, /* pc: INVERTED EXCLAMATION MARK */
/* 0xa2 */ NRP, /* pc: NON PRINTABLE */
/* 0xa3 */ 0x01, /* pc: POUND SIGN */
/* 0xa4 */ 0x24, /* pc: CURRENCY SIGN */
/* 0xa5 */ 0x03, /* pc: YEN SIGN*/
/* 0xa6 */ NRP, /* pc: NON PRINTABLE */
/* 0xa7 */ 0x5f, /* pc: SECTION SIGN */
/* 0xa8 */ NRP, /* pc: NON PRINTABLE */
/* 0xa9 */ NRP, /* pc: NON PRINTABLE */
/* 0xaa */ NRP, /* pc: NON PRINTABLE */
/* 0xab */ NRP, /* pc: NON PRINTABLE */
/* 0xac */ NRP, /* pc: NON PRINTABLE */
/* 0xad */ NRP, /* pc: NON PRINTABLE */
/* 0xae */ NRP, /* pc: NON PRINTABLE */
/* 0xaf */ NRP, /* pc: NON PRINTABLE */
/* 0xb0 */ NRP, /* pc: NON PRINTABLE */
/* 0xb1 */ NRP, /* pc: NON PRINTABLE */
/* 0xb2 */ NRP, /* pc: NON PRINTABLE */
/* 0xb3 */ NRP, /* pc: NON PRINTABLE */
/* 0xb4 */ NRP, /* pc: NON PRINTABLE */
/* 0xb5 */ NRP, /* pc: NON PRINTABLE */
/* 0xb6 */ NRP, /* pc: NON PRINTABLE */
/* 0xb7 */ NRP, /* pc: NON PRINTABLE */
/* 0xb8 */ NRP, /* pc: NON PRINTABLE */
/* 0xb9 */ NRP, /* pc: NON PRINTABLE */
/* 0xba */ NRP, /* pc: NON PRINTABLE */
/* 0xbb */ NRP, /* pc: NON PRINTABLE */
/* 0xbc */ NRP, /* pc: NON PRINTABLE */
/* 0xbd */ NRP, /* pc: NON PRINTABLE */
/* 0xbe */ NRP, /* pc: NON PRINTABLE */
/* 0xbf */ 0x60, /* pc: INVERTED QUESTION MARK */
/* 0xc0 */ NRP, /* pc: NON PRINTABLE */
/* 0xc1 */ NRP, /* pc: NON PRINTABLE */
/* 0xc2 */ NRP, /* pc: NON PRINTABLE */
/* 0xc3 */ NRP, /* pc: NON PRINTABLE */
/* 0xc4 */ 0x5b, /* pc: LATIN CAPITAL LETTER A WITH DIAERESIS */
/* 0xc5 */ 0x0e, /* pc: LATIN CAPITAL LETTER A WITH RING ABOVE */
/* 0xc6 */ 0x1c, /* pc: LATIN CAPITAL LETTER AE */
/* 0xc7 */ 0x09, /* pc: LATIN CAPITAL LETTER C WITH CEDILLA (mapped to small) */
/* 0xc8 */ NRP, /* pc: NON PRINTABLE */
/* 0xc9 */ 0x1f, /* pc: LATIN CAPITAL LETTER E WITH ACUTE  */
/* 0xca */ NRP, /* pc: NON PRINTABLE */
/* 0xcb */ NRP, /* pc: NON PRINTABLE */
/* 0xcc */ NRP, /* pc: NON PRINTABLE */
/* 0xcd */ NRP, /* pc: NON PRINTABLE */
/* 0xce */ NRP, /* pc: NON PRINTABLE */
/* 0xcf */ NRP, /* pc: NON PRINTABLE */
/* 0xd0 */ NRP, /* pc: NON PRINTABLE */
/* 0xd1 */ 0x5d, /* pc: LATIN CAPITAL LETTER N WITH TILDE */
/* 0xd2 */ NRP, /* pc: NON PRINTABLE */
/* 0xd3 */ NRP, /* pc: NON PRINTABLE */
/* 0xd4 */ NRP, /* pc: NON PRINTABLE */
/* 0xd5 */ NRP, /* pc: NON PRINTABLE */
/* 0xd6 */ 0x5c, /* pc: LATIN CAPITAL LETTER O WITH DIAEREIS */
/* 0xd7 */ NRP, /* pc: NON PRINTABLE */
/* 0xd8 */ 0x0b, /* pc: LATIN CAPITAL LETTER O WITH STROKE */
/* 0xd9 */ NRP, /* pc: NON PRINTABLE */
/* 0xda */ NRP, /* pc: NON PRINTABLE */
/* 0xdb */ NRP, /* pc: NON PRINTABLE */
/* 0xdc */ 0x5e, /* pc: LATIN CAPITAL LETTER U WITH DIAERESIS */
/* 0xdd */ NRP, /* pc: NON PRINTABLE */
/* 0xde */ NRP, /* pc: NON PRINTABLE */
/* 0xdf */ 0x1e, /* pc: LATIN SMALL LETTER SHARP S */
/* 0xe0 */ 0x7f, /* pc: LATIN SMALL LETTER A WITH GRAVE */
/* 0xe1 */ NRP, /* pc: NON PRINTABLE */
/* 0xe2 */ NRP, /* pc: NON PRINTABLE */
/* 0xe3 */ NRP, /* pc: NON PRINTABLE */
/* 0xe4 */ 0x7b, /* pc: LATIN SMALL LETTER A WITH DIAERESIS */
/* 0xe5 */ 0x0f, /* pc: LATIN SMALL LETTER A WITH RING ABOVE */
/* 0xe6 */ 0x1d, /* pc: LATIN SMALL LETTER AE */
/* 0xe7 */ 0x09, /* pc: LATIN SMALL LETTER C WITH CEDILLA */
/* 0xe8 */ 0x04, /* pc: NON PRINTABLE */
/* 0xe9 */ 0x05, /* pc: NON PRINTABLE */
/* 0xea */ NRP, /* pc: NON PRINTABLE */
/* 0xeb */ NRP, /* pc: NON PRINTABLE */
/* 0xec */ 0x07, /* pc: NON PRINTABLE */
/* 0xed */ NRP, /* pc: NON PRINTABLE */
/* 0xee */ NRP, /* pc: NON PRINTABLE */
/* 0xef */ NRP, /* pc: NON PRINTABLE */
/* 0xf0 */ NRP, /* pc: NON PRINTABLE */
/* 0xf1 */ 0x7d, /* pc: NON PRINTABLE */
/* 0xf2 */ 0x08, /* pc: NON PRINTABLE */
/* 0xf3 */ NRP, /* pc: NON PRINTABLE */
/* 0xf4 */ NRP, /* pc: NON PRINTABLE */
/* 0xf5 */ NRP, /* pc: NON PRINTABLE */
/* 0xf6 */ 0x7c, /* pc: NON PRINTABLE */
/* 0xf7 */ NRP, /* pc: NON PRINTABLE */
/* 0xf8 */ 0x0c, /* pc: NON PRINTABLE */
/* 0xf9 */ 0x06, /* pc: NON PRINTABLE */
/* 0xfa */ NRP, /* pc: NON PRINTABLE */
/* 0xfb */ NRP, /* pc: NON PRINTABLE */
/* 0xfc */ 0x7e, /* pc: NON PRINTABLE */
/* 0xfd */ NRP, /* pc: NON PRINTABLE */
/* 0xfe */ NRP, /* pc: NON PRINTABLE */
/* 0xff */ NRP, /* pc: NON PRINTABLE */
};

static const int gsm_to_unicode[128] = {
      '@',  0xA3,   '$',  0xA5,  0xE8,  0xE9,  0xF9,  0xEC,   /* 0 - 7 */
     0xF2,  0xC7,    10,  0xd8,  0xF8,    13,  0xC5,  0xE5,   /* 8 - 15 */
    0x394,   '_', 0x3A6, 0x393, 0x39B, 0x3A9, 0x3A0, 0x3A8,   /* 16 - 23 */
    0x3A3, 0x398, 0x39E,   NRP,  0xC6,  0xE6,  0xDF,  0xC9,   /* 24 - 31 */
      ' ',   '!',   '"',   '#',  0xA4,   '%',   '&',  '\'',   /* 32 - 39 */
      '(',   ')',   '*',   '+',   ',',   '-',   '.',   '/',   /* 40 - 47 */
      '0',   '1',   '2',   '3',   '4',   '5',   '6',   '7',   /* 48 - 55 */
      '8',   '9',   ':',   ';',   '<',   '=',   '>',   '?',   /* 56 - 63 */
      0xA1,  'A',   'B',   'C',   'D',   'E',   'F',   'G',   /* 64 - 71 */
      'H',   'I',   'J',   'K',   'L',   'M',   'N',   'O',   /* 73 - 79 */
      'P',   'Q',   'R',   'S',   'T',   'U',   'V',   'W',   /* 80 - 87 */
      'X',   'Y',   'Z',  0xC4,  0xD6,  0xD1,  0xDC,  0xA7,   /* 88 - 95 */
     0xBF,   'a',   'b',   'c',   'd',   'e',   'f',   'g',   /* 96 - 103 */
      'h',   'i',   'j',   'k',   'l',   'm',   'n',   'o',   /* 104 - 111 */
      'p',   'q',   'r',   's',   't',   'u',   'v',   'w',   /* 112 - 119 */
      'x',   'y',   'z',  0xE4,  0xF6,  0xF1,  0xFC,  0xE0    /* 120 - 127 */
};

static const int gsm_to_utf8[128] = {
      '@',  0xC2A3,    '$', 0xC2A5, 0xC3A8, 0xC3A9, 0xC3B9, 0xC3AC, /*   0 -   7 */
    0xC3B2, 0xC387,     10, 0xC398, 0xC3B8,     13, 0xC385, 0xC3A5, /*   8 -  15 */
    0xCE94,   '_' , 0xCEA6, 0xCE93, 0xCE9B, 0xCEA9, 0xCEA0, 0xCEA8, /*  16 -  23 */
    0xCEA3, 0xCE98, 0xCE9E,    NRP, 0xC386, 0xC3A6, 0xC39F, 0xC389, /*  24 -  31 */
       ' ',    '!',    '"',    '#', 0xC2A4,    '%',    '&',   '\'', /*  32 -  39 */
       '(',    ')',    '*',    '+',    ',',    '-',    '.',    '/', /*  40 -  47 */
       '0',    '1',    '2',    '3',    '4',    '5',    '6',    '7', /*  48 -  55 */
       '8',    '9',    ':',    ';',    '<',    '=',    '>',    '?', /*  56 -  63 */
    0xC2A1,    'A',    'B',    'C',    'D',    'E',    'F',    'G', /*  64 -  71 */
       'H',    'I',    'J',    'K',    'L',    'M',    'N',    'O', /*  73 -  79 */
       'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W', /*  80 -  87 */
       'X',    'Y',    'Z', 0xC384, 0xC396, 0xC391, 0xC393, 0xC2A7, /*  88 -  95 */
    0xC2BF,    'a',    'b',    'c',    'd',    'e',    'f',    'g', /*  96 - 103 */
       'h',    'i',    'j',    'k',    'l',    'm',    'n',    'o', /* 104 - 111 */
       'p',    'q',    'r',    's',    't',    'u',    'v',    'w', /* 112 - 119 */
       'x',    'y',    'z', 0xC3A4, 0xC3B6, 0xC3B1, 0xC3BC, 0xC3A0  /* 120 - 127 */
};

static unsigned char masks[7]  = { 0x00, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f };
static unsigned char rmasks[7] = { 0x00, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01 };
static unsigned char zmasks[7] = { 0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc };

int mod_smpp_utf8_to_gsm7_tb(unsigned char *utf8,unsigned char *gsm7)
{
    int i,p;
    int gsm7_char;
//    int utf8_len;

    i=0;p=0;
    while(utf8[i] != '\0') {
	gsm7_char = latin1_to_gsm[utf8[i]];
	
	if(gsm7_char > 255) {
	    gsm7[p] = (gsm7_char >> 8);
	    p++;
	}
	
	gsm7[p] = gsm7_char;
	p++;
	
	i++;
    }

    return p;
}

int mod_smpp_gsm7_to_utf8_tb(unsigned char *gsm7,int gsm7_len,unsigned char *utf8)
{
    int i,p;
    int wd;

    i=0;p=0;
    while(i < gsm7_len) {
	if(gsm7[i] == 0x1b) {
	    wd = 0;
	    switch(gsm7[i+1]) {
		case 0x14:
		    wd = 0x5e; // '^'
		    break;
		case 0x28:
		    wd = 0x7b; // '{'
		    break;
		case 0x29:
		    wd = 0x7d; // '}'
		    break;
		case 0x2f:
		    wd = 0x5c; // '\'
		    break;
		case 0x3c:
		    wd = 0x5b; // '['
		    break;
		case 0x3d:
		    wd = 0x7e; // '~'
		    break;
		case 0x3e:
		    wd = 0x5d; // ']'
		    break;
		case 0x40:
		    wd = 0x7c; // '|'
		    break;
	    }
	    i++;
	} else {
	    wd = gsm_to_utf8[gsm7[i]];
	}
	
	if(wd > 0) {
	    if(wd > 0xff) {
		utf8[p] = wd>>8;
		p++;
	    }
	    utf8[p] = wd;
	    p++;
	}
	i++;
    }
    return p;
}

/*
 *  1.Character converting from 'ISO 8859 Latin 1' to 'GSM 7 bit Default Alphabet'
 *  2.Bytes packing from 'septets' to 'octets'
 *   */
int mod_smpp_utf8_to_gsm7(unsigned char *utf8,unsigned char *gsm7)
{
    int i,c,p,utf8_len;
    unsigned char gsm7_tb[255] = {0};

    utf8_len = mod_smpp_utf8_to_gsm7_tb(utf8,gsm7_tb);

    i = 0;p = 0;
    while( i < utf8_len ) {
	for(c=0;c<7;c++) {
	    if( i > (utf8_len-1) ) goto _end;
	
	    if((c == 6)&&(gsm7_tb[i+1]=='\0')) gsm7_tb[i+1] = 0x0d;
	
	    if(c == 0) gsm7[p] = (gsm7_tb[i])|((gsm7_tb[i+1]&0x01)<<7);
	    else gsm7[p] = (gsm7_tb[i]>>c)|((gsm7_tb[i+1]&masks[c])<<(7-c));
	
	    p++;i++;
	}
	
	i++;
    }

    _end:
    return p;
}

/*
 *  1.Bytes unpacking from 'octets' to 'septets'
 *   2.Character converting from 'GSM 7 bit Default Alphabet' to 'ISO 8859 Latin 1'
 *   */
const char *mod_smpp_gsm7_to_utf8(unsigned char *gsm7,int gsm7_len)
{
    int i,p,c;
    int iter,utf8_len;

    unsigned char wd;
    unsigned char buf[300],buf2[300];

    if((gsm7 == NULL)||(gsm7_len <= 0)) return NULL;

    memset(buf,0,300);
    memset(buf2,0,300);

    iter = (gsm7_len) / 7;
    utf8_len = gsm7_len + iter;

    i=0;p=0;c=0;
    while( p < utf8_len ) {
	if(c == 0) {
	    wd = (gsm7[i]&0x7f);
	} else if(c == 7) { 
	    wd = (gsm7[i]>>1);
	    i++;
	} else {
	    wd = ((gsm7[i]&zmasks[c])>>(8-c))|((gsm7[i+1]&rmasks[c])<<c);
	    i++;
	}
	
	if( c < 7) c++;
	else c = 0;
	
	if((c==0)&&(wd==0x00)) break; // ???
	
	buf[p] = wd;
	p++;
    }

    mod_smpp_gsm7_to_utf8_tb(buf,p,buf2);
    return (const char *)strdup((const char *)buf2);
}

/*
 message = 'Short SMS EN'
 short_message = 53 68 6f 72 74 20 53 4d 53 20 45 4e (UTF-8)
 data_coding = 0

 message = 'Кратък СМС на БГ' (same short message in bulgarian!)
 short_message = 04 1a 04 40 04 30 04 42 04 4a 04 3a 00 20 04 21 04 1c 04 21 00 20 04 3d 04 30 00 20 04 11 04 13 (UCS-2)
 data_coding = 8
 short_message = d0 9a d1 80 d0 b0 d1 82 d1 8a d0 ba 20 d0 a1 d0 9c d0 a1 20 d0 bd d0 b0 20 d0 91 d0 93 (UTF-8)
 data_coding = 0
*/

int mod_smpp_data_coding_recognize(unsigned char *str_ptr)
{
    int i;

    i=0;
    while ( str_ptr[i] ) {
		if( str_ptr[i] > 0x80 ) return 8;
		i++;
    }

    return 0;
}

int mod_smpp_utf8_to_ucs2 (unsigned char *input,unsigned char *output)
{
    unsigned char *utf8;
    unsigned short ucs2;
    int i,c;

    i = 0; c = 0;
    while( input[i] ) {
		utf8 = &input[i];
		ucs2 = 0;
	
		if( utf8[0] < 0x80 ) {
			ucs2 = utf8[0];
			i++;
		} else if( ( utf8[0] & 0xE0 ) == 0xE0 ) {
			if ( utf8[1] < 0x80 || utf8[1] > 0xBF || utf8[2] < 0x80 || utf8[2] > 0xBF ) {
				return -1;
			}
	
			ucs2 = ((utf8[0] & 0x0F)<<12 )|((utf8[1] & 0x3F)<<6)|(utf8[2] & 0x3F);
	
			i = i + 3;
		} else if ((utf8[0] & 0xC0) == 0xC0) {
			if (utf8[1] < 0x80 || utf8[1] > 0xBF) {
				return -2;
			}
			
			//ucs2 = ((utf8[0] & 0x1F)<<6 | utf8[1] & 0x3F);
			ucs2 = (((utf8[0] & 0x1F)<<6) | (utf8[1] & 0x3F));
			
			i = i + 2;
		} else {
		    //return -3; vremenno !!!
		    i = i + 4; // vremenno !!!
		    continue; // vremenno !!!
		}
		
		output[c] = ucs2 >> 8;
		c++;
	
		output[c] = ucs2;
		c++;
    }

    return c;
}

int mod_smpp_ucs2_to_utf8 (unsigned char *input, unsigned char *output)
{
    int i,c,ucs2;
    unsigned char *ucs2_ptr;

    c = 0;i = 0;
    ucs2_ptr = &input[i];
    ucs2 = ( ucs2_ptr[0] << 8 | ucs2_ptr[1] );

    while ( ucs2 ) {
		if ( ucs2 < 0x80 ) {
			output[c] = ucs2;
			c++;
		}
	
		if ( ucs2 >= 0x80  && ucs2 < 0x800 ) {
			output[c] = (ucs2 >> 6)   | 0xC0;
			c++;

			output[c] = (ucs2 & 0x3F) | 0x80;
			c++;
		}
	
		if ( ucs2 >= 0x800 && ucs2 < 0xFFFF ) {
			if (ucs2 >= 0xD800 && ucs2 <= 0xDFFF) {
				//return UNICODE_SURROGATE_PAIR;
				return -1;
			}
	
			output[c] = ((ucs2 >> 12)) | 0xE0;
			c++;

			output[c] = ((ucs2 >> 6 ) & 0x3F) | 0x80;
			c++;

			output[c] = ((ucs2      ) & 0x3F) | 0x80;
			c++;
	
		}
	
		if ( ucs2 >= 0x10000 && ucs2 < 0x10FFFF ) {
			output[c] = 0xF0 | (ucs2 >> 18);
			c++;
		
			output[c] = 0x80 | ((ucs2 >> 12) & 0x3F);
			c++;
		
			output[c] = 0x80 | ((ucs2 >> 6) & 0x3F);
			c++;
		
			output[c] = 0x80 | ((ucs2 & 0x3F));
			c++;
	    }
	
		//	return UNICODE_BAD_INPUT;
		//	return -1;
	
		i = i + 2;
		ucs2_ptr = &input[i];
		ucs2 = ( ucs2_ptr[0] << 8 | ucs2_ptr[1] );
    }

    return 0;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
