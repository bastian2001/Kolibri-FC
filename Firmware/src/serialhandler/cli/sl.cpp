/**
 * @file sl.cpp
 * @brief Implementation of the sl command
 *
 * Copyright (c) 2026 Kolibri-FC contributors
 *
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 *
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

//
//  Copyright 1993,1998,2014 Toyoda Masashi (mtoyoda@acm.org)
// @(#)sl.1
//

/*========================================
 *    sl.c: SL version 5.05
 *        Copyright 1993,1998,2014-2015,2019
 *                  Toyoda Masashi
 *                  (mtoyoda@acm.org)
 *        Last Modified: 2019/03/19
 *========================================
 */
/* sl version 5.05 : by eyJhb 2019-08-14
 * - Merged pull request from cosmo-ray adding orange TGV (-G)
 * sl version 5.04 : by eyJhb 2019-03-19
 * - Merged pull request from EverNine allowing to specify the number of cars
 * - Merged pull request from ddugovic which adds
 *      `-e` for `ctrl+c` to work
 *      `-d` to enable disco mode
 *      `-w` making the locomotive go faster
 */
/* sl version 5.03 : Fix some more compiler warnings.                        */
/*                                              by Ryan Jacobs    2015/01/19 */
/* sl version 5.02 : Fix compiler warnings.                                  */
/*                                              by Jeff Schwab    2014/06/03 */
/* sl version 5.01 : removed cursor and handling of IO                       */
/*                                              by Chris Seymour  2014/01/03 */
/* sl version 5.00 : add -c option                                           */
/*                                              by Toyoda Masashi 2013/05/05 */
/* sl version 4.00 : add C51, usleep(40000)                                  */
/*                                              by Toyoda Masashi 2002/12/31 */
/* sl version 3.03 : add usleep(20000)                                       */
/*                                              by Toyoda Masashi 1998/07/22 */
/* sl version 3.02 : D51 flies! Change options.                              */
/*                                              by Toyoda Masashi 1993/01/19 */
/* sl version 3.01 : Wheel turns smoother                                    */
/*                                              by Toyoda Masashi 1992/12/25 */
/* sl version 3.00 : Add d(D51) option                                       */
/*                                              by Toyoda Masashi 1992/12/24 */
/* sl version 2.02 : Bug fixed.(dust remains in screen)                      */
/*                                              by Toyoda Masashi 1992/12/17 */
/* sl version 2.01 : Smoke run and disappear.                                */
/*                   Change '-a' to accident option.                         */
/*                                              by Toyoda Masashi 1992/12/16 */
/* sl version 2.00 : Add a(all),l(long),F(Fly!) options.                     */
/*                                              by Toyoda Masashi 1992/12/15 */
/* sl version 1.02 : Add turning wheel.                                      */
/*                                              by Toyoda Masashi 1992/12/14 */
/* sl version 1.01 : Add more complex smoke.                                 */
/*                                              by Toyoda Masashi 1992/12/14 */
/* sl version 1.00 : SL runs vomiting out smoke.                             */
/*                                              by Toyoda Masashi 1992/12/11 */

#include "sl.h"
#include "global.h"

#define VERSION "5.04"

void add_smoke(int y, int x, int action = 0);
void add_man(int y, int x);
int add_C51(int x);
int add_D51(int x);
int add_TGV(int x);
int add_sl(int x);
int my_mvaddstr(int y, int x, const char *str);

static int ACCIDENT = 0;
static int C51 = 0;
static int NUMBER = 2;
static int DISCO = 0;
static int FLY = 0;
static int LOGO = 0;
static int WIND = 0;
static int TGV = 0;

static int x;
static int base_usleep = 40000;

static char *frameBuf = nullptr;
static u32 slWidth = 50;
static u32 slHeight = 30;
static elapsedMicros slTimer = 0;

