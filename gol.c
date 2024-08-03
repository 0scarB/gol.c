#define U8    unsigned char
#define U16   unsigned short
#define I16     signed short
#define UINT  unsigned int
#define ULONG unsigned long

#define ERASE_SCREEN            "\033[2J"
#define ERASE_TIL_END_OF_SCREEN "\033[0J"
#define ERASE_TIL_END_OF_LINE   "\033[0K"
#define MOVE_CURSOR_0_0         "\033[H"

#define ALIVE_TEXT "##\0"
#define  DEAD_TEXT ". \0"

#define PRESET_RANDOM 0
#define PRESET_GLIDER 1
#define PRESETS_N PRESET_GLIDER + 1
static char* preset_names[] = {"Random", "Glider"};
U16 selected_preset = 0;

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

#define MAX_WORKING_MEM     48*1024
static U8    mem_region[MAX_WORKING_MEM] = {0};
static U8*   buf;
#define READ_STDIN_BUF_SIZE    1024
static char* read_stdin_buf;
static char* text_buf;
#define      MIN_TEXT_BUF_SIZE 1024
static UINT  max_text_buf_size;

#ifdef BROWSER
    static U8 should_output_prompt = 1;

    __attribute__((export_name("update")))
    void update(void);

    __attribute__((import_name("output")))
    void output(char* buf, UINT buf_size);

    __attribute__((import_name("clearTerm")))
    void clear_terminal(void);

    __attribute__((import_name("readInputChar")))
    char read_stdin_char(void);

    __attribute__((import_name("setUpdateInterval")))
    void set_update_interval(float interval);

    __attribute__((import_name("random")))
    float random(void);

    static void fill_random(char* buf, ULONG count) {
        while (--count)
            buf[count] = (char) (256*random());
    }
#else
    #define SYSCALL_32_AND_64BIT 0
    #define SYSCALL_32BIT_ONLY   1
    #ifdef LINUX_64_BIT
        #define SYSCALL_NO_READ          0
        #define SYSCALL_NO_WRITE         1
        #define SYSCALL_NO_SLEEP_NANO   35
        #define SYSCALL_NO_GET_RANDOM  318

    #else
        #define SYSCALL_NO_READ           3
        #define SYSCALL_NO_WRITE          4
        #define SYSCALL_NO_SLEEP_NANO  0xa2
        #define SYSCALL_NO_GET_RANDOM 0x163
    #endif

    ULONG update_interval[2] = {0, 0};

    void syscall(ULONG syscall_no, ULONG arg1, ULONG arg2, ULONG arg3);

    static void update(void);

    void _start(void) {
        while (1) {
            update();
            syscall(SYSCALL_NO_SLEEP_NANO, (ULONG) update_interval, 0, 0);
        }
    }

    static void output(char* buf, ULONG buf_size) {
        syscall(SYSCALL_NO_WRITE, 0, (ULONG) buf, buf_size);
    }

    static char read_stdin_char(void) {
        syscall(SYSCALL_NO_READ, 0, (ULONG) read_stdin_buf, 1);
        return read_stdin_buf[0];
    }

    static void fill_random(char* buf, ULONG count) {
        syscall(SYSCALL_NO_GET_RANDOM, (ULONG) buf, count, 0);
    }

    static void set_update_interval(float secs) {
        ULONG whole_secs = secs/1;
        ULONG nonosecs   = (secs - (float) whole_secs)*1e9;
        update_interval[0] = whole_secs;
        update_interval[1] = nonosecs;
    }
#endif

static void output_str(char* str) {
    U16 len = 0;
    while (str[len++] != '\0');
    output(str, len);
}

#define OLD 0
#define NEW 1
static U8 get_cell(U8 old_or_new, U16 x, U16 y) {
    UINT total_offset = y*width + x;
    U16   byte_offset = total_offset>>2;
    U8     bit_offset = ((total_offset&3)<<1) | (is_new^old_or_new);
    return (buf[byte_offset]>>bit_offset) & 1;
}

static void set_cell(U8 old_or_new, U16 x, U16 y, U8 value) {
    UINT total_offset = y*width + x;
    U16   byte_offset = total_offset>>2;
    U8     bit_offset = ((total_offset&3)<<1) | (is_new^old_or_new);
    if (value) {
        buf[byte_offset] |=   1<<bit_offset;
    } else {
        buf[byte_offset] &= ~(1<<bit_offset);
    }
}

#ifndef BROWSER
#pragma GCC push_options
#pragma GCC optimize ("O0")
#endif
static U8 prompt_uint_input(char* prompt, U16* result) {
    if (*result > 0) return 0;

    if (uint_input == 0
    #ifdef BROWSER
        && should_output_prompt
    #endif
    ) {
        output_str(prompt);
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
            output("Number cannot be 0. Please try again!\n", 38);
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
            output("Invalid positive whole number. Please try again!\n", 49);
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
#ifndef BROWSER
#pragma GCC pop_options
#endif

void update(void) {
    //
    // Setup
    //
    if (!setup) {
        U16 mem_offset = 0;

        read_stdin_buf = (char*) mem_region + mem_offset;
        mem_offset += READ_STDIN_BUF_SIZE;

        if (prompt_uint_input("Game  width: ", &width )) return;
        if (prompt_uint_input("Game height: ", &height)) return;

        buf = (U8*) mem_region + mem_offset;
        mem_offset += (width*height>>2) + 4;

        if (mem_offset >= MAX_WORKING_MEM - MIN_TEXT_BUF_SIZE) {
            output_str("Grid too large. Please try again!\n");
            width  = 0;
            height = 0;
            return;
        };

        text_buf = (char*) mem_region + mem_offset;
        max_text_buf_size = ((MAX_WORKING_MEM - mem_offset)>>10)<<10;

        #ifdef BROWSER
        if (should_output_prompt) {
        #endif
            output_str("Choose a preset:");
            for (U8 i = 0; i < PRESETS_N; ++i) {
                char line_start[4] = "\n : ";
                line_start[1] = i + '1';
                output(line_start, 3);
                output_str(preset_names[i]);
            }
        #ifdef BROWSER
        }
        #endif
        if (prompt_uint_input("\nChoice: ", &selected_preset)) return;
        switch (selected_preset - 1) {
            case PRESET_GLIDER:
                for (U16 y = 0; y < 3; ++y)
                    for (U16 x = 0; x < 3; ++x)
                        set_cell(OLD, x, y, GLIDER[y*3 + x]);
                break;
            case PRESET_RANDOM:
                fill_random((char*) buf, width*height>>2);
                break;
            default:
                output_str("Invalid choice. Please try again!\n");
                selected_preset = 0;
                return;
        }

        #ifndef BROWSER
            output(ERASE_SCREEN MOVE_CURSOR_0_0, 7);
        #endif

        set_update_interval(0.1);

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
                output(text_buf, text_ptr - text_buf);
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
    output(text_buf, text_ptr - text_buf);
    #ifndef BROWSER
        output(ERASE_TIL_END_OF_SCREEN MOVE_CURSOR_0_0, 7);
    #endif

    is_new ^= 1;
}
