#ifndef _CUST_BATTERY_METER_TABLE_H
#define _CUST_BATTERY_METER_TABLE_H

#include <mach/mt_typedefs.h>
#include <mach/battery_meter.h>

/* ============================================================ */
/* define */
/* ============================================================ */
#define BAT_NTC_10 1
#define BAT_NTC_47 0

#if (BAT_NTC_10 == 1)
#define RBAT_PULL_UP_R             16900
#define RBAT_PULL_DOWN_R		   27000
#endif

#if (BAT_NTC_47 == 1)
#define RBAT_PULL_UP_R             61900
#define RBAT_PULL_DOWN_R		  100000
#endif
#define RBAT_PULL_UP_VOLT          1800



/* ============================================================ */
/* ENUM */
/* ============================================================ */

/* ============================================================ */
/* structure */
/* ============================================================ */

/* ============================================================ */
/* typedef */
/* ============================================================ */

/* ============================================================ */
/* External Variables */
/* ============================================================ */

/* ============================================================ */
/* External function */
/* ============================================================ */

/* ============================================================ */
/* <DOD, Battery_Voltage> Table */
/* ============================================================ */
#if (BAT_NTC_10 == 1)
BATT_TEMPERATURE Batt_Temperature_Table[] = {
	{-20, 68237},
	{-15, 53650},
	{-10, 42506},
	{-5, 33892},
	{0, 27219},
	{5, 22021},
	{10, 17926},
	{15, 14674},
	{20, 12081},
	{25, 10000},
	{30, 8315},
	{35, 6948},
	{40, 5834},
	{45, 4917},
	{50, 4161},
	{55, 3535},
	{60, 3014},
	{65, 2404},
	{70, 1846}
};
#endif

#if (BAT_NTC_47 == 1)
BATT_TEMPERATURE Batt_Temperature_Table[] = {
	{-20, 483954},
	{-15, 360850},
	{-10, 271697},
	{-5, 206463},
	{0, 158214},
	{5, 122259},
	{10, 95227},
	{15, 74730},
	{20, 59065},
	{25, 47000},
	{30, 37643},
	{35, 30334},
	{40, 24591},
	{45, 20048},
	{50, 16433},
	{55, 13539},
	{60, 11210}
};
#endif

/* T0 -10C */
BATTERY_PROFILE_STRUC battery_profile_t0[] = {
{0,4106},
{5,4060},
{9,4036},
{14,4022},
{18,4009},
{22,3996},
{27,3981},
{31,3963},
{36,3933},
{40,3878},
{45,3846},
{49,3829},
{54,3818},
{58,3808},
{62,3801},
{66,3795},
{69,3790},
{71,3785},
{73,3780},
{76,3775},
{77,3769},
{79,3764},
{80,3759},
{81,3754},
{82,3750},
{83,3746},
{84,3742},
{85,3739},
{86,3736},
{87,3733},
{87,3731},
{88,3728},
{89,3726},
{90,3724},
{90,3723},
{90,3721},
{91,3720},
{91,3719},
{92,3717},
{92,3717},
{93,3716},
{93,3715},
{94,3714},
{94,3714},
{95,3713},
{95,3713},
{95,3712},
{95,3711},
{96,3711},
{96,3711},
{97,3710},
{97,3710},
{97,3710},
{98,3709},
{98,3709},
{98,3708},
{99,3708},
{99,3708},
{99,3707},
{100,3707},
{100,3707},
{100,3400}
};


/* T1 0C */
BATTERY_PROFILE_STRUC battery_profile_t1[] = {
{0,4322},
{2,4297},
{4,4273},
{6,4251},
{9,4228},
{10,4199},
{13,4154},
{15,4115},
{17,4093},
{19,4079},
{21,4055},
{23,4019},
{25,3990},
{28,3969},
{29,3955},
{32,3943},
{34,3929},
{36,3914},
{38,3898},
{40,3883},
{42,3869},
{44,3858},
{46,3847},
{48,3837},
{51,3829},
{53,3822},
{55,3814},
{57,3807},
{59,3801},
{61,3794},
{63,3788},
{65,3784},
{67,3778},
{70,3774},
{72,3769},
{74,3762},
{76,3755},
{78,3747},
{80,3737},
{82,3725},
{84,3714},
{86,3706},
{88,3700},
{91,3695},
{93,3687},
{94,3677},
{95,3659},
{96,3637},
{97,3612},
{98,3589},
{98,3569},
{99,3552},
{99,3538},
{99,3526},
{99,3517},
{99,3510},
{100,3502},
{100,3496},
{100,3491},
{100,3487},
{100,3482},
{100,3400},

};

