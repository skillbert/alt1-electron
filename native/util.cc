#include "util.h"

//TODO this should never be needed
void flipBGRAtoRGBA(void* data, size_t len) {
	byte* index = (byte*)data;
	byte* end = index + len;
	for (; index < end; index += 4) {
		unsigned char tmp = index[0];
		//TODO profile this, does the compiler do fancy simd swizzles if we self assign 1 and 3 as well?
		index[0] = index[2];
		index[2] = tmp;
	}
}

void flipBGRAtoRGBA(void* outdata, void* indata, size_t len) {
	byte* inbytes = (byte*)indata;
	byte* outbytes = (byte*)outdata;
	for (size_t i = 0; i < len; i += 4) {
		outbytes[i + 0] = inbytes[i + 2];
		outbytes[i + 1] = inbytes[i + 1];
		outbytes[i + 2] = inbytes[i + 0];
		outbytes[i + 3] = inbytes[i + 3];
	}
}
