#define U8   unsigned char
#define U16  unsigned short
#define I16    signed short
#define UINT unsigned int

#define ERASE_SCREEN            "\033[2J"
#define ERASE_TIL_END_OF_SCREEN "\033[0J"
#define ERASE_TIL_END_OF_LINE   "\033[0K"
#define MOVE_CURSOR_0_0         "\033[H"

#define ALIVE_TEXT "##\0"
#define  DEAD_TEXT ". \0"

static const U8 GLIDER[9] = {
    0, 1, 0,
    0, 0, 1,
    1, 1, 1,
};

static U8  setup  = 0;
static U16 width  = 0;
static U16 height = 0;
static U8  is_new = 0;
static U16 uint_input;

#define MAX_WORKING_MEM 48*1024
static U8    mem_region[MAX_WORKING_MEM] = {0};
static U8*   buf;
#define READ_STDIN_BUF_SIZE 1024
static char* read_stdin_buf;
static char* text_buf;
#define      MIN_TEXT_BUF_SIZE 1024
static UINT  max_text_buf_size;

#ifdef BROWSER
    static U8 should_output_prompt = 1;
#endif

#define OLD 0
#define NEW 1
static U8 get_cell(U8 old_or_new, U16 x, U16 y) {
    UINT total_offset = y*width + x;
    U16           byte_offset = total_offset>>2;
    U8             bit_offset = ((total_offset&3)<<1) | (is_new^old_or_new);
    return (buf[byte_offset]>>bit_offset) & 1;
}

static void set_cell(U8 old_or_new, U16 x, U16 y, U8 value) {
    UINT total_offset = y*width + x;
    U16           byte_offset = total_offset>>2;
    U8             bit_offset = ((total_offset&3)<<1) | (is_new^old_or_new);
    if (value) {
        buf[byte_offset] |=   1<<bit_offset;
    } else {
        buf[byte_offset] &= ~(1<<bit_offset);
    }
}

#ifdef BROWSER
    __attribute__((export_name("update")))
    void update(void);

    __attribute__((import_name("output")))
    void write_stdout(char* buf, UINT buf_size);

    __attribute__((import_name("clearTerm")))
    void clear_terminal(void);

    __attribute__((import_name("readInputChar")))
    char read_stdin_char(void);
#else
    static void update(void);

    void _start(void) {
        float secs = 0.1;

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

    static void write_stdout(char* buf, UINT buf_size) {
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

    static char read_stdin_char(void) {
        #ifdef LINUX_64_BIT
            U8 dummy_ret;
            asm volatile(
                "syscall"
                : "=a"(dummy_ret)
                : "a"(0), "D"(0), "S"(read_stdin_buf), "d"(1)
                : "memory");
        #endif
        #ifdef LINUX_32_BIT
            U8 dummy_ret;
            asm volatile(
                "int $0x80"
                : "=a"(dummy_ret)
                : "a"(0x03), "b"(0), "c"(read_stdin_buf), "d"(1)
                : "memory");
        #endif
        return read_stdin_buf[0];
    }
#endif

static U8 prompt_uint_input(char* prompt, U16* result) {
    if (*result > 0) return 0;

    if (uint_input == 0
    #ifdef BROWSER
        && should_output_prompt
    #endif
    ) {
        U8 prompt_len = 0;
        while (prompt[prompt_len++] != '\0');
        write_stdout(prompt, prompt_len);
        #ifdef BROWSER
            should_output_prompt = 0;
        #endif
    }

    char c = read_stdin_char();
    #ifdef BROWSER
        // The browser version of read_stdin_char is nonblocking.
        // It returns 0 if nothing new is read.
        if (c == 0) return 1;
    #endif
    if (c == '\n') {
        if (uint_input == 0) {
            write_stdout("Number cannot be 0. Please try again!\n", 38);
            #ifdef BROWSER
                should_output_prompt = 1;
            #endif
            return 1;
        }

        *result = uint_input;
        uint_input = 0;
        #ifdef BROWSER
            should_output_prompt = 1;
        #endif
        return 0;
    } else {
        if (c < '0' || c > '9') {
            write_stdout("Invalid positive whole number. Please try again!\n", 49);
            // Input is line-buffered
            // -> we read until the end of the line to fully clear the
            //    invalid input from the read buffer.
            while (read_stdin_char() != '\n');
            uint_input = 0;
            #ifdef BROWSER
                should_output_prompt = 1;
            #endif
            return 1;
        }

        uint_input *= 10;
        uint_input += c - '0';
        return 1;
    }
}

void update(void) {
    //
    // Setup
    //
    if (!setup) {
        U16 mem_offset = 0;

        read_stdin_buf = (void*) mem_region + mem_offset;
        mem_offset += READ_STDIN_BUF_SIZE;

        if (prompt_uint_input("Game  width: ", &width )) return;
        if (prompt_uint_input("Game height: ", &height)) return;

        buf = (void*) mem_region + mem_offset;
        mem_offset += (width*height>>2) + 4;

        if (mem_offset >= MAX_WORKING_MEM - MIN_TEXT_BUF_SIZE) {
            write_stdout("Grid too large. Please try again!\n", 34);
            width  = 0;
            height = 0;
            return;
        };

        text_buf = (void*) mem_region + mem_offset;
        max_text_buf_size = ((MAX_WORKING_MEM - mem_offset)>>10)<<10;

        for (U16 y = 0; y < 3; ++y)
            for (U16 x = 0; x < 3; ++x)
                set_cell(OLD, x, y, GLIDER[y*3 + x]);

        #ifndef BROWSER
            write_stdout(ERASE_SCREEN MOVE_CURSOR_0_0, 7);
        #endif
        setup = 1;
    }

    //
    // Game update
    //
    #ifdef BROWSER
        clear_terminal();
    #endif
    char* text_ptr = text_buf;
    for (I16 y = 0; y < height; ++y) {
        for (I16 x = 0; x < width; ++x) {
            U8 cell_was_alive = get_cell(OLD, x, y);

            U8 live_neighbors = 0;
            for (I16 ny = y-1; ny <= y+1; ++ny)
                for (I16 nx = x-1; nx <= x+1; ++nx)
                    live_neighbors +=
                        get_cell(OLD, (nx+width ) % width ,
                                      (ny+height) % height);
            live_neighbors -= cell_was_alive;

            set_cell(NEW, x, y,
                live_neighbors == 3 ||
                live_neighbors == 2 && cell_was_alive);

            char* cell_text = cell_was_alive ? ALIVE_TEXT : DEAD_TEXT;
            while (*cell_text != '\0')
                *text_ptr++ = *cell_text++;

            if (text_ptr - text_buf > max_text_buf_size) {
                write_stdout(text_buf, text_ptr - text_buf);
                text_ptr = text_buf;
            }
        }
        #ifndef BROWSER
            char* escape_seq = ERASE_TIL_END_OF_LINE;
            while (*escape_seq != '\0')
                *text_ptr++ = *escape_seq++;
        #endif
        *text_ptr++ = '\n';
    }
    write_stdout(text_buf, text_ptr - text_buf);
    #ifndef BROWSER
        write_stdout(ERASE_TIL_END_OF_SCREEN MOVE_CURSOR_0_0, 7);
    #endif

    is_new ^= 1;
}