/* T2 25C */
BATTERY_PROFILE_STRUC battery_profile_t2[] = {
{0,4337},
{2,4310},
{4,4286},
{6,4265},
{8,4244},
{9,4223},
{11,4203},
{13,4183},
{15,4163},
{17,4144},
{19,4125},
{21,4105},
{23,4087},
{24,4074},
{26,4062},
{28,4034},
{30,4004},
{32,3984},
{34,3973},
{36,3966},
{37,3953},
{39,3938},
{41,3921},
{43,3901},
{45,3884},
{47,3869},
{48,3857},
{50,3847},
{52,3838},
{54,3829},
{56,3822},
{58,3814},
{60,3807},
{61,3802},
{63,3796},
{65,3790},
{67,3785},
{69,3781},
{71,3776},
{73,3771},
{74,3768},
{76,3762},
{78,3755},
{80,3748},
{82,3740},
{84,3727},
{86,3714},
{88,3699},
{89,3693},
{91,3690},
{93,3689},
{95,3682},
{97,3628},
{99,3524},
{100,3357},
{100,3318},
{100,3290},
{100,3273},
{100,3260},
{100,3255},
{100,3251},
{100,3251}

};

/* T3 50C */
BATTERY_PROFILE_STRUC battery_profile_t3[] = {
{0,4342},
{2,4317},
{4,4294},
{6,4273},
{7,4252},
{9,4231},
{11,4210},
{13,4190},
{14,4170},
{16,4151},
{18,4131},
{20,4112},
{22,4094},
{24,4075},
{25,4058},
{27,4042},
{29,4024},
{31,4007},
{33,3993},
{34,3978},
{36,3964},
{38,3950},
{40,3937},
{42,3922},
{43,3904},
{45,3883},
{47,3867},
{49,3856},
{51,3845},
{53,3837},
{54,3829},
{56,3820},
{58,3812},
{60,3806},
{62,3799},
{63,3793},
{65,3788},
{67,3783},
{69,3778},
{71,3771},
{73,3759},
{74,3751},
{76,3744},
{78,3736},
{80,3729},
{82,3722},
{83,3713},
{85,3700},
{87,3687},
{89,3678},
{91,3676},
{92,3675},
{94,3671},
{96,3639},
{98,3560},
{100,3426},
{100,3276},
{100,3244},
{100,3235},
{100,3232},
{100,3228},
{100,3228},

};


/* battery profile for actual temperature. The size should be the same as T1, T2 and T3 */
BATTERY_PROFILE_STRUC battery_profile_temperature[] = {
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
};

/* ============================================================ */
/* <Rbat, Battery_Voltage> Table */
/* ============================================================ */
/* T0 -10C */
R_PROFILE_STRUC r_profile_t0[] = {
{3200,4106},
{3200,4060},
{3470,4036},
{3600,4022},
{3680,4009},
{3770,3996},
{3820,3981},
{3920,3963},
{4080,3933},
{4930,3878},
{5740,3846},
{5890,3829},
{5910,3818},
{5950,3808},
{6010,3801},
{5960,3795},
{5900,3790},
{5850,3785},
{5810,3780},
{5750,3775},
{5690,3769},
{5650,3764},
{5600,3759},
{5540,3754},
{5500,3750},
{5460,3746},
{5420,3742},
{5390,3739},
{5360,3736},
{5340,3733},
{5310,3731},
{5290,3728},
{5280,3726},
{5250,3724},
{5230,3723},
{5220,3721},
{5210,3720},
{5200,3719},
{5170,3717},
{5170,3717},
{5170,3716},
{5160,3715},
{5150,3714},
{5150,3714},
{5130,3713},
{5130,3713},
{5130,3712},
{5120,3711},
{5110,3711},
{5120,3711},
{5100,3710},
{5110,3710},
{5100,3710},
{5100,3709},
{5100,3709},
{5090,3708},
{5090,3708},
{5080,3708},
{5080,3707},
{5080,3707},
{5090,3707},
{2010,3400}
};


