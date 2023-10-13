
#include <stdio.h>
#include <cstdlib>
#include <sys/stat.h>

extern "C" {
#include "xmodem.h"
}

#include "spstream.h"
#include "terminal.h"
#include "cmdproc.h"


SerialPortStream stream("/dev/ttyUSB0", SerialPortStream::SPEED_115200);
Terminal term(stream);
CommandProcessor proc(term);


int _inbyte(unsigned short t) {
	return term.Getc(t);
}

void _outbyte(int c) {
	term.Putc((char)c);
}





int CmdFn_YmodemReceive(void *ctx, Terminal &t, cmdproc::CmdArgs_t &a);
cmdproc::CmdOpt_t RyCmdOpts[] = {
		{
				.ch = 'r',
				.full = "rename",
				.args = "filename",
				.description = "Rename the received file.",
		},
};
cmdproc::CmdDef_t RyCmd = {
		.fn = CmdFn_YmodemReceive,
		.ctx = nullptr,
		.cmd = "ry",
		.args = "path",
		.options = RyCmdOpts,
		.optc = sizeof(RyCmdOpts) / sizeof(cmdproc::CmdOpt_t) ,
		.descr = "Receiving a file via the YMODEM protocol\r\n"
				 "(XMODEM and ZMODEM are not supported)\r\n"
				 "\r\n"
				 "Special commands:\r\n"
				 "  \"ry -r .fs \\\" - Overwriting the file system (littlefs binary).\r\n"
				 "\r\n"
				 "* The recommended command for sending a file from a UNIX system is:\r\n"
				 "  \"sz -vv --binary --ymodem\"\r\n"
				 "\r\n"
				 "Notes:\n"
				 "  Before writing a file system, it must be unmounted,\r\n"
				 "  and after writing it must be mounted. During a file system\r\n"
				 "  write, flash memory sectors are erased, which takes time.\r\n"
};


int CmdFn_YmodemTransmit(void *ctx, Terminal &t, cmdproc::CmdArgs_t &a);
cmdproc::CmdOpt_t SyCmdOpts[] = {
		{
				.ch = 'r',
				.full = "rename",
				.args = "filename",
				.description = "Rename the file on the recipient side.",
		},
};
cmdproc::CmdDef_t SyCmd = {
		.fn = CmdFn_YmodemTransmit,
		.ctx = nullptr,
		.cmd = "sy",
		.args = "path",
		.options = SyCmdOpts,
		.optc = sizeof(SyCmdOpts) / sizeof(cmdproc::CmdOpt_t) ,
		.descr = "Sending a file via the YMODEM protocol\r\n"
				 "(XMODEM and ZMODEM are not supported)\r\n"
				 "\r\n"
				 "Special commands:\r\n"
				 "  \"sy .fs\" - Send file system (littlefs binary)\r\n"
				 "  \"sy .key\" - Sending the contents of the MU key.\r\n"
				 "\r\n"
				 "* The recommended command for sending a file from a UNIX system is:\r\n"
				 "  \"rz -vv --binary --ymodem --rename\""
};



int wfd;
void StoreChunk(void *funcCtx, void *xmodemBuffer, int xmodemSize) {
	//printf("%s", (char *)xmodemBuffer);
	write(wfd, xmodemBuffer, xmodemSize);
}

int sfd;
void FetchChunk(void *funcCtx, void *xmodemBuffer, int xmodemSize) {
	//printf("%s", (char *)xmodemBuffer);
	read(sfd, xmodemBuffer, xmodemSize);
}


int CmdFn_YmodemReceive(void *ctx, Terminal &t, cmdproc::CmdArgs_t &a) {
	int res;
	char chunk[128];

	char *renamedFileName = nullptr;

	cmdproc::OptArgs_t *opt = a.opts;
	for (int i = 0; i < a.optc; i++, opt++)
		switch (opt->ref->ch) {
		case 'r':
			renamedFileName = opt->argv[0];
			break;
		}

	// header
	res = XmodemReceive(nullptr, chunk, sizeof(chunk), 1, 1);
	printf("res: %d\n", res);

	char *yCtrlPacket = chunk;
	char *fileName = yCtrlPacket;

	if (renamedFileName != nullptr)
		fileName = renamedFileName;

	wfd = open(fileName, O_WRONLY | O_CREAT);

	char *fileStat = &yCtrlPacket[strlen(yCtrlPacket) + 1];
	for (char *c = fileStat; *c != '\0'; c++)
		if (*c == ' ')
			*c = '\0';

	char *fileSize = fileStat;

	printf("Filename: %s\n", fileName);
	printf("size: %s\n", fileSize);

	// content
	res = XmodemReceive(StoreChunk, chunk, atoi(fileSize), 1, 0);
	printf("res: %d\n", res);

	// end
	res = XmodemReceive(nullptr, chunk, sizeof(chunk), 1, 1);
	printf("res: %d\n", res);

	close(wfd);

	return 0;
}


