#ifndef __ANSI_H__
#define __ANSI_H__

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <cstdarg>

// Запрос позиции курсора - \033[6n
//   Ответ: ESC[#;#R
//


class ANSI {
public:
	static const size_t MAX_NUMS = 5;

public:
	typedef enum : uint32_t {
		NONE = 0x7ff,
		CONTINUE,

		KEY_END_LINE,
		KEY_NEW_LINE,
		KEY_RETURN,

		KEY_TAB,

		KEY_BACKSPACE,
		KEY_DEL,

		KEY_UP,
		KEY_DOWN,
		KEY_RIGHT,
		KEY_LEFT,

		ERASE_DISPLAY,

		STYLE
	} TCode;

	typedef enum : uint32_t {
		STYLE_RESET,
		STYLE_BOLD = 1,
		STYLE_DIM,
		STYLE_ITALIC,
		STYLE_UNDERLINE,
		STYLE_BLINKING,
		STYLE_INVERSE,
		STYLE_HIDDEN,

		STYLE_FG_BLACK = 30,
		STYLE_FG_RED,
		STYLE_FG_GREEN,
		STYLE_FG_YELLOW,
		STYLE_FG_BLUE,
		STYLE_FG_MAGENTA,
		STYLE_FG_CYAN,
		STYLE_FG_WHITE,
		STYLE_FG_DEFAULT = 39,

		STYLE_BG_BLACK = 40,
		STYLE_BG_RED,
		STYLE_BG_GREEN,
		STYLE_BG_YELLOW,
		STYLE_BG_BLUE,
		STYLE_BG_MAGENTA,
		STYLE_BG_CYAN,
		STYLE_BG_WHITE,
		STYLE_BG_DEFAULT = 49,

		STYLE_END = 0xffff
	} TStyle;


public:
	ANSI() {
		resetDecoder();

		for (auto &num : resNumber)
			num = 0;
	}

	TCode Decode(char c) {
		esc.buff[esc.pos] = c;

		if (esc.pos == 0)
		{
			switch (c) {
				case '\010':		// Ctrl + Backspace
					return KEY_BACKSPACE;

				case '\011':		// Tab
					return KEY_TAB;

				case '\0':		// \0
					return KEY_END_LINE;

				case '\n':		// \n
					return KEY_NEW_LINE;

				case '\r':		// \r
					return KEY_RETURN;

				case '\033':		// ESC
					esc.pos++;
					return CONTINUE;

				case '\177':		// Backspace
					return KEY_BACKSPACE;

				default:
					resetDecoder();
					return NONE;
			}
		}

		else if (esc.pos == 1) {
			switch (c) {
				case '[':		// CSI
					break;

				default:
					resetDecoder();
					return NONE;
			}

			esc.pos++;
			return CONTINUE;
		}

		else if (esc.pos < sizeof(esc.buff)) {
			switch (c) {
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					esc.pos++;
					esc.number[esc.numn] *= 10;
					esc.number[esc.numn] += c - '0';
					break;

				case ';':
					esc.numn++;
					break;

				case 'A':
					resNumber[0] = esc.number[0] ? esc.number[0] : 1;
					resetDecoder();
					return KEY_UP;

				case 'B':
					resNumber[0] = esc.number[0] ? esc.number[0] : 1;
					resetDecoder();
					return KEY_DOWN;

				case 'C':
					resNumber[0] = esc.number[0] ? esc.number[0] : 1;
					resetDecoder();
					return KEY_RIGHT;

				case 'D':
					resNumber[0] = esc.number[0] ? esc.number[0] : 1;
					resetDecoder();
					return KEY_LEFT;

				case '~':
					resNumber[0] = esc.number[0];
					resetDecoder();
					return decodeVT(resNumber[0]);

				default:
					resetDecoder();
					return NONE;
			}

			esc.pos++;
			return CONTINUE;

		} else {	// Переполнение буфера ESC последовательности
			resetDecoder();
			return NONE;
		}

	}

	short GetNum(int n) {
		if ((n < 0) || (n > MAX_NUMS))
			return 0;
		return resNumber[n];
	}

	static void Encode(char *s, TCode code, ...) {
		//char tmp[32] = {0};
		uint32_t style;
		va_list args;

		va_start(args, code);
		switch (code) {
			case ERASE_DISPLAY:
				// Erase saved lines + goto 0;0 + erase from cursor to end display
				sprintf(s, "\033[3J\033[0;0H\033[0J");
				break;

			case STYLE:
				sprintf(s, "\033[");
				while (1) {
					style = va_arg(args, uint32_t);
					if (style == STYLE_END)
						break;
					sprintf(&s[strlen(s)], "%d;", style);
				}
				s[strlen(s)-1] = 'm';
				break;

			case KEY_BACKSPACE:
				*(s++) = '\010';
				*(s++) = '\0';
				break;
		}
		va_end(args);
	}

private:
	void resetDecoder() {
		esc.pos = 0;
		esc.numn = 0;
		for (auto &num : esc.number)
			num = 0;
	}

	TCode decodeVT(short n) {
		switch (n) {
			case 3:
				return KEY_DEL;

			default:
				return NONE;
		}
	}


private:
	struct {
		char buff[32];
		uint8_t pos;

		short number[MAX_NUMS];
		uint8_t numn;
	} esc;

	short resNumber[MAX_NUMS];

};




#endif /* __ANSI_H__ */