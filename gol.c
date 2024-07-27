#define U8  unsigned char
#define U16 unsigned short
#define I16   signed short

#define ALIVE_TEXT "##"
#define  DEAD_TEXT ". "

static U8 mem_region[16*4096] = {0};

static void write_stdout(char* buf, U16 buf_size);
static void sleep_fractional(float secs);
static void clear_terminal(void);

void _start(void) {
    U16 w = 20;
    U16 h = 30;

    static const U8 GLIDER[3][3] = {
        {0, 1, 0},
        {0, 0, 1},
        {1, 1, 1},
    };

    U8 (*bufs)[h][w] = (U8 (*)[h][w]) mem_region;
    U8 buf_idx_toggle = 0;
    #define OLD_BUF bufs[buf_idx_toggle^1]
    #define NEW_BUF bufs[buf_idx_toggle  ]
    for (U16 y = 0; y < 3; ++y)
        for (U16 x = 0; x < 3; ++x)
            OLD_BUF[y][x] = GLIDER[y][x];

    char* text_buf = (char*) mem_region + 2*w*h;

    while (1) {
        char* text_ptr = text_buf;

        for (I16 y = 0; y < h; ++y) {
            for (I16 x = 0; x < w; ++x) {
                U8 live_neighbors = 0;
                for (I16 ny = y-1; ny <= y+1; ++ny)
                    for (I16 nx = x-1; nx <= x+1; ++nx)
                        live_neighbors += OLD_BUF[(ny+h) % h][(nx+w) % w];
                live_neighbors -= OLD_BUF[y][x];

                NEW_BUF[y][x] =
                    live_neighbors == 3 ||
                    live_neighbors == 2 && OLD_BUF[y][x];

                char* cell_text = OLD_BUF[y][x] ? ALIVE_TEXT : DEAD_TEXT;
                while (*cell_text != '\0')
                    *text_ptr++ = *cell_text++;
            }
            *text_ptr++ = '\n';
        }

        clear_terminal();
        write_stdout(text_buf, text_ptr - text_buf);

        buf_idx_toggle ^= 1;
        sleep_fractional(0.1);
    }
}

static void write_stdout(char* buf, U16 buf_size) {
    #ifdef LINUX_64_BIT
        int dummy_ret;
        asm volatile(
            "syscall"
            : "=a"(dummy_ret)
            : "a"(1), "D"(1), "S"(buf), "d"(buf_size)
            : "memory");
    #endif
    #ifdef LINUX_32_BIT
        int dummy_ret;
        asm volatile(
            "int $0x80"
            : "=a"(dummy_ret)
            : "a"(0x04), "b"(1), "c"(buf), "d"(buf_size)
            : "memory");
    #endif
}

static void sleep_fractional(float secs) {
    unsigned long whole_secs = secs/1;
    unsigned long nonosecs   = (secs - (float) whole_secs)*1e9;

    unsigned long fake_timespec[2] = {whole_secs, nonosecs};
    #ifdef LINUX_64_BIT
        int dummy_ret;
        asm volatile(
            "syscall"
            : "=a"(dummy_ret)
            : "a"(35), "D"(fake_timespec), "S"(0)
            : "memory");
    #endif
    #ifdef LINUX_32_BIT
        int dummy_ret;
        asm volatile(
            "int $0x80"
            : "=a"(dummy_ret)
            : "a"(0xa2), "b"(fake_timespec), "c"(0)
            : "memory");
    #endif
}

static void clear_terminal(void) {
    write_stdout("\033c", 2);
}

