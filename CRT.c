/*
htop - CRT.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "CRT.h"
#include "Settings.h"

#include "StringUtils.h"
#include "RichString.h"

#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <langinfo.h>
#if HAVE_SETUID_ENABLED
#include <unistd.h>
#include <sys/types.h>
#endif

#define ColorIndex(i,j) ((7-i)*8+j)

#define ColorPair(i,j) COLOR_PAIR(ColorIndex(i,j))

#define Black COLOR_BLACK
#define Red COLOR_RED
#define Green COLOR_GREEN
#define Yellow COLOR_YELLOW
#define Blue COLOR_BLUE
#define Magenta COLOR_MAGENTA
#define Cyan COLOR_CYAN
#define White COLOR_WHITE

#define ColorPairGrayBlack ColorPair(Magenta,Magenta)
#define ColorIndexGrayBlack ColorIndex(Magenta,Magenta)

#define KEY_WHEELUP KEY_F(20)
#define KEY_WHEELDOWN KEY_F(21)
#define KEY_RECLICK KEY_F(22)

//#link curses

/*{
#include <stdbool.h>

typedef enum TreeStr_ {
   TREE_STR_HORZ,
   TREE_STR_VERT,
   TREE_STR_RTEE,
   TREE_STR_BEND,
   TREE_STR_TEND,
   TREE_STR_OPEN,
   TREE_STR_SHUT,
   TREE_STR_COUNT
} TreeStr;

typedef enum ColorElements_ {
   RESET_COLOR,
   DEFAULT_COLOR,
   FUNCTION_BAR,
   FUNCTION_KEY,
   FAILED_SEARCH,
   PANEL_HEADER_FOCUS,
   PANEL_HEADER_UNFOCUS,
   PANEL_SELECTION_FOCUS,
   PANEL_SELECTION_FOLLOW,
   PANEL_SELECTION_UNFOCUS,
   LARGE_NUMBER,
   METER_TEXT,
   METER_VALUE,
   LED_COLOR,
   UPTIME,
   BATTERY,
   TASKS_RUNNING,
   SWAP,
   PROCESS,
   PROCESS_SHADOW,
   PROCESS_TAG,
   PROCESS_MEGABYTES,
   PROCESS_TREE,
   PROCESS_R_STATE,
   PROCESS_D_STATE,
   PROCESS_BASENAME,
   PROCESS_HIGH_PRIORITY,
   PROCESS_LOW_PRIORITY,
   PROCESS_THREAD,
   PROCESS_THREAD_BASENAME,
   BAR_BORDER,
   BAR_SHADOW,
   GRAPH_1,
   GRAPH_2,
   MEMORY_USED,
   MEMORY_BUFFERS,
   MEMORY_BUFFERS_TEXT,
   MEMORY_CACHE,
   LOAD,
   LOAD_AVERAGE_FIFTEEN,
   LOAD_AVERAGE_FIVE,
   LOAD_AVERAGE_ONE,
   CHECK_BOX,
   CHECK_MARK,
   CHECK_TEXT,
   CLOCK,
   HELP_BOLD,
   HOSTNAME,
   CPU_NICE,
   CPU_NICE_TEXT,
   CPU_NORMAL,
   CPU_KERNEL,
   CPU_IOWAIT,
   CPU_IRQ,
   CPU_SOFTIRQ,
   CPU_STEAL,
   CPU_GUEST,
   LAST_COLORELEMENT
} ColorElements;

void CRT_fatalError(const char* note) __attribute__ ((noreturn));

void CRT_handleSIGSEGV(int sgn);

#define KEY_ALT(x) (KEY_F(64 - 26) + (x - 'A'))

}*/

const char *CRT_treeStrAscii[TREE_STR_COUNT] = {
   "-", // TREE_STR_HORZ
   "|", // TREE_STR_VERT
   "`", // TREE_STR_RTEE
   "`", // TREE_STR_BEND
   ",", // TREE_STR_TEND
   "+", // TREE_STR_OPEN
   "-", // TREE_STR_SHUT
};

#ifdef HAVE_LIBNCURSESW

const char *CRT_treeStrUtf8[TREE_STR_COUNT] = {
   "\xe2\x94\x80", // TREE_STR_HORZ ─
   "\xe2\x94\x82", // TREE_STR_VERT │
   "\xe2\x94\x9c", // TREE_STR_RTEE ├
   "\xe2\x94\x94", // TREE_STR_BEND └
   "\xe2\x94\x8c", // TREE_STR_TEND ┌
   "+",            // TREE_STR_OPEN +
   "\xe2\x94\x80", // TREE_STR_SHUT ─
};

