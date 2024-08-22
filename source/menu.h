typedef const struct
{
    const char* name;
    const char* highlight_str;
    int (*action)(void);
    bool pause;
    bool heading;
} MainMenuItem;

typedef const struct
{
    const char* name;
    const char** options;
    int count;
    int* selected;
} SettingsItem;

void DrawHeading(void);
void DrawFooter(int controls);
void MainMenu(int argc; MainMenuItem argv[argc], int argc);
void SettingsMenu(int argc; SettingsItem argv[argc], int argc);