int CmdFn_YmodemTransmit(void *ctx, Terminal &t, cmdproc::CmdArgs_t &a) {
	int res;
	char chunk[128];

	char *filename = a.argv[0];

	cmdproc::OptArgs_t *opt = a.opts;
	for (int i = 0; i < a.optc; i++, opt++)
		switch (opt->ref->ch) {
			case 'r':	// --rename
				filename = opt->argv[0];
				break;
		}

	sfd = open(a.argv[0], O_RDONLY);
	struct stat stat;

	fstat(sfd, &stat);

	memset(chunk, 0, sizeof(chunk));
	sprintf(chunk, "%s", filename);
	sprintf(&chunk[strlen(chunk)+1], "%ld", stat.st_size);
	// header
	res = XmodemTransmit(nullptr, chunk, 128, 0, 1);

	// content
	res = XmodemTransmit(FetchChunk, ctx, stat.st_size, 1, 0);

	// end
	memset(chunk, 0, sizeof(chunk));
	res = XmodemTransmit(nullptr, chunk, 128, 0, 1);

	close(sfd);

	return 0;
}


int TestFunction(void *ctx, Terminal &t, cmdproc::CmdArgs_t &a) {
	t.Puts("Args:\r\n");
	for (int i = 0; i < a.argc; i++) {
		t.Puts("  ");
		t.Puts(a.argv[i]);
		t.Puts("\r\n");
	}
	t.Puts("\r\n");

	if (a.optc)
		t.Puts("Options:\r\n");
	for (int i = 0; i < a.optc; i++) {
		if (a.opts[i].ref->full != nullptr) {
			t.Puts("  --");
			t.Puts(a.opts[i].ref->full);
		} else {
			t.Puts("  -");
			t.Putc(a.opts[i].ref->ch);
		}
		t.Puts("\r\n");

		if (a.opts[i].argc)
			t.Puts("    Args:\r\n");
		for (int n = 0; n < a.opts[i].argc; n++) {
			t.Puts("      ");
			t.Puts(a.opts[i].argv[n]);
			t.Puts("\r\n");
		}
		t.Puts("\r\n");
	}

	return 0;
}



cmdproc::CmdOpt_t TestCmdOptions[] = {
		{
				.ch = '1',
				.full = "opt1",
				.description = "Option 1",
		},
		{
				.ch = '2',
				.full = nullptr,
				.args = "arg1",
				.description = "Option 2",
		},
		{
				.ch = '\0',
				.full = "opt3",
				.description = nullptr,
		},
		{
				.ch = '4',
				.full = "opt4",
				.args = "arg1 arg2",
				.description = "Option 4",
		}
};

cmdproc::CmdDef_t TestCmd = {
		.fn = TestFunction,
		.ctx = nullptr,
		.cmd = "test",
		.args = "string *strings",
		.options = TestCmdOptions,
		.optc = sizeof(TestCmdOptions) / sizeof(cmdproc::CmdOpt_t),
		.descr = "Test terminal. \n<string> - String for print\nend line."
};



const char *autocompTable[] = {
		"help",
		"reset",
		"test",
		"sy",
		"ry",
};


int main() {
	static char prefix[64];
	ANSI::Encode(prefix, ANSI::STYLE, ANSI::STYLE_BOLD, ANSI::STYLE_FG_GREEN, ANSI::STYLE_END);
	sprintf(&prefix[strlen(prefix)], "ktrc");
	ANSI::Encode(&prefix[strlen(prefix)], ANSI::STYLE, ANSI::STYLE_RESET, ANSI::STYLE_END);
	sprintf(&prefix[strlen(prefix)], "# ");

	proc.SetInputPrefix(prefix);

	proc.Register(RyCmd);
	proc.Register(SyCmd);
	proc.Register(TestCmd);

	/*
	term.historyWriteNewest("hello world 1");
	term.historySaveNewest();
	term.historyWriteNewest("hello world 2");
	term.historySaveNewest();
	term.historyWriteNewest("hello world 3");
	term.historySaveNewest();
	term.historyWriteNewest("hello world 4");
	term.historySaveNewest();
	term.historyWriteNewest("a?");
	term.historySaveNewest();

	printf("f. %s\n", term.historyForward());

	printf("b. %s\n", term.historyBack());
	printf("b. %s\n", term.historyBack());
	printf("b. %s\n", term.historyBack());
	printf("b. %s\n", term.historyBack());
	printf("b. %s\n", term.historyBack());
	printf("b. %s\n", term.historyBack());

	printf("f. %s\n", term.historyForward());
	printf("f. %s\n", term.historyForward());
	printf("f. %s\n", term.historyForward());
	printf("f. %s\n", term.historyForward());
	printf("f. %s\n", term.historyForward());
	printf("f. %s\n", term.historyForward());

	printf("b. %s\n", term.historyBack());
	printf("f. %s\n", term.historyForward());
	*/

	//term.SetAutocomplete(autocompTable, sizeof(autocompTable) / sizeof(size_t));


	//proc.Exec("test -1 -2 arg1 --opt3 -4 arg2 arg3 \"argument 1\" \"argument 2\"");
	//proc.Exec("test arg4 -1");

	//CmdFn_YmodemReceive(nullptr, term, 0, nullptr);

	proc.Run();

	return 0;
}
