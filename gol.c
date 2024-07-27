#define U8  unsigned char
#define U16 unsigned short
#define I16   signed short

static void setup(void);
#ifdef WEB
    __attribute__((export_name("update")))
    void update(void);

    __attribute__((import_name("output")))
    void write_stdout(char* buf, U16 buf_size);

    __attribute__((import_name("clearTerm")))
    void clear_terminal(void);

    __attribute__((import_name("updateEveryNSecs")))
    void update_every_n_secs(float secs);
#else
    static void update(void);
    static void write_stdout(char* buf, U16 buf_size);
    static void clear_terminal(void);
    static void update_every_n_secs(float secs);
#endif

#define ALIVE_TEXT "##"
#define  DEAD_TEXT ". "
#define WIDTH  20
#define HEIGHT 30

static const U8 GLIDER[3][3] = {
    {0, 1, 0},
    {0, 0, 1},
    {1, 1, 1},
};

static U8 bufs[2][HEIGHT][WIDTH];
static U8 buf_idx_toggle = 0;
#define OLD_BUF bufs[buf_idx_toggle^1]
#define NEW_BUF bufs[buf_idx_toggle  ]
static char text_buf[16*WIDTH*HEIGHT];

void _start(void) {
    setup();
    update_every_n_secs(0.1);
}

void setup(void) {
    for (U16 y = 0; y < 3; ++y)
        for (U16 x = 0; x < 3; ++x)
            OLD_BUF[y][x] = GLIDER[y][x];
}

void update(void) {
    char* text_ptr = text_buf;

    for (I16 y = 0; y < HEIGHT; ++y) {
        for (I16 x = 0; x < WIDTH; ++x) {
            U8 live_neighbors = 0;
            for (I16 ny = y-1; ny <= y+1; ++ny)
                for (I16 nx = x-1; nx <= x+1; ++nx)
                    live_neighbors +=
                        OLD_BUF[(ny+HEIGHT) % HEIGHT]
                               [(nx+WIDTH ) % WIDTH ];
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
}

#ifndef WEB
    void write_stdout(char* buf, U16 buf_size) {
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
    void clear_terminal(void) {
        write_stdout("\033c", 2);
    }

    void update_every_n_secs(float secs) {
        unsigned long whole_secs = secs/1;
        unsigned long nonosecs   = (secs - (float) whole_secs)*1e9;
        unsigned long fake_timespec[2] = {whole_secs, nonosecs};

        while (1) {
            update();
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
    }
#endif

