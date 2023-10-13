#ifndef __SINGLE_TERMINAL_H__
#define __SINGLE_TERMINAL_H__

#include <stddef.h>
#include <stdint.h>
#include <memory.h>
#include <error.h>

#include "terminal.h"


class SingleTerminal : public Terminal {
public:
	static const size_t MAX_INPUT_LEN = 64;
	static const size_t HISTORY_SIZE = 5;

public:
	SingleTerminal(ParallelStream &a_stream)
	:
	Terminal(historyInputs, HISTORY_SIZE),
	stream(a_stream)
	{
		for (int i = 0; i < HISTORY_SIZE; i++)
			historyInputs[i] = historyBuffers[i];
	}

public:
	void Puts(const char *s) final {
		char c;
		size_t len = strlen(s);

		for (int i = 0; i < len; i++) {
			c = s[i];
			Putc(s[i]);

			switch (ansi.Decode(c)) {
				case ANSI::KEY_END_LINE:
					break;

				case ANSI::KEY_NEW_LINE:
					term.ypos++;
				case ANSI::KEY_RETURN:
					term.xpos = 0;
					break;

				case ANSI::KEY_BACKSPACE:
					term.xpos--;
					break;

				case ANSI::KEY_TAB:
					term.xpos = (term.xpos / 8) * 8 + 8;
					break;

				case ANSI::KEY_UP:
					term.ypos -= ansi.GetNum(0);
					break;

				case ANSI::KEY_DOWN:
					term.ypos += ansi.GetNum(0);
					break;

				case ANSI::KEY_RIGHT:
					term.xpos += ansi.GetNum(0);
					break;

				case ANSI::KEY_LEFT:
					term.xpos -= ansi.GetNum(0);
					break;

				default:
					break;
			}

			if (term.xpos < 0) term.xpos = 0;
			if (term.ypos < 0) term.ypos = 0;
		}
	}

	char * Gets(char *s) final {
		char c;
		char tmp[32];
		const char *tmpP;

		int res;

		int pos = 0;
		size_t size = 0;
		int histSelected = -1;

		int tabCnt = 0;

		while (1) {
			res = Getc();
			if (res == -ENODATA)
				continue;
			else if (res < 0)
				return nullptr;
			c = (char)res;
			printf("In:  \\%03o 0x%02x '%c'\n", (unsigned char)c, (unsigned char)c, (unsigned char)c);

			switch (ansi.Decode(c)) {
				case ANSI::NONE: {
					Putc(c);
					// 1234
					// size=4, pos=2

					if (pos < size) {
						memmove(&s[pos+1], &s[pos], size - pos);
						s[size+1] = '\0';

						Puts(&s[pos+1]);

						sprintf(tmp, "\033[%zuD", size - pos);
						Puts(tmp);
					}

					s[pos] = c;

					size++;
					pos++;

					tabCnt = 0;

					break;
				}

				case ANSI::KEY_END_LINE:
				case ANSI::KEY_NEW_LINE:
				case ANSI::KEY_RETURN: {
					s[size] = '\0';
					historyPush(s);

					return s;
				}

				case ANSI::KEY_TAB: {
					s[size] = '\0';

					/*
					// Подсчёт совпадений
					size_t cmpCnt = 0;
					for (size_t i = 0; i < autocompn; i++)
						if (strncmp(s, autocomp[i], pos) == 0)
							cmpCnt++;

					// Вывод совпадений при многократном нажатии TAB
					if ((cmpCnt > 1) && (++tabCnt > 1)) {

					}
					*/

					break;
				}

				case ANSI::KEY_BACKSPACE: {
					if (pos == 0)
						break;

					if (pos == size) {
						Puts("\010\033[0K");
					}
					else {
						memmove(&s[pos - 1], &s[pos], size - pos);
						s[size-1] = '\0';

						Putc('\010');	// backspace
						Puts(&s[pos-1]);
						Putc(' ');		// space

						// move left to prev cursor pos
						sprintf(tmp, "\033[%zuD", size - pos + 1);
						Puts(tmp);
					}

					size--;
					pos--;
					break;
				}

				case ANSI::KEY_DEL: {
					int n = size - pos;

					if (pos == size)
						break;

					size--;

					if (n == 1) {
						Puts(" \010");	// space + backspace
					} else {
						memmove(&s[pos], &s[pos + 1], size - pos);
						s[size] = '\0';

						Puts(&s[pos]);
						Putc(' ');

						// move left to prev cursor pos
						sprintf(tmp, "\033[%zuD", size - pos + 1);
						Puts(tmp);
					}
					break;
				}

				case ANSI::KEY_LEFT: {
					if (pos == 0)
						break;

					pos--;
					Puts("\033[D");
					break;
				}

				case ANSI::KEY_RIGHT: {
					if (pos == size)
						break;

					pos++;
					Puts("\033[C");
					break;
				}

				case ANSI::KEY_UP: {
					if (histSelected == HISTORY_SIZE)
						break;

					tmpP = historyGet(++histSelected);
					if (tmpP == nullptr)
						break;

					if (pos > 0) {
						sprintf(tmp, "\033[%dD", pos);
						Puts(tmp);
					}
					Puts("\033[0K");
					Puts(tmpP);

					size = strlen(tmpP);
					pos = size;

					break;
				}

				case ANSI::KEY_DOWN: {
					if (histSelected == 0) {
						histSelected--;

						if (pos > 0) {
							sprintf(tmp, "\033[%dD", pos);
							Puts(tmp);
						}
						Puts("\033[0K");

						size = 0;
						pos = 0;

					} else if (histSelected > 0) {
						tmpP = historyGet(--histSelected);
						if (tmpP == nullptr)
							break;

						if (pos > 0) {
							sprintf(tmp, "\033[%dD", pos);
							Puts(tmp);
						}
						Puts("\033[0K");
						Puts(tmpP);

						size = strlen(tmpP);
						pos = size;
					}

					break;
				}

				default: {
					break;
				}
			}
		}
	}

	void Putc(char c) final {
		//printf("Out: \\%03o 0x%02x '%c'\n", (unsigned char)c, (unsigned char)c, (unsigned char)c);
		stream.WriteByte(c);
	}

	int Getc(uint32_t timeoutMs = 100) {
		char c;
		int res = stream.ReadByte(&c, timeoutMs);

		//if (res == 1) printf("In:  \\%03o 0x%02x '%c'\n", (unsigned char)c, (unsigned char)c, (unsigned char)c);

		if (res == 0)
			return -ENODATA;

		else if (res == 1)
			return (unsigned char)c;

		else
			return res;
	}

private:
	ParallelStream &stream;

	// TODO: Нужно поработать над реализацией истории
	char *historyInputs[HISTORY_SIZE];
	char historyBuffers[HISTORY_SIZE][MAX_INPUT_LEN];
};



#endif /* __SINGLE_TERMINAL_H__ */