bool CRT_utf8 = false;

#endif

const char **CRT_treeStr = CRT_treeStrAscii;

static bool CRT_hasColors;

int CRT_delay = 0;

int* CRT_colors;
                                       /* these are mine from here*/
int **CRT_colorSchemes;
                                       /* to here */
int CRT_cursorX = 0;

int CRT_scrollHAmount = 5;

int CRT_scrollWheelVAmount = 10;

char* CRT_termType;

// TODO move color scheme to Settings, perhaps?

int CRT_colorScheme = 0;

void *backtraceArray[128];

static void CRT_handleSIGTERM(int sgn) {
   (void) sgn;
   CRT_done();
   exit(0);
}

#if HAVE_SETUID_ENABLED

static int CRT_euid = -1;

static int CRT_egid = -1;

#define DIE(msg) do { CRT_done(); fprintf(stderr, msg); exit(1); } while(0)

void CRT_dropPrivileges() {
   CRT_egid = getegid();
   CRT_euid = geteuid();
   if (setegid(getgid()) == -1) {
      DIE("Fatal error: failed dropping group privileges.\n");
   }
   if (seteuid(getuid()) == -1) {
      DIE("Fatal error: failed dropping user privileges.\n");
   }
}

void CRT_restorePrivileges() {
   if (CRT_egid == -1 || CRT_euid == -1) {
      DIE("Fatal error: internal inconsistency.\n");
   }
   if (setegid(CRT_egid) == -1) {
      DIE("Fatal error: failed restoring group privileges.\n");
   }
   if (seteuid(CRT_euid) == -1) {
      DIE("Fatal error: failed restoring user privileges.\n");
   }
}

#else

/* Turn setuid operations into NOPs */

#ifndef CRT_dropPrivileges
#define CRT_dropPrivileges()
#define CRT_restorePrivileges()
#endif

#endif

// TODO: pass an instance of Settings instead.

void CRT_init(int delay, int colorScheme) {
   
   USER_THEME_ASSIGN();
   DEFAULT_THEME();

   initscr();
   noecho();
   CRT_delay = delay;
   if (CRT_delay == 0) {
      CRT_delay = 1;
   }
   CRT_colors = CRT_colorSchemes[colorScheme];
   CRT_colorScheme = colorScheme;
   
   halfdelay(CRT_delay);
   nonl();
   intrflush(stdscr, false);
   keypad(stdscr, true);
   mouseinterval(0);
   curs_set(0);
   if (has_colors()) {
      start_color();
      CRT_hasColors = true;
   } else {
      CRT_hasColors = false;
   }
   CRT_termType = getenv("TERM");
   if (String_eq(CRT_termType, "linux"))
      CRT_scrollHAmount = 20;
   else
      CRT_scrollHAmount = 5;
   if (String_startsWith(CRT_termType, "xterm") || String_eq(CRT_termType, "vt220")) {
      define_key("\033[H", KEY_HOME);
      define_key("\033[F", KEY_END);
      define_key("\033[7~", KEY_HOME);
      define_key("\033[8~", KEY_END);
      define_key("\033OP", KEY_F(1));
      define_key("\033OQ", KEY_F(2));
      define_key("\033OR", KEY_F(3));
      define_key("\033OS", KEY_F(4));
      define_key("\033[11~", KEY_F(1));
      define_key("\033[12~", KEY_F(2));
      define_key("\033[13~", KEY_F(3));
      define_key("\033[14~", KEY_F(4));
      define_key("\033[17;2~", KEY_F(18));
      char sequence[3] = "\033a";
      for (char c = 'a'; c <= 'z'; c++) {
         sequence[1] = c;
         define_key(sequence, KEY_ALT('A' + (c - 'a')));
      }
   }
#ifndef DEBUG
   signal(11, CRT_handleSIGSEGV);
#endif
   signal(SIGTERM, CRT_handleSIGTERM);
   signal(SIGQUIT, CRT_handleSIGTERM);
   use_default_colors();
   if (!has_colors())
      CRT_colorScheme = 1;
   CRT_setColors(CRT_colorScheme);

   /* initialize locale */
   setlocale(LC_CTYPE, "");

#ifdef HAVE_LIBNCURSESW
   if(strcmp(nl_langinfo(CODESET), "UTF-8") == 0)
      CRT_utf8 = true;
   else
      CRT_utf8 = false;
#endif

   CRT_treeStr =
#ifdef HAVE_LIBNCURSESW
      CRT_utf8 ? CRT_treeStrUtf8 :
#endif
      CRT_treeStrAscii;

#if NCURSES_MOUSE_VERSION > 1
   mousemask(BUTTON1_RELEASED | BUTTON4_PRESSED | BUTTON5_PRESSED, NULL);
#else
   mousemask(BUTTON1_RELEASED, NULL);
#endif

}

