#ifndef __COMMAND_PROCESSOR_H__
#define __COMMAND_PROCESSOR_H__

#include <stddef.h>

#include "terminal.h"

//class CommandProcessor;


namespace cmdproc {
	typedef struct {
		const char ch;				/* >> cmd -"o" */
		const char *full;			/* >> cmd -o --"option" */
		const char *args;			/* >> cmd -o --options "arg1 arg2".
 									 *
 									 * Аргументы необходимо писать в одну строку через пробел: "arg1 arg2 ~arg3".
 									 *
 									 * Типы аргументов:
 									 *  "arg.name"  - Обязательный аргумент
 									 *  "~arg.name" - Не обязательный аргумент
 									 */
		const char *description;	/* -o --option <arg1> <arg2> "this option..." */
	} CmdOpt_t;

	typedef struct {
		const CmdOpt_t *ref;		// Указатель на структуру описания опции

		char **argv;				// Указатели на полученные аргументы опции
		int argc;					// Количество полученных аргументов
	} OptArgs_t;

	typedef struct {
		char **argv;				// Указатель на полученные аргументы команды
		int argc;					// Количество полученных аргументов

		OptArgs_t *opts;			// Указатель на указатели полученные опции
		int optc;					// Количество полученных опций
	} CmdArgs_t;

	typedef struct {
		int (*fn)(void *ctx, Terminal &t, CmdArgs_t &a);
		void *ctx;

		const char *cmd;			/* >> "cmd" */
		const char *args;			/* >> cmd [options] "<arg1> <arg2>"
									 *
									 * Аргументы необходимо писать в одну строку через пробел: "arg1 arg2 ~arg3".
									 *
									 * Типы аргументов:
									 *  "arg.name"  - Обязательный аргумент
									 *  "~arg.name" - Не обязательный аргумент.
									 *                Должен быть установлен в качестве последнего аргумента.
									 *                (Не сочетается с множеством аргументов)
									 *  "*arg.name" - Множество аргументов.
									 *                Должен быть установлен в качестве последнего аргумента.
									 *                (Не сочетается с необязательным аргументом)
									 * */
		CmdOpt_t   *options;		/* Options: ... */
		const int   optc;			/* Количество опций */
		const char *descr;			/* Description: ... */
	} CmdDef_t;
}



class CommandProcessor {
public:
	static const size_t MAX_INPUT_LEN = 64;

	static const size_t MAX_CMD = 32;
	static const size_t MAX_ARGS = 16;

	static const inline char NEWLINE[] = "\r\n";
	static const inline char DEFAULT_PREFIX[] = ">> ";

	static const inline char HELP_ARG[] = "help";

public:
	CommandProcessor(Terminal &t)
	:
	term(t)
	{
		prefix = DEFAULT_PREFIX;

		for (auto &cmd : commands)
			cmd = nullptr;

		Register(baseCmd_Reset);
		Register(baseCmd_Help);
	}

	void Register(cmdproc::CmdDef_t &a_cmd) {
		for (auto &cmd : commands) {
			if ((cmd != nullptr) && (strcmp(cmd->cmd, a_cmd.cmd) == 0))
				return;	// TODO: Ошибка
		}

		for (auto &cmd : commands) {
			if (cmd != nullptr)
				continue;

			cmd = &a_cmd;
			break;
		}
	}

	void Unregister(cmdproc::CmdDef_t *a_cmd) {
		for (auto &cmd : commands) {
			if (cmd != a_cmd)
				continue;

			cmd = nullptr;
			break;
		}
	}

	void SetInputPrefix(const char *pref) {
		prefix = pref;
	}

	void Run() {
		char input[MAX_INPUT_LEN];

		while (1) {
			term.Puts(NEWLINE);
			term.Puts(prefix);

			if (term.Gets(input) == nullptr)
				continue;

			if (strlen(input) == 0)
				continue;

			term.Puts(NEWLINE);
			Exec(input);
		}
	}

