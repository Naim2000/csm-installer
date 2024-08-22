#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ogc/consol.h>

#include "menu.h"
#include "pad.h"
#include "video.h"

void DrawHeading(void)
{
	int conX, conY;
	char line[120];
	memset(line, 0xc4, sizeof(line));

	CON_GetMetrics(&conX, &conY);
	// Draw a nice heading.
	clear();
	puts("csm-installer v1.4 by thepikachugamer");
	puts("Super basic menu (2)");
	printf("%.*s", conX, line);
	// putchar('\n');

/*
	// Draw a nicer heading. // Nvm this looks worse
	int width = conX - 3;
	char horizontal[120];
	memset(horizontal, 0xcd, sizeof(horizontal));

	printf(" \xc9%.*s\xbb", width, horizontal);
	printf(" \xcc%.*scsm-installer v1.4 - by thepikachugamer%.*s\xb9", (width - 39) / 2, horizontal, (width - 39) / 2, horizontal); // 39
	printf(" \xc8%.*s\xbc", width, horizontal);
*/
}

void DrawFooter(int controls)
{
	int conX, conY, curX, curY;
	char line[120];
	memset(line, 0xc4, sizeof(line));

	CON_GetMetrics(&conX, &conY);
	CON_GetPosition(&curX, &curY);

	printf("\x1b[%i;0H", conY - 3);
	if (controls) {
		printf("%.2sControls:%.*s", line, conX - 11, line);
		switch (controls) {
			case 1:
				printf("	(A) : Select                               \x12 Up/Down : Move cursor\n");
				printf("	[B] : Nothing                             Home/Start : Return to HBC");
				break;
			case 2:
				printf("	\x1d Left/Right : Change setting              \x12 Up/Down : Move cursor\n");
				printf("	         [B] : Main menu                  Home/Start : Main menu");
		}
	}
	else {
		printf("%.*s", conX, line);
	}

	printf("\x1b[%i;%iH", curY, curX);
}

__attribute__((weak))
void deinitialize(void) { }

void MainMenu(int argc; MainMenuItem argv[argc], int argc)
{
	int x = 0, conX, conY;

	DrawHeading();
	CON_GetPosition(&conX, &conY);
	DrawFooter(1);

	while (true)
	{
		MainMenuItem* item = argv + x;

		printf("\x1b[%i;0H", conY);

		for (MainMenuItem* i = argv; i < argv + argc; i++)
		{
			if (i == item) printf("%s>>  %s\x1b[40m\x1b[39m\n", i->highlight_str ?: "", i->name);
			else printf("    %s\n", i->name);
		}

		switch (wait_button(0))
		{
			case WPAD_BUTTON_UP:
			{
				if (x-- == 0) x = argc - 1;
				break;
			}

			case WPAD_BUTTON_DOWN:
			{
				if (++x == argc) x = 0;
				break;
			}

			case WPAD_BUTTON_A:
			{
				if (!item->action) return;

				clear();
				if (item->heading) { DrawHeading(); DrawFooter(false); }
				item->action();

				if (item->pause) {
					puts("\nPress any button to continue...");
					wait_button(0);
				}

				clear();
				DrawHeading();
				DrawFooter(1);
				break;
			}
/*
			case WPAD_BUTTON_B:
			{
				return;
			}
*/
			case WPAD_BUTTON_HOME:
			{
				// deinitialize();
				// exit(0);
				return;
			}
		}
	}
}

void SettingsMenu(int argc; SettingsItem argv[argc], int argc)
{
	int x = 0, conX, conY;

	DrawHeading();
	CON_GetPosition(&conX, &conY);
	DrawFooter(2);

	while (true)
	{
		SettingsItem* selected = argv + x;
		int* selectedopt = selected->selected; // !!!?

		printf("\x1b[%i;0H", conY);

		for (SettingsItem* i = argv; i < argv + argc; i++)
		{
			clearln();

			if (i == selected)
				printf("	%-40s	%c %s %c\n", i->name, (*i->selected) ? '<':' ', i->options[*i->selected], (((*i->selected) + 1) < i->count) ? '>':' ');
			else
				printf("	%-40s	  %s  \n", i->name, i->options[*i->selected]);
		}

		switch (wait_button(0))
		{
			case WPAD_BUTTON_UP:
			{
				if (x-- == 0) x = argc - 1;
				break;
			}

			case WPAD_BUTTON_DOWN:
			{
				if (++x == argc) x = 0;
				break;
			}

			case WPAD_BUTTON_LEFT:
			{
				if (*selectedopt) (*selectedopt)--;
				break;
			}

			case WPAD_BUTTON_RIGHT:
			{
				if ((*selectedopt) + 1 < selected->count) (*selectedopt)++;
				break;
			}

			case WPAD_BUTTON_A:
			case WPAD_BUTTON_B:
			case WPAD_BUTTON_HOME:
			{
				return;
			}
		}
	}
}
