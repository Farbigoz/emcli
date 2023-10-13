#ifndef __TERMINAL_H__
#define __TERMINAL_H__

#include <stddef.h>
#include <stdint.h>
#include <memory.h>
#include <error.h>

#include "paralstream.h"
#include "ansi.h"


class Terminal {
public:
	static const size_t HISTORY_BUFF_SIZE = 256;

public:		// Terminal API
	Terminal(ParallelStream &a_stream)
	:
	stream(a_stream)
	{
		term.xpos = 0;
		term.ypos = 0;

		hist.rPtr = hist.wPtr = hist.buff;
		*hist.wPtr = '\0';
	}

	void Puts(const char *s) {
		for (; *s != '\0'; s++)
			Putc(*s);
	}

	char * Gets(char *s) {
		char c;
		int res;

		char tmp[32];
		const char *tmpP;

		const char *histS = historyGetNewest();

		size_t pos = 0;
		size_t size = 0;

		while (true) {
			res = Getc();

			if (res == -ENODATA)
				continue;
			else if (res < 0)
				return nullptr;

			c = (char)res;

			// Обработка символа
			if (res < ANSI::NONE) {
				Putc(c);

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

				continue;
			}

			switch (res) {
				case ANSI::KEY_END_LINE:
				case ANSI::KEY_NEW_LINE:
				case ANSI::KEY_RETURN: {
					s[size] = '\0';

					historyWriteNewest(s);
					historySaveNewest();

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

						sprintf(tmp, "\010%s ", &s[pos-1]);	// backspace + text + space
						Puts(tmp);

						sprintf(tmp, "\033[%zuD", strlen(tmp) - 1); // move left to prev cursor pos
						Puts(tmp);
					}

					size--;
					pos--;
					break;
				}

				case ANSI::KEY_DEL: {
					size_t n = size - pos;

					if (pos == size)
						break;

					size--;

					if (n == 1) {
						Puts(" \010");	// space + backspace
					} else {
						memmove(&s[pos], &s[pos + 1], size - pos);
						s[size] = '\0';

						sprintf(tmp, "%s ", &s[pos]);	// text + space
						Puts(tmp);

						sprintf(tmp, "\033[%zuD", strlen(tmp)); // move left to prev cursor pos
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
					s[size] = '\0';

					if (histS == historyGetNewest())
						historyWriteNewest(s);

					histS = historyBack();
					strcpy(s, histS);

					if (pos > 0) {
						sprintf(tmp, "\033[%zuD", pos);
						Puts(tmp);
					}
					sprintf(tmp, "\033[0K%s", s);
					Puts(tmp);

					size = strlen(s);
					pos = size;

					break;
				}

				case ANSI::KEY_DOWN: {
					s[size] = '\0';

					histS = historyForward();
					strcpy(s, histS);

					if (pos > 0) {
						sprintf(tmp, "\033[%zuD", pos);
						Puts(tmp);
					}
					sprintf(tmp, "\033[0K%s", s);
					Puts(tmp);

					size = strlen(s);
					pos = size;
				}

				default:
					break;
			}
		}
	}

	void Putc(char c) {
		stream.WriteByte(c);

		switch (ansiOut.Decode(c)) {
			case ANSI::KEY_END_LINE:
				break;

			case ANSI::KEY_NEW_LINE:
				term.ypos++;
				break;

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
				term.ypos -= ansiOut.GetNum(0);
				break;

			case ANSI::KEY_DOWN:
				term.ypos += ansiOut.GetNum(0);
				break;

			case ANSI::KEY_RIGHT:
				term.xpos += ansiOut.GetNum(0);
				break;

			case ANSI::KEY_LEFT:
				term.xpos -= ansiOut.GetNum(0);
				break;

			default:
				break;
		}

		if (term.xpos < 0) term.xpos = 0;
		if (term.ypos < 0) term.ypos = 0;
	}

	int Getc(uint32_t timeoutMs = 100) {
		char c;
		int res;
		ANSI::TCode code;

		while (1) {
			res = stream.ReadByte(&c, timeoutMs);
			//if (res == 1) printf("In:  \\%03o 0x%02x '%c'\n", (unsigned char)c, (unsigned char)c, (unsigned char)c);

			if (res == 0) {
				return -ENODATA;

			} else if (res == 1) {
				code = ansiIn.Decode(c);

				if (code == ANSI::NONE)
					return (unsigned char) c;
				else if (code == ANSI::CONTINUE)
					continue;
				else
					return code;

			} else {
				return res;
			}
		}
	}

	//void SetAutocomplete(char *table[], size_t n) {
	//	autocomp = table;
	//	autocompn = n;
	//}

private:
public:
	void historyWriteNewest(const char *s) {
		size_t len = strlen(s) + 1; // len + '\0'

		// (Длина == 1) ИЛИ (Новая строка == Текущая строка)
		if ((len == 1) || (strcmp(s, hist.wPtr) == 0))
			// Игнорирование
			return;

		size_t free = &hist.buff[HISTORY_BUFF_SIZE] - hist.wPtr;
		size_t usage = HISTORY_BUFF_SIZE - free;
		size_t removeLen = 0;
		// Удаление первых элементов буфера, если не достаточно места для сохранения строки
		while (len > (free + removeLen)) {
			removeLen += strlen(&hist.buff[removeLen]) + 1; // len + '\0'
		}
		usage -= removeLen;
		memmove(hist.buff, &hist.buff[removeLen], usage);

		hist.rPtr = hist.wPtr = &hist.buff[usage];
		strcpy(hist.wPtr, s);
	}

	void historySaveNewest() {
		size_t len = strlen(hist.wPtr) + 1; // len + '\0'

		if (len == 1)
			return;

		hist.rPtr = hist.wPtr += len;
		*hist.wPtr = '\0';
	}

	const char * historyGetNewest() const {
		return hist.wPtr;
	}

	const char * historyBack() {
		const char *ret = nullptr;

		//if (histn.rPtr <= histn.wPtr) {
			// Переход к предыдущей строке
			if (hist.rPtr > hist.buff) { hist.rPtr--; }
			while ((hist.rPtr > hist.buff) && (*(hist.rPtr - 1) != '\0')) { hist.rPtr--; }
			// for (; (histn.rPtr > histn.buff) && (*histn.rPtr != '\0'); histn.rPtr--);

			ret = hist.rPtr;
		//}

		return ret;
	}

	const char * historyForward() {
		const char *ret;

		if (hist.rPtr < hist.wPtr) {
			hist.rPtr++;

			// Переход к следующей строке
			while ((hist.rPtr <= hist.wPtr) && (*(hist.rPtr - 1) != '\0')) { hist.rPtr++; }

			ret = hist.rPtr;
		}
		else
			ret = hist.wPtr;

		return ret;
	}


private:
	ParallelStream &stream;

	ANSI ansiIn;
	ANSI ansiOut;

	struct {
		int xpos;
		int ypos;
	} term;

	struct {
		char buff[HISTORY_BUFF_SIZE];

		char *wPtr;
		const char *rPtr;
	} hist;

	//char **autocomp;
	//size_t autocompn;
};



#endif /* __TERMINAL_H__ */