static bool cleanup(Command *cmd) {
	add_smoke(0, 0, 2);
	cmd->print("\x0e");
	if (frameBuf) free(frameBuf);
	frameBuf = nullptr;
	return false;
}
static void cleanup2(Command *cmd) {
	cleanup(cmd);
}
static void prepFrameBuf(Command *cmd) {
	if (frameBuf) free(frameBuf);
	slWidth = cmd->getWidth();
	slHeight = cmd->getHeight();
	if (slWidth > 300) slWidth = 300;
	if (slHeight > 100) slHeight = 100;
	frameBuf = (char *)malloc(slWidth * slHeight);
	if (!frameBuf) return;
	memset(frameBuf, 0, slWidth * slHeight);
}

void initSl() {
	Command *cmd = new Command("sl", "You spelt ls wrong");

	cmd->addFlagArg("accident", 'a', "An accident is occurring. People cry for help.");
	cmd->addFlagArg("c51", 'c', "C51 appears instead of D51.");
	cmd->addFlagArg("disco", 'd', "Disco mode. Alternate colors during animation.");
	cmd->addFlagArg("fly", 'f', "It flies like the galaxy express 999.");
	cmd->addFlagArg("TGV", 'g', "It's French like Oscar.");
	cmd->addIntArg("num_cars", 'n', true, false, 0, 255, "Specify the number of cars like `sl -n 5`.", "1");
	cmd->addFlagArg("little", 'l', "Little version.");
	cmd->addFlagArg("windy", 'w', "Windy day. Locomotive moves faster.");
	cmd->addFlagArg("version", 'v', "Prints the version and last updated.");

	cmd->setExecuteFunction([](std::map<string, RuntimeArg> &args, Command *cmd) {
		if (std::get<bool>(args["version"].value)) {
			cmd->print("Version: " VERSION ", last updated: 2019-03-19\n");
			return false;
		}
		ACCIDENT = 0;
		C51 = 0;
		NUMBER = 2;
		DISCO = 0;
		FLY = 0;
		LOGO = 0;
		WIND = 0;
		TGV = 0;
		if (std::get<bool>(args["accident"].value))
			ACCIDENT = 1;
		if (std::get<bool>(args["c51"].value))
			C51 = 1;
		if (std::get<bool>(args["disco"].value))
			DISCO = 0;
		if (std::get<bool>(args["fly"].value))
			FLY = 1;
		if (std::get<bool>(args["TGV"].value))
			TGV = 0;
		if (std::get<bool>(args["little"].value))
			LOGO = 1;
		if (std::get<bool>(args["windy"].value))
			WIND = 200;
		NUMBER = std::get<int>(args["num_cars"].value);
		cmd->print("\x0f\x02");

		prepFrameBuf(cmd);
		if (!frameBuf) return false;

		add_smoke(0, 0, 1);

		base_usleep = 40000;
		if (TGV == 1) {
			base_usleep /= 2;
			if (WIND) WIND = 150;
		}
		x = slWidth - 1;
		return true;
	});

	cmd->setLoopFunction([](Command *cmd) {
		if (!frameBuf) return false;
		if (slTimer < base_usleep - WIND * 100) return true;
		slTimer = 0;
		if (LOGO == 1) {
			if (add_sl(x) == 1) return cleanup(cmd);
		} else if (C51 == 1) {
			if (add_C51(x) == 1) return cleanup(cmd);
		} else if (TGV == 1) {
			if (add_TGV(x) == 1) return cleanup(cmd);
		} else {
			if (add_D51(x) == 1) return cleanup(cmd);
		}
		cmd->print("\x03\x02\v");
		char s[slWidth + 2];
		s[slWidth] = '\n';
		s[slWidth + 1] = '\0';
		for (int i = 0; i < slHeight; i++) {
			memcpy(s, frameBuf + i * slWidth, slWidth);
			bool allSpace = true;
			for (int j = 0; j < slWidth; j++) {
				if (s[j] == '\0') s[j] = ' ';
				if (s[j] != ' ') {
					allSpace = false;
				}
			}
			if (allSpace)
				cmd->print(" \n");
			else
				cmd->print(s);
		}
		x--;
		return true;
	});

	cmd->setResizeFunction(prepFrameBuf);

	cmd->setAbortFunction(cleanup2);

	Command::cliCommands.push_back(cmd);
}