	int Exec(const char *input) {
		size_t len = strlen(input);
		// Клон входного буфера
		char inputBuff[MAX_INPUT_LEN + 1];

		if (len == 0)
			return -1;

		// Копирование входного буфера
		memcpy(inputBuff, input, MAX_INPUT_LEN);
		inputBuff[MAX_INPUT_LEN] = '\0';

		char *argv[MAX_ARGS];
		int argc = 0;
		int quotes = 0;
		argv[argc++] = inputBuff;
		for (char *c = inputBuff; *c != '\0'; c++) {
			if ((*c == ' ') && (quotes == 0)) {
				*c = '\0';
				argv[argc++] = c + 1;
			}
			else if (*c == '\\') {
				if (*(c + 1) == '"')
					c += 2;
			}
			else if (*c == '"') {
				*c = '\0';
				if (quotes++ == 0)
					argv[argc++] = c + 1;
				else
					quotes = 0;
			}
		}

		cmdproc::CmdDef_t *cmd = nullptr;

		// Поиск команды среди зарегистрированных
		for (auto &_cmd : commands) {
			if (_cmd == nullptr)
				continue;

			if (strcmp(_cmd->cmd, argv[0]) == 0) {
				cmd = _cmd;
				break;
			}
		}

		// Если команда не найдена
		if (cmd == nullptr) {
			term.Puts(argv[0]);
			term.Puts(": command not found. Use \"help\" to get available commands.");
			return -1;
		}

		// Определение аргументов команды
		int cmdArgcMin = 0;
		int cmdArgcMax = 0;
		if (cmd->args != nullptr)
			for (const char *c = cmd->args; *c != '\0' ; c++) {
				if ((c != cmd->args) && (*(c-1) != ' '))
					continue;

				switch (*c) {
					case '~':
						cmdArgcMax++;
						break;

					case '*':
						cmdArgcMax = 99;
						break;

					default:
						cmdArgcMin++;
						cmdArgcMax++;
					}
			}

		cmdproc::OptArgs_t cmdOptArr[MAX_ARGS] = {0, };
		char *cmdArgvArr[MAX_ARGS];
		char *optArgvArr[MAX_ARGS];
		int cmdArgN = 0;
		int optArgN = 0;

		cmdproc::OptArgs_t *cmdOpt = cmdOptArr;
		char **cmdArgv = cmdArgvArr;
		char **optArgv = optArgvArr;

		// Разбор аргументов и опций
		bool isShortOpt, isFullOpt;

		char *argS;
		for (int argn = 1; (argS = argv[argn]) != argv[argc]; argn++) {
			if (*argS == '\0')
				continue;

			// Проверка типа аргумента
			isFullOpt = strncmp(argS, "--", 2) == 0;
			if (!isFullOpt)
				isShortOpt = (strncmp(argS, "-", 1) == 0) && (strlen(argS) == 2);
			else
				isShortOpt = false;

			// Опция - помощь?
			if (isFullOpt && (strcmp(&argS[2], HELP_ARG) == 0)) {
				PrintCommandHelp(*cmd);
				return -1;
			}

			// Аргумент - опция?
			if (isShortOpt || isFullOpt)
			{
				// Аргумент - опция

				// Опция выбрана?
				if ((cmdOpt->ref != nullptr)) {
					// Опция выбрана

					// Количество аргументов опции соответствует?
					if (cmdOpt->argc < optArgN) {
						// Если не соответствует - ошибка, т.к. производится переход к следующей опции,
						// а предыдущая ещё не заполнена
						if (cmdOpt->ref->full != nullptr) {
							term.Puts("--");
							term.Puts(cmdOpt->ref->full);
						} else {
							term.Putc('-');
							term.Putc(cmdOpt->ref->ch);
						}
						term.Puts(": invalid number of option arguments. "
								  "Use \"--help\" option to get available command options.");
						return -1;
					}
					else {
						// Если соответствует - переход к заполнению следующей опции

						cmdOpt++;
					}
				}

				// Поиск опции
				for (int optN = 0; optN < cmd->optc; optN++) {
					if (
							(
									isShortOpt &&
									(argS[1] == cmd->options[optN].ch)
							) ||
							(
									isFullOpt &&
									(cmd->options[optN].full != nullptr) &&
									(strcmp(&argS[2], cmd->options[optN].full) == 0)
							)
					)
					{
						// Выбор опции
						optArgN = 0;

						cmdOpt->ref = &cmd->options[optN];
						cmdOpt->argv = optArgv;
						cmdOpt->argc = 0;

						// Подсчёт количества аргументов
						if (cmdOpt->ref->args != nullptr) {
							cmdOpt->argc++;
							for (const char *c = cmdOpt->ref->args; *c != '\0'; c++)
								if (*c == ' ')
									cmdOpt->argc++;
						}

						break;
					}
				}

				// Опция выбрана?
				if (cmdOpt->ref == nullptr) {
					// Опция не выбрана - ошибка о неизвестной опции
					term.Puts(argS);
					term.Puts(": unknown option. Use \"--help\" option to get available command options.");
					return -1;
				}
			}
			else
			{
				// Аргумент - не опция

				if ((cmdOpt->ref != nullptr) && (optArgN < cmdOpt->argc)) {
					// Опция выбрана и ей требуются аргументы

					*(optArgv++) = argS;
					optArgN++;
				}
				else if (cmdArgN < cmdArgcMax) {
					// Опция не выбрана и требуются аргументы команды

					*(cmdArgv++) = argS;
					cmdArgN++;
				}
				else {
					// Опция не выбрана и больше не требуются аргументы команды
					term.Puts("Too many arguments. Use \"--help\" option to get information about command usage.");
					return -1;
				}
			}
		}

		if ((cmdOpt->ref != nullptr)) {
			// Выбрана опция
			if ((optArgN < cmdOpt->argc)) {
				// Опции недостаточно аргументов
				if (cmdOpt->ref->full != nullptr) {
					term.Puts("--");
					term.Puts(cmdOpt->ref->full);
				} else {
					term.Putc('-');
					term.Putc(cmdOpt->ref->ch);
				}

				term.Puts(": invalid number of option arguments. "
						  "Use \"--help\" option to get available command options.");
				return -1;
			}

			// Переход к следующей опции
			cmdOpt++;
		}

		if (cmdArgN < cmdArgcMin) {
			// Аргументов команды недостаточно
			term.Puts("Missing arguments. Use \"--help\" option to get information about command usage.");
			return -1;
		}

		cmdproc::CmdArgs_t cmdArgs = {cmdArgvArr, (int)(cmdArgv - cmdArgvArr),
									  cmdOptArr, (int)(cmdOpt - cmdOptArr)};

		// Выполнение команды
		return cmd->fn(cmd->ctx, term, cmdArgs);
	}

private:
	void PrintCommandHelp(cmdproc::CmdDef_t &cmd) {
		size_t len;
		cmdproc::CmdOpt_t *opt;


		term.Puts("Usage:\r\n\t");
		term.Puts(cmd.cmd);
		term.Puts(" [options]");		// Даже если опции не заданы, всегда существует опция "--help"
		if (cmd.args != nullptr) {
			term.Puts(" <");
			for (const char *c = cmd.args; *c != '\0'; c++) {
				if (*c == ' ')
					term.Puts("> <");
				else
					term.Putc(*c);
			}
			term.Putc('>');
		}
		term.Puts("\r\n\r\n");

		if (cmd.descr != nullptr) {
			term.Puts("Description:\r\n\t");

			len = strlen(cmd.descr);
			for (int i = 0; i < len; i++)
				switch (cmd.descr[i]) {
				case '\n':
					term.Puts("\r\n\t");
					break;

				case '\r':
					break;

				default:
					term.Putc(cmd.descr[i]);
				}

			term.Puts("\r\n\r\n");
		}

		//term.Puts("Options:\r\n");
		//term.Puts("\t   --help\r\033[40CHelp for this command.\r\n");
		if (cmd.options) {
			term.Puts("Options:\r\n");

			for (int i = 0; i < cmd.optc; i++) {
				opt = &cmd.options[i];
				term.Putc('\t');

				if (opt->ch) {
					term.Putc('-');
					term.Putc(opt->ch);
				} else
					term.Puts("  ");

				if (opt->full) {
					term.Puts(" --");
					term.Puts(opt->full);
				}

				if (opt->args) {
					term.Puts(" <");
					for (const char *c = opt->args; *c != '\0'; c++) {
						if (*c == ' ')
							term.Puts("> <");
						else
							term.Putc(*c);
					}
					term.Putc('>');
				}

				if (opt->description) {
					term.Puts("\r\033[40C");
					term.Puts(opt->description);
				}

				term.Puts("\r\n");
			}
		}

	}


private:
	Terminal &term;

