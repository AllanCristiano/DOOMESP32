#include "doomgeneric.h"
#include "doomkeys.h"
#include "i_system.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32) || defined(WIN32)
#define OS_WINDOWS
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#endif

#ifdef OS_WINDOWS
#define CLK 0

#define WINDOWS_CALL(cond_, format_)       \
	do {                               \
		if (UNLIKELY(cond_))       \
			winError(format_); \
	} while (0)

void winError(char *format)
{
	LPVOID lpMsgBuf;
	DWORD dw = GetLastError();
	errno = dw;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);
	I_Error(format, lpMsgBuf);
}

/* Modified from https://stackoverflow.com/a/31335254 */
struct timespec {
	long tv_sec;
	long tv_nsec;
};
int clock_gettime(int p, struct timespec *spec)
{
	(void)p;
	__int64 wintime;
	GetSystemTimeAsFileTime((FILETIME *)&wintime);
	wintime -= 116444736000000000ll;
	spec->tv_sec = wintime / 10000000ll;
	spec->tv_nsec = wintime % 10000000ll * 100;
	return 0;
}

#else
#define CLK CLOCK_REALTIME
#endif

#define UNLIKELY(x_) __builtin_expect((x_), 0)
#define CALL(stmt_, format_)                     \
	do {                                     \
		if (UNLIKELY(stmt_))             \
			I_Error(format_, errno); \
	} while (0)
#define CALL_STDOUT(stmt_, format_) CALL((stmt_) == EOF, format_)

#define BYTE_TO_TEXT(buf_, byte_)                      \
	do {                                           \
		*(buf_)++ = '0' + (byte_) / 100u;      \
		*(buf_)++ = '0' + (byte_) / 10u % 10u; \
		*(buf_)++ = '0' + (byte_) % 10u;       \
	} while (0)

const char* grad[] = {"█", "▓", "▒", "░", " "};
#define GRAD_LEN 5

#define INPUT_BUFFER_LEN 16u
#define EVENT_BUFFER_LEN (INPUT_BUFFER_LEN * 2u - 1u)

struct color_t {
	uint32_t b : 8;
	uint32_t g : 8;
	uint32_t r : 8;
};

char *output_buffer;
size_t output_buffer_size;
struct timespec ts_init;

char input_buffer[INPUT_BUFFER_LEN];
uint16_t event_buffer[EVENT_BUFFER_LEN];
uint16_t *event_buf_loc;

void DG_Init()
{
#ifdef OS_WINDOWS
    __asm__ __volatile__ (
        "call GetStdHandle;"
        "mov %%eax, %0;"
        "cmp $-1, %0;"
        "je error_handle;"
        
        "call GetConsoleMode;"
        "mov %%eax, %1;"
        "cmp $0, %1;"
        "je error_handle;"
        
        "or $0x4, %1;"  // ENABLE_VIRTUAL_TERMINAL_PROCESSING
        "call SetConsoleMode;"
        "cmp $0, %0;"
        "je error_handle;"

        "call GetStdHandle;"
        "mov %%eax, %2;"
        "cmp $-1, %2;"
        "je error_handle;"

        "call GetConsoleMode;"
        "mov %%eax, %1;"
        "cmp $0, %1;"
        "je error_handle;"

        "and $0xFFFFFFD7, %1;"  // ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_QUICK_EDIT_MODE)
        "call SetConsoleMode;"
        "cmp $0, %1;"
        "je error_handle;"

        "jmp done;"

        "error_handle:"
        "mov $1, %3;"  // Sinaliza erro

        "done:"
        : "=r" (hOutputHandle), "=r" (mode), "=r" (hInputHandle), "=r" (error)
        : "0" (hOutputHandle), "1" (mode), "2" (hInputHandle)
        : "eax", "memory"
    );
#endif
	/* Longest SGR code: \033[38;2;RRR;GGG;BBBm (length 19)
	 * Maximum 21 bytes per pixel: SGR + 2 x char
	 * 1 Newline character per line
	 * SGR clear code: \033[0m (length 4)
	 */
	output_buffer_size = 21u * DOOMGENERIC_RESX  * DOOMGENERIC_RESY + DOOMGENERIC_RESY + 4u;
	output_buffer = malloc(output_buffer_size);

	clock_gettime(CLK, &ts_init);

	memset(input_buffer, '\0', INPUT_BUFFER_LEN);
}

