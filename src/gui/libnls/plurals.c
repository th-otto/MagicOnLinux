#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "libnls.h"
#include "libnlsI.h"


struct _nls_plural const libnls_plurals[] = {
#define P(c, n, e, s) { c, #c, n, e, s }
	P(PLURAL_NONE,        1, "nplurals=1; plural=0", "0"),
	P(PLURAL_NOT_ONE,     2, "nplurals=2; plural=n != 1", "n1#"),
	P(PLURAL_GREATER_ONE, 2, "nplurals=2; plural=(n > 1)", "n1>"),
	P(PLURAL_RULE_3,      2, "nplurals=2; plural=(n%10!=1 || n%100==11)", "n10%1#n100%11=|"),
	P(PLURAL_RULE_4,      2, "nplurals=2; plural=(n != 0)", "n0#"),
	P(PLURAL_RULE_5,      2, "nplurals=2; plural=n%10==1 ? 0 : 1", "n10%1=0 1?"),
	P(PLURAL_RULE_6,      2, "nplurals=2; plural=(n==1 || n==2 || n==3 || (n%10!=4 && n%10!=6 && n%10!=9))", "n1=n2=|n3=|n10%4#n10%6#&n10%9#&|"),
	P(PLURAL_RULE_7,      2, "nplurals=2; plural=(n<=1 || (n>=11 && n<=99))", "n1(n11)n99(&|"),
	P(PLURAL_RULE_8,      3, "nplurals=3; plural=(n==1) ? 0 : (n>=2 && n<=4) ? 1 : 2", "n1=0n2)n4(&1 2??"),
	P(PLURAL_RULE_9,      3, "nplurals=3; plural=(n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2)", "n10%1=n100%11#&0n10%2)n10%4(&n100%10<n100%20)|&1 2??"),
	P(PLURAL_RULE_10,     3, "nplurals=3; plural=(n%10==1 && n%100!=11 ? 0 : n != 0 ? 1 : 2)", "n10%1=n100%11#&0n0#1 2??"),
	P(PLURAL_RULE_11,     3, "nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2)", "n1=0n10%2)n10%4(&n100%10<n100%20)|&1 2??"),
	P(PLURAL_RULE_12,     3, "nplurals=3; plural=n==1 ? 0 : (n==0 || (n%100 > 0 && n%100 < 20)) ? 1 : 2", "n1=0n0=n100%0>n100%20<&|1 2??"),
	P(PLURAL_RULE_13,     3, "nplurals=3; plural=(n==1) ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2", "n1=0n10%2)n10%4(&n100%10<n100%20)|&1 2??"),
	P(PLURAL_RULE_14,     3, "nplurals=3; plural=n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2", "n10%1=n100%11#&0n10%2)n10%4(&n100%10<n100%20)|&1 2??"),
	P(PLURAL_RULE_15,     3, "nplurals=3; plural=(n==0 ? 0 : n==1 ? 1 : 2)", "n0=0n1=1 2??"),
	P(PLURAL_RULE_16,     3, "nplurals=3; plural=n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<12 || n%100>=14) ? 1 : 2", "n10%1=n100%11#&0n10%2)n10%4(&n100%12<n100%14)|&1 2??"),
	P(PLURAL_RULE_17,     3, "nplurals=3; plural=(n%10==0 || (n%100>=11 && n%100<=19) ? 0 : n%10==1 && n%100!=11 ? 1 : 2)", "n10%0=n100%11)n100%19(&|0n10%1=n100%11#&1 2??"),
	P(PLURAL_RULE_18,     3, "nplurals=3; plural=(n==0 || n==1 ? 0 : n>=2 && n<=10 ? 1 : 2)", "n0=n1=|0n2)n10(&1 2??"),
	P(PLURAL_RULE_19,     4, "nplurals=4; plural=(n==1 ? 0 : n%10>=2 && (n%100<10 || n%100>=20) ? 1 : n%10==0 || (n%100>10 && n%100<20) ? 2 : 3)", "n1=0n10%2)n100%10<n100%20)|&1n10%0=n100%10>n100%20<&|2 3???"),
	P(PLURAL_RULE_20,     4, "nplurals=4; plural=(n%100==1 ? 0 : n%100==2 ? 1 : n%100==3 || n%100==4 ? 2 : 3)", "n100%1=0n100%2=1n100%3=n100%4=|2 3???"),
	P(PLURAL_RULE_21,     4, "nplurals=4; plural=(n==1) ? 0 : (n==2) ? 1 : (n != 8 && n != 11) ? 2 : 3", "n1=0n2=1n8#n11#&2 3???"),
	P(PLURAL_RULE_22,     4, "nplurals=4; plural=(n==1 || n==11) ? 0 : (n==2 || n==12) ? 1 : (n > 2 && n < 20) ? 2 : 3", "n1=n11=|0n2=n12=|1n2>n20<&2 3???"),
	P(PLURAL_RULE_23,     4, "nplurals=4; plural=(n==1) ? 0 : (n==2) ? 1 : (n == 3) ? 2 : 3", "n1=0n2=1n3=2 3???"),
	P(PLURAL_RULE_24,     4, "nplurals=4; plural=(n==1 ? 0 : n==0 || ( n%100>1 && n%100<11) ? 1 : (n%100>10 && n%100<20 ) ? 2 : 3)", "n1=0n0=n100%1>n100%11<&|1n100%10>n100%20<&2 3???"),
	P(PLURAL_RULE_25,     4, "nplurals=4; plural=(n%10==1 ? 0 : n%10==2 ? 1 : n%100==0 || n%100==20 || n%100==40 || n%100==60 || n%100==80 ? 2 : 3)", "n10%1=0n10%2=1n100%0=n100%20=|n100%40=|n100%60=|n100%80=|2 3???"),
	P(PLURAL_RULE_26,     5, "nplurals=5; plural=n==1 ? 0 : n==2 ? 1 : n<7 ? 2 : n < 11 ? 3 : 4", "n1=0n2=1n7<2n11<3 4????"),
	P(PLURAL_RULE_27,     5, "nplurals=5; plural=(n%10==1 && n%100!=11 && n%100!=71 && n%100!=91 ? 0 : n%10==2 && n%100!=12 && n%100!=72 && n%100!=92 ? 1 : ((n%10>=3 && n%10<=4) || n%10==9) && (n%100<10 || n%100>19) && (n%100<70 || n%100>79) && (n%100<90 || n%100>99) ? 2 : n!=0 && n%1000000==0 ? 3 : 4)", "n10%1=n100%11#&n100%71#&n100%91#&0n10%2=n100%12#&n100%72#&n100%92#&1n10%3)n10%4(&n10%9=|n100%10<n100%19>|&n100%70<n100%79>|&n100%90<n100%99>|&2n0#n1000000%0=&3 4????"),
	P(PLURAL_RULE_28,     6, "nplurals=6; plural=(n==0 ? 0 : n==1 ? 1 : n==2 ? 2 : n%100>=3 && n%100<=10 ? 3 : n%100>=11 ? 4 : 5)", "n0=0n1=1n2=2n100%3)n100%10(&3n100%11)4 5?????"),
	P(PLURAL_RULE_29,     6, "nplurals=6; plural=(n==0 ? 0 : n==1 ? 1 : n==2 ? 2 : n==3 ? 3 : n==6 ? 4 : 5)", "n0=0n1=1n2=2n3=3n6=4 5?????"),
#undef P
	{ -1, NULL, 0, NULL, NULL }
};