	const char *prefix;
	cmdproc::CmdDef_t *commands[MAX_CMD];

private:


private:
	int CmdFn_Reset() {
		char tmp[32];
		ANSI::Encode(tmp, ANSI::ERASE_DISPLAY);
		term.Puts(tmp);

		return 0;
	}

	int CmdFn_Help() {
		for (auto &cmd : commands) {
			if (cmd == nullptr)
				continue;

			term.Putc(' ');
			term.Puts(cmd->cmd);
			if (cmd->options) {
				term.Puts(" [options]");
			}
			if (cmd->args != nullptr) {
				term.Puts(" <");
				for (const char *c = cmd->args; *c != '\0'; c++) {
					if (*c == ' ')
						term.Puts("> <");
					else
						term.Putc(*c);
				}
				term.Putc('>');
			}
			term.Puts("\r\n");
		}

		return 0;
	}


private:
	cmdproc::CmdDef_t baseCmd_Reset = {
			.fn = [](void *ctx, Terminal &t, cmdproc::CmdArgs_t &a) -> int
					{ return reinterpret_cast<CommandProcessor *>(ctx)->CmdFn_Reset(); },
			.ctx = this,
			.cmd = "reset",
			.args = nullptr,
			.options = nullptr,
			.optc = 0,
			.descr = "Clean display."
	};

	cmdproc::CmdDef_t baseCmd_Help = {
			.fn = [](void *ctx, Terminal &t, cmdproc::CmdArgs_t &a) -> int
					{ return reinterpret_cast<CommandProcessor *>(ctx)->CmdFn_Help(); },
			.ctx = this,
			.cmd = "help",
			.args = nullptr,
			.options = nullptr,
			.optc = 0,
			.descr = "Display all commands."
	};

};



#endif /* __COMMAND_PROCESSOR_H__ */