void CRT_done() {
   curs_set(1);
   endwin();
}

void CRT_fatalError(const char* note) {
   char* sysMsg = strerror(errno);
   CRT_done();
   fprintf(stderr, "%s: %s\n", note, sysMsg);
   exit(2);
}

int CRT_readKey() {
   nocbreak();
   cbreak();
   nodelay(stdscr, FALSE);
   int ret = getch();
   halfdelay(CRT_delay);
   return ret;
}

void CRT_disableDelay() {
   nocbreak();
   cbreak();
   nodelay(stdscr, TRUE);
}

void CRT_enableDelay() {
   halfdelay(CRT_delay);
}

void CRT_setColors(int colorScheme) {
   CRT_colorScheme = colorScheme;

/* goes through every color pair and makes all the black backgrounds -1 -- transparent? */   

   for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 8; j++) {
         if (ColorIndex(i,j) != ColorPairGrayBlack) {
            int bg = (j==0)
                     ? -1
                     : j;
            init_pair(ColorIndex(i,j), i, bg);
         }
      }
   }
   
   
   /* except for grayblack, which gets set to black/black if theres more than 8 colors, and and white (gray?) / black otherwise */
  
   int grayBlackFg = COLORS > 8 ? 8 : 0;
   int grayBlackBg = -1;
   
   init_pair(ColorIndexGrayBlack, grayBlackFg, grayBlackBg);

   CRT_colors = CRT_colorSchemes[colorScheme];
}

void USER_THEME_ASSIGN(void) {
   
   FILE *fp;
   int l = 0;
   int m = 0;
   CRT_colorSchemes=malloc((sizeof(CRT_colorSchemes)*(LAST_COLOR_SCHEME_ENTRY+1)));   // +1 for the last entry itself (default, now)
   
	for (l=0; l < LAST_COLOR_SCHEME_ENTRY; l++) //redo how memory is allocated here
	{
      CRT_colorSchemes[l]=malloc((sizeof(int) * LAST_COLORELEMENT));

		int HTOP_FILE_PATH_LEN;
		HTOP_FILE_PATH_LEN=strlen(htopDir);

		int FILE_PATH_BUF;
		FILE_PATH_BUF=(HTOP_FILE_PATH_LEN + strlen(FILE_NAME_M[l]));

		char FILE_PATH[FILE_PATH_BUF+1];

		sprintf(FILE_PATH,"%s%s", htopDir, FILE_NAME_M[l]);

		char FILE_READ_BUF[12]; //this should be adequate-- even the highest shifted attribute (left shift 30) only gives values into the single billions. this gives 10 elements for the characters, 1 for the newline, and 1 for the null.

		fp=fopen(FILE_PATH, "r");

		if (fp != NULL)
		{

			for (m=0; m < LAST_COLORELEMENT; m++)
			{
				fgets(FILE_READ_BUF, sizeof FILE_READ_BUF, fp);
				CRT_colorSchemes[l][m]=(atoi(FILE_READ_BUF));
			}
		}

		fclose(fp);
	}
}

void DEFAULT_THEME() {
   
   char *DEFAULT_THEME_NAME={"Default"};
   FILE_NAME_M[LAST_COLOR_SCHEME_ENTRY]=DEFAULT_THEME_NAME;
   
	CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY]=malloc((sizeof(int) * LAST_COLORELEMENT));
   
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][0]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][1]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][2]=262144;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][3]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][4]=2359296;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][5]=262144;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][6]=262144;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][7]=262144;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][8]=262144;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][9]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][10]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][11]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][12]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][13]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][14]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][15]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][16]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][17]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][18]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][19]=1048576;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][20]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][21]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][22]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][23]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][24]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][25]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][26]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][27]=1048576;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][28]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][29]=262144;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][30]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][31]=1048576;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][32]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][33]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][34]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][35]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][36]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][37]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][38]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][39]=1048576;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][40]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][41]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][42]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][43]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][44]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][45]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][46]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][47]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][48]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][49]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][50]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][51]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][52]=0;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][53]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][54]=2097152;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][55]=262144;
   CRT_colorSchemes[LAST_COLOR_SCHEME_ENTRY][56]=262144;
}
