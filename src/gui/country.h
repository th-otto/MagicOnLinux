#ifndef COUNTRY_H
#define COUNTRY_H

typedef enum {
	LANG_SYSTEM = -1,
	COUNTRY_US = 0,	 /* USA */
	COUNTRY_DE = 1,	 /* Germany */
	COUNTRY_FR = 2,	 /* France */
	COUNTRY_UK = 3,	 /* United Kingdom */
	COUNTRY_ES = 4,	 /* Spain */
	COUNTRY_IT = 5,	 /* Italy */
	COUNTRY_SE = 6,	 /* Sweden */
	COUNTRY_SF = 7,	 /* Switzerland (French) */
	COUNTRY_SG = 8,	 /* Switzerland (German) */
	COUNTRY_TR = 9,	 /* Turkey */
	COUNTRY_FI = 10, /* Finland */
	COUNTRY_NO = 11, /* Norway */
	COUNTRY_DK = 12, /* Denmark */
	COUNTRY_SA = 13, /* Saudi Arabia */
	COUNTRY_NL = 14, /* Holland */
	COUNTRY_CZ = 15, /* Czech Republic */
	COUNTRY_HU = 16, /* Hungary */
	COUNTRY_PL = 17, /* Poland */
	COUNTRY_LT = 18, /* Lithuania */
	COUNTRY_RU = 19, /* Russia */
	COUNTRY_EE = 20, /* Estonia */
	COUNTRY_BY = 21, /* Belarus */
	COUNTRY_UA = 22, /* Ukraine */
	COUNTRY_SK = 23, /* Slovak Republic */
	COUNTRY_RO = 24, /* Romania */
	COUNTRY_BG = 25, /* Bulgaria */
	COUNTRY_SI = 26, /* Slovenia */
	COUNTRY_HR = 27, /* Croatia */
	COUNTRY_RS = 28, /* Serbia */
	COUNTRY_ME = 29, /* Montenegro */
	COUNTRY_MK = 30, /* Macedonia */
	COUNTRY_GR = 31, /* Greece */
	COUNTRY_LV = 32, /* Latvia */
	COUNTRY_IL = 33, /* Israel */
	COUNTRY_ZA = 34, /* South Africa */
	COUNTRY_PT = 35, /* Portugal */
	COUNTRY_BE = 36, /* Belgium */
	COUNTRY_JP = 37, /* Japan */
	COUNTRY_CN = 38, /* China */
	COUNTRY_KR = 39, /* Korea */
	COUNTRY_VN = 40, /* Vietnam */
	COUNTRY_IN = 41, /* India */
	COUNTRY_IR = 42, /* Iran */
	COUNTRY_MN = 43, /* Mongolia */
	COUNTRY_NP = 44, /* Nepal */
	COUNTRY_LA = 45, /* Lao People's Democratic Republic */
	COUNTRY_KH = 46, /* Cambodia */
	COUNTRY_ID = 47, /* Indonesia */
	COUNTRY_BD = 48, /* Bangladesh */
	COUNTRY_CA = 54, /* Catalan */
	COUNTRY_MX = 99, /* Mexico (found in Atari sources) */
} language_t;


#ifdef __cplusplus
extern "C" {
#endif

language_t language_get_default(void);
const char *language_get_name(language_t id);

#ifdef __cplusplus
}
#endif

#endif /* COUNTRY_H */