void DG_DrawPixel(uint32_t *color, struct color_t *pixel, char **buf, unsigned row, unsigned col) {
    if (row >= DOOMGENERIC_RESY) {
        // Termina a recursão quando todas as linhas forem processadas
        **buf = '\033';
        *(*buf + 1) = '[';
        *(*buf + 2) = '0';
        *(*buf + 3) = 'm';
        return;
    }

    if (col >= DOOMGENERIC_RESX) {
        // Move para a próxima linha e insere nova linha
        *(*buf)++ = '\n';
        DG_DrawPixel(color, pixel, buf, row + 1, 0);  // Chama a próxima linha
        return;
    }

    // Lógica para processar um pixel
    if ((*color ^ *(uint32_t *)pixel) & 0x00FFFFFF) {
        *(*buf)++ = '\033';
        *(*buf)++ = '[';
        *(*buf)++ = '3';
        *(*buf)++ = '8';
        *(*buf)++ = ';';
        *(*buf)++ = '2';
        *(*buf)++ = ';';
        BYTE_TO_TEXT(*buf, pixel->r);
        *(*buf)++ = ';';
        BYTE_TO_TEXT(*buf, pixel->g);
        *(*buf)++ = ';';
        BYTE_TO_TEXT(*buf, pixel->b);
        *(*buf)++ = 'm';
        *color = *(uint32_t *)pixel;
    }

    // Adiciona sombreamento com base na posição do pixel
    float shadow_factor = 1.0 - (float)col / DOOMGENERIC_RESX;
    int shaded_r = (int)(pixel->r * shadow_factor);
    int shaded_g = (int)(pixel->g * shadow_factor);
    int shaded_b = (int)(pixel->b * shadow_factor);

    // Converte os valores sombreados para texto e armazena no buffer
    const char *v_char = grad[(shaded_r + shaded_g + shaded_b) * GRAD_LEN / 766u];
    *buf += sprintf(*buf, "%s", v_char);

    // Processa o próximo pixel
    DG_DrawPixel(color, pixel + 1, buf, row, col + 1); 
}

void DG_DrawFrame() {
    uint32_t color = 0xFFFFFF00;
    struct color_t *pixel = (struct color_t *)DG_ScreenBuffer;
    char *buf = output_buffer;

    DG_DrawPixel(&color, pixel, &buf, 0, 0); 

    
    fputs("\033[;H\033[1m", stdout);

    CALL_STDOUT(fputs(output_buffer, stdout), "DG_DrawFrame: fputs error %d");

    memset(output_buffer, '\0', buf - output_buffer + 1u);
}



// Função para esperar por um número especificado de milissegundos
void DG_SleepMs(uint32_t ms) {
    if (ms == 0) return;  // Evita chamadas desnecessárias

#ifdef OS_WINDOWS
    Sleep(ms);  // Suspende a execução em Windows
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;  // Converte milissegundos para segundos
    ts.tv_nsec = (ms % 1000) * 1000000;  // Converte o restante para nanosegundos
    nanosleep(&ts, NULL);  // Suspende a execução em outros sistemas
#endif
}

// Função para obter o tempo decorrido em milissegundos desde um ponto inicial
uint32_t DG_GetTicksMs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);  // Usar CLOCK_MONOTONIC é mais apropriado

    // Calcula a diferença em milissegundos
    uint32_t elapsed_ms = (ts.tv_sec - ts_init.tv_sec) * 1000 + (ts.tv_nsec - ts_init.tv_nsec) / 1000000;
    return elapsed_ms > 0 ? elapsed_ms : 0;  // Garante que o retorno não seja negativo
}

char convertToDoomKey(char **buf) {
    char key;

    // Assembly inline para carregar o caractere atual
    __asm__ __volatile__ (
        "mov (%1), %0"   // Carrega o valor de **buf em key
        : "=r"(key)      // Saída: armazena o caractere em 'key'
        : "r"(*buf)      // Entrada: o endereço apontado por *buf
    );

    switch (key) {
    case '\012':  // Nova linha
        (*buf)++;
        return KEY_ENTER;

    case '\033':  // Escape
        (*buf)++;
        if (**buf == '[') {
            (*buf)++;
            
            // Assembly inline para carregar o próximo caractere após '['
            __asm__ __volatile__ (
                "mov (%1), %0\n\t"   // Carrega o próximo valor de **buf em key
                "incq %1"            // Avança o ponteiro buf
                : "=r"(key)          // Saída: armazena o próximo caractere em 'key'
                : "r"(*buf)          // Entrada: o endereço apontado por *buf
            );
            
            switch (key) {
            case 'A': return KEY_UPARROW;
            case 'B': return KEY_DOWNARROW;
            case 'C': return KEY_RIGHTARROW;
            case 'D': return KEY_LEFTARROW;
            default: return KEY_ESCAPE;  // Retorna ESC se não for uma seta
            }
        }
        return KEY_ESCAPE;  // Retorna ESC se não for seguido de '['

    case ' ':  // Espaço
        (*buf)++;
        return KEY_FIRE;

    default:
        return tolower((*(*buf)++));  // Converte o caractere para minúsculo e avança o buffer
    }
}



