#ifndef __PARALLEL_STREAM_H__
#define __PARALLEL_STREAM_H__

#include <stddef.h>


class ParallelStream {
public:
	//virtual int Receive(uint8_t *p, size_t len) = 0;

	virtual int Write(const char *p, size_t len) = 0;

	virtual int WriteByte(char c) = 0;

	virtual int ReadByte(char *c, size_t timeoutMs) = 0;
};


#endif /* __PARALLEL_STREAM_H__ */