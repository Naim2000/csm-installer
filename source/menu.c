#include <stdio.h>
#include <stdlib.h>
#include <ogc/consol.h>

#include "menu.h"
#include "pad.h"
#include "video.h"

void DrawHeading(void)
{
	int conX, conY;
	CON_GetMetrics(&conX, &conY);

	// Draw a nice heading.
	puts("csm-installer v1.4 by thepikachugamer");
	puts("Super basic menu (2)");
	for (int i = 0; i < conX; i++) putchar(0xc4);
}

__attribute__((weak))
void deinitialize(void) { }

void MainMenu(int argc; MainMenuItem argv[argc], int argc)
{
	int x = 0, conX, conY;

	clear();
	DrawHeading();
	CON_GetPosition(&conX, &conY);

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
				if (item->heading) DrawHeading();
				item->action();

				if (item->pause) {
					puts("\nPress any button to continue...");
					wait_button(0);
				}

				clear();
				DrawHeading();
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
				deinitialize();
				exit(0);
			}
		}
	}
}

void SettingsMenu(int argc; SettingsItem argv[argc], int argc)
{
	int x = 0, conX, conY;

	clear();
	DrawHeading();
	CON_GetPosition(&conX, &conY);

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