void DG_ReadInput(void)
{
	static char prev_input_buffer[INPUT_BUFFER_LEN];
	static char raw_input_buffer[INPUT_BUFFER_LEN];

	memcpy(prev_input_buffer, input_buffer, INPUT_BUFFER_LEN);
	memset(raw_input_buffer, '\0', INPUT_BUFFER_LEN);
	memset(input_buffer, '\0', INPUT_BUFFER_LEN);
	memset(event_buffer, '\0', 2u * EVENT_BUFFER_LEN);
	event_buf_loc = event_buffer;
#if defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
	struct termios oldt, newt;

	/* Disable canonical mode */
	CALL(tcgetattr(STDIN_FILENO, &oldt), "DG_DrawFrame: tcgetattr error %d");
	newt = oldt;
	newt.c_lflag &= ~(ICANON);
	newt.c_cc[VMIN] = 0;
	newt.c_cc[VTIME] = 0;
	CALL(tcsetattr(STDIN_FILENO, TCSANOW, &newt), "DG_DrawFrame: tcsetattr error %d");

	CALL(read(2, raw_input_buffer, INPUT_BUFFER_LEN - 1u) < 0, "DG_DrawFrame: read error %d");

	CALL(tcsetattr(STDIN_FILENO, TCSANOW, &oldt), "DG_DrawFrame: tcsetattr error %d");

	/* Flush input buffer to prevent read of previous unread input */
	CALL(tcflush(STDIN_FILENO, TCIFLUSH), "DG_DrawFrame: tcflush error %d");
#else /* defined(OS_WINDOWS) */
	const HANDLE hInputHandle = GetStdHandle(STD_INPUT_HANDLE);
	WINDOWS_CALL(hInputHandle == INVALID_HANDLE_VALUE, "DG_ReadInput: %s");

	/* Disable canonical mode */
	DWORD old_mode, new_mode;
	WINDOWS_CALL(!GetConsoleMode(hInputHandle, &old_mode), "DG_ReadInput: %s");
	new_mode = old_mode;
	new_mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
	WINDOWS_CALL(!SetConsoleMode(hInputHandle, new_mode), "DG_ReadInput: %s");

	DWORD event_cnt;
	WINDOWS_CALL(!GetNumberOfConsoleInputEvents(hInputHandle, &event_cnt), "DG_ReadInput: %s");

	/* ReadConsole is blocking so must manually process events */
	int input_count = 0;
	if (event_cnt) {
		INPUT_RECORD input_records[32];
		WINDOWS_CALL(!ReadConsoleInput(hInputHandle, input_records, 32, &event_cnt), "DG_ReadInput: %s");

		DWORD i;
		for (i = 0; i < event_cnt; i++) {
			if (input_records[i].Event.KeyEvent.bKeyDown && input_records[i].EventType == KEY_EVENT) {
				raw_input_buffer[input_count++] = input_records[i].Event.KeyEvent.uChar.AsciiChar;
				if (input_count == INPUT_BUFFER_LEN - 1u)
					break;
			}
		}
	}

	WINDOWS_CALL(!SetConsoleMode(hInputHandle, old_mode), "DG_ReadInput: %s");
#endif
	/* create input buffer */
	char *raw_input_buf_loc = raw_input_buffer;
	char *input_buf_loc = input_buffer;
	while (*raw_input_buf_loc)
		*input_buf_loc++ = convertToDoomKey(&raw_input_buf_loc);

	/* construct event array */
	int i, j;
	for (i = 0; input_buffer[i]; i++) {
		/* skip duplicates */
		for (j = i + 1; input_buffer[j]; j++) {
			if (input_buffer[i] == input_buffer[j])
				goto LBL_CONTINUE_1;
		}

		/* pressed events */
		for (j = 0; prev_input_buffer[j]; j++) {
			if (input_buffer[i] == prev_input_buffer[j])
				goto LBL_CONTINUE_1;
		}
		*event_buf_loc++ = 0x0100 | input_buffer[i];

	LBL_CONTINUE_1:;
	}

	/* depressed events */
	for (i = 0; prev_input_buffer[i]; i++) {
		for (j = 0; input_buffer[j]; j++) {
			if (prev_input_buffer[i] == input_buffer[j])
				goto LBL_CONTINUE_2;
		}
		*event_buf_loc++ = 0xFF & prev_input_buffer[i];

	LBL_CONTINUE_2:;
	}

	event_buf_loc = event_buffer;
}

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
    if (!*event_buf_loc)  // Verifica se há eventos no buffer
        return 0;

    *pressed = *event_buf_loc >> 8;   // Extrai o estado de pressionado
    *doomKey = *event_buf_loc & 0xFF; // Extrai a tecla DOOM
    event_buf_loc++; // Avança para o próximo evento

    return 1;
}

void DG_SetWindowTitle(const char *title)
{
	(void)title;
}