int my_mvaddstr(int y, int x, const char *str) {
	if (!frameBuf || y >= slHeight || y < 0) return 1;
	for (; x < 0; ++x, ++str)
		if (*str == '\0') return 1;
	int pos = y * slWidth + x;
	for (; *str != '\0'; ++str, ++x, ++pos) {
		if (x < slWidth)
			frameBuf[pos] = *str;
		else
			break;
	}
	return 0;
}

int add_sl(int x) {
	const char *sl[LOGOPATTERNS][LOGOHEIGHT + 1] = {{LOGO1, LOGO2, LOGO3, LOGO4, LWHL11, LWHL12, DELLN},
													{LOGO1, LOGO2, LOGO3, LOGO4, LWHL21, LWHL22, DELLN},
													{LOGO1, LOGO2, LOGO3, LOGO4, LWHL31, LWHL32, DELLN},
													{LOGO1, LOGO2, LOGO3, LOGO4, LWHL41, LWHL42, DELLN},
													{LOGO1, LOGO2, LOGO3, LOGO4, LWHL51, LWHL52, DELLN},
													{LOGO1, LOGO2, LOGO3, LOGO4, LWHL61, LWHL62, DELLN}};

	const char *coal[LOGOHEIGHT + 1] = {LCOAL1, LCOAL2, LCOAL3, LCOAL4, LCOAL5, LCOAL6, DELLN};

	const char *car[LOGOHEIGHT + 1] = {LCAR1, LCAR2, LCAR3, LCAR4, LCAR5, LCAR6, DELLN};

	if (NUMBER < 0)
		NUMBER = 2;

	int i, j, y, py = 0;
	int LOGOLENGTH = 42 + 21 * NUMBER;

	if (x < -LOGOLENGTH) return 1;
	y = slHeight / 2 - 3;

	if (FLY == 1) {
		y = (x / 6) + slHeight - (slWidth / 6) - LOGOHEIGHT;
		py = 2;
	}
	for (i = 0; i <= LOGOHEIGHT; ++i) {
		my_mvaddstr(y + i, x, sl[(LOGOLENGTH + x) / 3 % LOGOPATTERNS][i]);
		my_mvaddstr(y + i + py, x + 21, coal[i]);
		for (j = 2; j <= NUMBER + 1; ++j)
			my_mvaddstr(y + i + py * j, x + 21 * j, car[i]);
	}
	if (ACCIDENT == 1) {
		add_man(y + 1, x + 14);
		for (j = 2; j <= NUMBER + 1; ++j) {
			add_man(y + 1 + py * j, x + 3 + 21 * j);
			add_man(y + 1 + py * j, x + 11 + 21 * j);
		}
	}
	add_smoke(y - 1, x + LOGOFUNNEL);
	return 0;
}
int add_D51(int x) {
	const char *d51[D51PATTERNS][D51HEIGHT + 1] = {{D51STR1, D51STR2, D51STR3, D51STR4, D51STR5, D51STR6, D51STR7,
													D51WHL11, D51WHL12, D51WHL13, D51DEL},
												   {D51STR1, D51STR2, D51STR3, D51STR4, D51STR5, D51STR6, D51STR7,
													D51WHL21, D51WHL22, D51WHL23, D51DEL},
												   {D51STR1, D51STR2, D51STR3, D51STR4, D51STR5, D51STR6, D51STR7,
													D51WHL31, D51WHL32, D51WHL33, D51DEL},
												   {D51STR1, D51STR2, D51STR3, D51STR4, D51STR5, D51STR6, D51STR7,
													D51WHL41, D51WHL42, D51WHL43, D51DEL},
												   {D51STR1, D51STR2, D51STR3, D51STR4, D51STR5, D51STR6, D51STR7,
													D51WHL51, D51WHL52, D51WHL53, D51DEL},
												   {D51STR1, D51STR2, D51STR3, D51STR4, D51STR5, D51STR6, D51STR7,
													D51WHL61, D51WHL62, D51WHL63, D51DEL}};
	const char *coal[D51HEIGHT + 1] = {COAL01, COAL02, COAL03, COAL04, COAL05,
									   COAL06, COAL07, COAL08, COAL09, COAL10, COALDEL};

	if (NUMBER < 0)
		NUMBER = 1;

	int y, i, j, dy = 0;
	int D51LENGTH = 54 + 29 * NUMBER;

	if (x < -D51LENGTH) return 1;
	y = slHeight / 2 - 5;

	if (FLY == 1) {
		y = (x / 7) + slHeight - (slWidth / 7) - D51HEIGHT;
		dy = 1;
	}
	for (i = 0; i <= D51HEIGHT; ++i) {
		my_mvaddstr(y + i, x, d51[(D51LENGTH + x) % D51PATTERNS][i]);
		for (j = 1; j <= NUMBER; ++j)
			my_mvaddstr(y + i + dy * j, x + 24 + 29 * j, coal[i]);
	}
	if (ACCIDENT == 1) {
		add_man(y + 2, x + 43);
		add_man(y + 2, x + 47);
	}
	add_smoke(y - 1, x + D51FUNNEL);
	return 0;
}

