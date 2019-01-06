/*
htop - CRT.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "CRT.h"
#include "ColorsPanel.h"

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

int DIREC_BUF;

const char **CRT_treeStr = CRT_treeStrAscii;

static bool CRT_hasColors;

int CRT_delay = 0;

int* CRT_colors;

int **CRT_colorSchemes;  

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

#ifndef CRT_dropPrivileges3
#define CRT_dropPrivileges()
#define CRT_restorePrivileges()
#endif

#endif

// TODO: pass an instance of Settings instead.

void CRT_init(int delay, int colorScheme) {
   
/* Please rip this code apart */  
   
   FILE *fp;
   DIREC_BUF=0;
	int h=0;
	int j=0;
	int k=0;
	int l=0;
	int m=0;

	char *HTOP_PATH={"/.config/htop/"};

	int HTOP_PATH_LEN;
	HTOP_PATH_LEN=strlen(HTOP_PATH);

	char *HOME_PATH;
	HOME_PATH=getenv("HOME");

	int HOME_PATH_LEN;
	HOME_PATH_LEN=strlen(HOME_PATH);

	int HFP_BUF;
	HFP_BUF=(HOME_PATH_LEN + HTOP_PATH_LEN);

	char HTOP_FILE_PATH[HFP_BUF];

	sprintf(HTOP_FILE_PATH, "%s%s", HOME_PATH, HTOP_PATH);

	DIR *DIRECT;
	DIRECT = opendir(HTOP_FILE_PATH);

	if (DIRECT != NULL)
	{
		closedir(DIRECT);
	}

	else

	{
		exit(1);
	}

	struct dirent *entry;

	opendir(HTOP_FILE_PATH);

	while ((entry = readdir(DIRECT)) != NULL)
	{
		if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
		{
			h++;
		}
	}

	rewinddir(DIRECT);

	char *FILE_NAME[h];
	char *FILE_NAME_M[h];
	int FILE_NAME_LEN[h];

	opendir(HTOP_FILE_PATH);

	while ((entry = readdir(DIRECT)) != NULL)
	{
		if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
		{
			int FILE_NAME_BUF=strlen(entry->d_name);
			FILE_NAME[DIREC_BUF] = malloc(FILE_NAME_BUF+1);

			strcpy(FILE_NAME[DIREC_BUF], entry->d_name);
			FILE_NAME_LEN[DIREC_BUF] = FILE_NAME_BUF;
			DIREC_BUF++;
		}
	}

	closedir(DIRECT);

	char FILE_TYPE[]=".theme";
	int FILE_TYPE_LEN = strlen(FILE_TYPE);
   

	for (j=0; j < DIREC_BUF; j++)
	{
		if (FILE_NAME_LEN[j] > FILE_TYPE_LEN)
		{
			char FILE_TYPE_COMP[FILE_TYPE_LEN];
			int OFFSET = FILE_NAME_LEN[j] - FILE_TYPE_LEN;

			sprintf(FILE_TYPE_COMP,"%s", (FILE_NAME[j] + OFFSET));

			int CHECK=strcmp(FILE_TYPE_COMP,FILE_TYPE);

			if (CHECK == 0)
			{
				FILE_NAME_M[k]=FILE_NAME[j];
            ColorSchemeNames[k]=malloc(sizeof(ColorSchemeNames));
            ColorSchemeNames[k]=FILE_NAME_M[k];
				k++;
			}
		}
	}
	
   CRT_colorSchemes=malloc((sizeof(CRT_colorSchemes)*k));
   
	for (l=0; l < k; l++)
	{
      CRT_colorSchemes[l]=malloc((sizeof(int) * LAST_COLORELEMENT));

		int HTOP_FILE_PATH_LEN;
		HTOP_FILE_PATH_LEN=strlen(HTOP_FILE_PATH);

		int FILE_PATH_BUF;
		FILE_PATH_BUF=(HTOP_FILE_PATH_LEN + strlen(FILE_NAME_M[l]));

		char FILE_PATH[FILE_PATH_BUF+1];

		sprintf(FILE_PATH,"%s%s",HTOP_FILE_PATH, FILE_NAME_M[l]);

      /* at 15 character buffer, even with 256 colors initialized and the resulting tens of millions of resulting color pairs, this would cover it. 8 for the characters, 1 for the newline, and 1 for the null. make it dynamic later anyway */
		char FILE_READ_BUF[10]; 

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
   
/* you can stop ripping apart here */
   
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

/* not entirely sure what this does. looks like it makes a new color, one would assume bg is background but... */   
   
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

   int grayBlackFg = COLORS > 8 ? 8 : 0;
   int grayBlackBg = 0;
   
   init_pair(ColorIndexGrayBlack, grayBlackFg, grayBlackBg);

   CRT_colors = CRT_colorSchemes[colorScheme];
}
