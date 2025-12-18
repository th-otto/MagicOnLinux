#include "country.h"
#include <string.h>
#include <stdlib.h>
#include <locale.h>

static struct {
	language_t id;
	char short_code[3];
	char full_code[6];
#ifdef QT_VERSION
	QLocale::Language language;
	QLocale::Country country;
#define QT(l, c) , QLocale::l, QLocale::c
#else
#define QT(l, c)
#endif
} const languages[] = {
	{ COUNTRY_US, "en", "en_US" QT(English, UnitedStates) },
	{ COUNTRY_DE, "de", "de_DE" QT(German, Germany) }, 
	{ COUNTRY_FR, "fr", "fr_FR" QT(French, France) },
	{ COUNTRY_UK, "en", "en_GB" QT(English, UnitedKingdom) }, 
	{ COUNTRY_ES, "es", "es_ES" QT(Spanish, Spain) },
	{ COUNTRY_IT, "it", "it_IT" QT(Italian, Italy) },
	{ COUNTRY_SE, "sv", "sv_SE" QT(Swedish, Sweden) },
	{ COUNTRY_SF, "fr", "fr_CH" QT(French, Switzerland) },
	{ COUNTRY_SG, "de", "de_CH" QT(German, Switzerland) },
	{ COUNTRY_TR, "tr", "tr_TR" QT(Turkish, Turkey) },
	{ COUNTRY_FI, "fi", "fi_FI" QT(Finnish, Finland) },
	{ COUNTRY_NO, "no", "no_NO" QT(NorwegianBokmal, Norway) },
	{ COUNTRY_DK, "da", "da_DK" QT(Danish, Denmark) },
	{ COUNTRY_SA, "ar", "ar_SA" QT(Arabic, SaudiArabia) },
	{ COUNTRY_NL, "nl", "nl_NL" QT(Dutch, Netherlands) },
	{ COUNTRY_CZ, "cs", "cs_CZ" QT(Czech, CzechRepublic) },
	{ COUNTRY_HU, "hu", "hu_HU" QT(Hungarian, Hungary) },
	{ COUNTRY_PL, "pl", "pl_PL" QT(Polish, Poland) },
	{ COUNTRY_LT, "lt", "lt_LT" QT(Lithuanian, Lithuania) },
	{ COUNTRY_RU, "ru", "ru_RU" QT(Russian, Russia) },
	{ COUNTRY_EE, "et", "et_EE" QT(Estonian, Estonia) },
	{ COUNTRY_BY, "be", "be_BY" QT(Belarusian, Belarus) },
	{ COUNTRY_UA, "uk", "uk_UA" QT(Ukrainian, Ukraine) },
	{ COUNTRY_SK, "sk", "sk_SK" QT(Slovak, Slovakia) },
	{ COUNTRY_RO, "ro", "ro_RO" QT(Romanian, Romania) },
	{ COUNTRY_BG, "bg", "bg_BG" QT(Bulgarian, Bulgaria) },
	{ COUNTRY_SI, "sl", "sl_SI" QT(Slovenian, Slovenia) },
	{ COUNTRY_HR, "hr", "hr_HR" QT(Croatian, Croatia) },
	{ COUNTRY_RS, "sr", "sr_RS" QT(Serbian, Serbia) },
	{ COUNTRY_ME, "sr", "sr_ME" QT(Serbian, Montenegro) },
	{ COUNTRY_MK, "mk", "mk_MK" QT(Macedonian, Macedonia) },
	{ COUNTRY_GR, "el", "el_GR" QT(Greek, Greece) },
	{ COUNTRY_LV, "lv", "lv_LV" QT(Latvian, Latvia) },
	{ COUNTRY_IL, "he", "he_IL" QT(Hebrew, Israel) },
	{ COUNTRY_ZA, "af", "af_ZA" QT(Afrikaans, SouthAfrica) },
	{ COUNTRY_PT, "pt", "pt_PT" QT(Portuguese, Portugal) },
	{ COUNTRY_BE, "fr", "fr_BE" QT(French, Belgium) },
	{ COUNTRY_JP, "ja", "ja_JP" QT(Japanese, Japan) },
	{ COUNTRY_CN, "zh", "zh_CN" QT(Chinese, China) },
	{ COUNTRY_KR, "ko", "ko_KR" QT(Korean, SouthKorea) },
	{ COUNTRY_VN, "vi", "vi_VN" QT(Vietnamese, Vietnam) },
	{ COUNTRY_IN, "hi", "hi_IN" QT(Hindi, India) },
	{ COUNTRY_IR, "fa", "fa_IR" QT(Persian, Iran) },
	{ COUNTRY_MN, "mn", "mn_MN" QT(Mongolian, Mongolia) },
	{ COUNTRY_NP, "ne", "ne_NP" QT(Nepali, Nepal) },
	{ COUNTRY_LA, "lo", "lo_LA" QT(Lao, Laos) },
	{ COUNTRY_KH, "km", "km_KH" QT(Khmer, Cambodia) },
	{ COUNTRY_ID, "id", "id_ID" QT(Indonesian, Indonesia) },
	{ COUNTRY_BD, "bn", "bn_BD" QT(Bengali, Bangladesh) },
	{ COUNTRY_CA, "ca", "ca_ES" QT(Catalan, Spain) },
};
#undef QT

language_t language_from_name(const char *lang_id)
{
	size_t i;

	for (i = 0; i < sizeof(languages) / sizeof(languages[0]); i++)
	{
		if (strncmp(lang_id, languages[i].full_code, 5) == 0)
		{
			return languages[i].id;
		}
	}
	for (i = 0; i < sizeof(languages) / sizeof(languages[0]); i++)
	{
		if (strncmp(lang_id, languages[i].short_code, 2) == 0)
		{
			return languages[i].id;
		}
	}
	return LANG_SYSTEM;
}


language_t language_get_default(void)
{
	const char *locale;
	
	locale = setlocale(LC_MESSAGES, NULL);
	if (locale == NULL || locale[0] == '\0')
    {
		locale = getenv("LC_ALL");
		if (locale == NULL || locale[0] == '\0')
		{
			locale = getenv("LC_MESSAGES");
			if (locale == NULL || locale[0] == '\0')
			{
				locale = getenv("LANG");
			}
		}
	}
	if (locale != NULL)
	{
		language_t id = language_from_name(locale);
		if (id != LANG_SYSTEM)
			return id;
	}	
	return COUNTRY_US;
}

const char *language_get_name(language_t id)
{
	size_t i;
	
	for (i = 0; i < sizeof(languages) / sizeof(languages[0]); i++)
	{
		if (id == languages[i].id)
			return languages[i].full_code;
	}
	return "C";
}