int add_TGV(int x) {
	const char *tgv[TGVPATTERNS][TGVHEIGHT + 1] = {{TGVSTR0, TGVSTR1, TGVSTR2, TGVSTR3, TGVSTR4, TGVSTR5, TGVSTR6,
													TGVWHL1, TGVDEL},
												   {TGVSTR0, TGVSTR1, TGVSTR2, TGVSTR3, TGVSTR4, TGVSTR5, TGVSTR6,
													TGVWHL2, TGVDEL}};
	const char *vagoon[TGVHEIGHT + 1] = {TGVVAG0, TGVVAG1, TGVVAG2, TGVVAG3, TGVVAG4, TGVVAG5, TGVVAG6, TGVVAG7, TGVDEL};

	int y, i, dy = 0;

	if (x < -TGVLENGTH) return 1;
	y = slHeight / 2 - 5;

	if (FLY == 1) {
		y = (x / 7) + slHeight - (slWidth / 7) - TGVHEIGHT;
		dy = 1;
	}
	// attron(COLOR_PAIR(2));
	for (i = 0; i <= TGVHEIGHT; ++i) {
		my_mvaddstr(y + i, x, tgv[((TGVLENGTH + x) / 2) % TGVPATTERNS][i]);
		my_mvaddstr(y + i + dy, x + 55, vagoon[i]);
	}
	// attroff(COLOR_PAIR(2));

	if (ACCIDENT == 1) {
		add_man(y + 2, x + 14);

		add_man(y + dy + 3, x + 85);
		add_man(y + dy + 3, x + 90);
		add_man(y + dy + 3, x + 95);
		add_man(y + dy + 3, x + 100);
	}
	return 0;
}

int add_C51(int x) {
	const char *c51[C51PATTERNS][C51HEIGHT + 1] = {{C51STR1, C51STR2, C51STR3, C51STR4, C51STR5, C51STR6, C51STR7,
													C51WH11, C51WH12, C51WH13, C51WH14, C51DEL},
												   {C51STR1, C51STR2, C51STR3, C51STR4, C51STR5, C51STR6, C51STR7,
													C51WH21, C51WH22, C51WH23, C51WH24, C51DEL},
												   {C51STR1, C51STR2, C51STR3, C51STR4, C51STR5, C51STR6, C51STR7,
													C51WH31, C51WH32, C51WH33, C51WH34, C51DEL},
												   {C51STR1, C51STR2, C51STR3, C51STR4, C51STR5, C51STR6, C51STR7,
													C51WH41, C51WH42, C51WH43, C51WH44, C51DEL},
												   {C51STR1, C51STR2, C51STR3, C51STR4, C51STR5, C51STR6, C51STR7,
													C51WH51, C51WH52, C51WH53, C51WH54, C51DEL},
												   {C51STR1, C51STR2, C51STR3, C51STR4, C51STR5, C51STR6, C51STR7,
													C51WH61, C51WH62, C51WH63, C51WH64, C51DEL}};
	const char *coal[C51HEIGHT + 1] = {COALDEL, COAL01, COAL02, COAL03, COAL04, COAL05,
									   COAL06, COAL07, COAL08, COAL09, COAL10, COALDEL};

	if (NUMBER < 0)
		NUMBER = 1;

	int y, i, j, dy = 0;
	int C51LENGTH = 58 + 29 * NUMBER;

	if (x < -C51LENGTH) return 1;
	y = slHeight / 2 - 5;

	if (FLY == 1) {
		y = (x / 7) + slHeight - (slWidth / 7) - C51HEIGHT;
		dy = 1;
	}
	for (i = 0; i <= C51HEIGHT; ++i) {
		my_mvaddstr(y + i, x, c51[(C51LENGTH + x) % C51PATTERNS][i]);
		for (j = 1; j <= NUMBER; j++)
			my_mvaddstr(y + i + dy * j, x + 26 + 29 * j, coal[i]);
	}
	if (ACCIDENT == 1) {
		add_man(y + 3, x + 45);
		add_man(y + 3, x + 49);
	}
	add_smoke(y - 1, x + C51FUNNEL);
	return 0;
}

