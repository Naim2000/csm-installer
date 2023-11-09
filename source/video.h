#define clear() printf("\x1b[2J")
#define CONSOLE_HEIGHT		(480-32)
#define CONSOLE_WIDTH		(640)

[[gnu::constructor]] void init_video();