/* T1 0C */
R_PROFILE_STRUC r_profile_t1[] = {
{1580,4322},
{1580,4297},
{1610,4273},
{1670,4251},
{1720,4228},
{1760,4199},
{1880,4154},
{2270,4115},
{2490,4093},
{2630,4079},
{2630,4055},
{2570,4019},
{2530,3990},
{2580,3969},
{2600,3955},
{2650,3943},
{2640,3929},
{2620,3914},
{2590,3898},
{2570,3883},
{2560,3869},
{2590,3858},
{2570,3847},
{2610,3837},
{2630,3829},
{2690,3822},
{2700,3814},
{2750,3807},
{2770,3801},
{2850,3794},
{2870,3788},
{2930,3784},
{2970,3778},
{3060,3774},
{3140,3769},
{3210,3762},
{3310,3755},
{3420,3747},
{3520,3737},
{3620,3725},
{3730,3714},
{3880,3706},
{4110,3700},
{4460,3695},
{4860,3687},
{4770,3677},
{4600,3659},
{4390,3637},
{4140,3612},
{3900,3589},
{3690,3569},
{3530,3552},
{3390,3538},
{3270,3526},
{3180,3517},
{3110,3510},
{3030,3502},
{2970,3496},
{2920,3491},
{2870,3487},
{2820,3482},
{2030,3400}
};


/* T2 25C */
R_PROFILE_STRUC r_profile_t2[] = {
{460,4337},
{460,4310},
{450,4286},
{460,4265},
{470,4244},
{470,4223},
{490,4203},
{520,4183},
{530,4163},
{560,4144},
{580,4125},
{590,4105},
{620,4087},
{660,4074},
{650,4062},
{660,4034},
{680,4004},
{710,3984},
{750,3973},
{800,3966},
{780,3953},
{770,3938},
{720,3921},
{660,3901},
{630,3884},
{600,3869},
{590,3857},
{570,3847},
{560,3838},
{560,3829},
{570,3822},
{590,3814},
{590,3807},
{600,3802},
{630,3796},
{640,3790},
{650,3785},
{660,3781},
{660,3776},
{650,3771},
{670,3768},
{660,3762},
{650,3755},
{660,3748},
{680,3740},
{670,3727},
{680,3714},
{630,3699},
{640,3693},
{640,3690},
{700,3689},
{790,3682},
{770,3628},
{960,3524},
{1570,3357},
{1190,3318},
{920,3290},
{740,3273},
{620,3260},
{550,3255},
{520,3251},
{540,3251}
};


/* T3 50C */
R_PROFILE_STRUC r_profile_t3[] = {
{280,4342},
{280,4317},
{280,4294},
{280,4273},
{280,4252},
{290,4231},
{300,4210},
{300,4190},
{300,4170},
{310,4151},
{310,4131},
{310,4112},
{330,4094},
{320,4075},
{330,4058},
{350,4042},
{350,4024},
{360,4007},
{370,3993},
{390,3978},
{400,3964},
{420,3950},
{450,3937},
{450,3922},
{410,3904},
{340,3883},
{300,3867},
{300,3856},
{280,3845},
{310,3837},
{310,3829},
{310,3820},
{310,3812},
{320,3806},
{320,3799},
{330,3793},
{340,3788},
{350,3783},
{360,3778},
{350,3771},
{310,3759},
{310,3751},
{300,3744},
{290,3736},
{300,3729},
{300,3722},
{310,3713},
{300,3700},
{310,3687},
{310,3678},
{310,3676},
{340,3675},
{360,3671},
{370,3639},
{500,3560},
{660,3426},
{770,3276},
{460,3244},
{370,3235},
{320,3232},
{300,3228},
{300,3228}
};

/* r-table profile for actual temperature. The size should be the same as T1, T2 and T3 */
R_PROFILE_STRUC r_profile_temperature[] = {
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0}
};

/* ============================================================ */
/* function prototype */
/* ============================================================ */
int fgauge_get_saddles(void);
BATTERY_PROFILE_STRUC_P fgauge_get_profile(kal_uint32 temperature);

int fgauge_get_saddles_r_table(void);
R_PROFILE_STRUC_P fgauge_get_profile_r_table(kal_uint32 temperature);

#endif				/* #ifndef _CUST_BATTERY_METER_TABLE_H */