void add_man(int y, int x) {
	const char *man[2][2] = {{"", "(O)"}, {"Help!", "\\O/"}};
	int i;

	for (i = 0; i < 2; ++i) {
		my_mvaddstr(y + i, x, man[(42 + 21 * NUMBER + x) / 12 % 2][i]);
	}
}

void add_smoke(int y, int x, int action)
#define SMOKEPTNS 16
{
	struct smokes {
		i16 y, x;
		i16 ptrn, kind;
	};
	static struct smokes *S;
	static int sum = 0;
	if (action == 0) {
		// normal draw smoke
	} else if (action == 1) {
		// first call
		if (S) free(S);
		S = (smokes *)malloc(1000 * sizeof(smokes));
		sum = 0;
		if (!S) return;
		memset(S, 0, 1000 * sizeof(smokes));
		return;
	} else if (action == 2) {
		// end
		if (S) free(S);
		S = nullptr;
		return;
	}
	if (!S) return;
	const char *Smoke[2][SMOKEPTNS] = {{"(   )", "(    )", "(    )", "(   )", "(  )",
										"(  )", "( )", "( )", "()", "()",
										"O", "O", "O", "O", "O",
										" "},
									   {"(@@@)", "(@@@@)", "(@@@@)", "(@@@)", "(@@)",
										"(@@)", "(@)", "(@)", "@@", "@@",
										"@", "@", "@", "@", "@",
										" "}};
	const char *Eraser[SMOKEPTNS] = {"     ", "      ", "      ", "     ", "    ",
									 "    ", "   ", "   ", "  ", "  ",
									 " ", " ", " ", " ", " ",
									 " "};
	const int dy[SMOKEPTNS] = {2, 1, 1, 1, 0, 0, 0, 0, 0, 0,
							   0, 0, 0, 0, 0, 0};
	const int dx[SMOKEPTNS] = {-2, -1, 0, 1, 1, 1, 1, 1, 2, 2,
							   2, 2, 2, 3, 3, 3};
	int i;

	if (DISCO && (x + INT_MAX / 2) % 4 == 2) {
	}
	// attron(COLOR_PAIR((x + INT_MAX / 2) / 16 % 4 + 1));
	if (x % 4 == 0) {
		for (i = 0; i < sum; ++i) {
			my_mvaddstr(S[i].y, S[i].x, Eraser[S[i].ptrn]);
			S[i].y -= dy[S[i].ptrn];
			S[i].x += dx[S[i].ptrn];
			S[i].ptrn += (S[i].ptrn < SMOKEPTNS - 1) ? 1 : 0;
			my_mvaddstr(S[i].y, S[i].x, Smoke[S[i].kind][S[i].ptrn]);
		}
		my_mvaddstr(y, x, Smoke[sum % 2][0]);
		S[sum].y = y;
		S[sum].x = x;
		S[sum].ptrn = 0;
		S[sum].kind = sum % 2;
		sum++;
	}
}
