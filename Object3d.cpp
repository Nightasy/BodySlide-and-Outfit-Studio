#include "Object3d.h"

float h2float(const ushort& in) {
	float ret;
	uint t1;
	uint t2;
	uint t3;

	t1 = in & 0x7fff;
	t2 = in & 0x8000;
	t3 = in & 0x7c00;

	t1 <<= 13;
	t2 <<= 16;

	t1 += 0x38000000;
	t1 = (t3 == 0 ? 0 : t1);
	t1 |= t2;

	*((uint*)&ret) = t1;
	return ret;
}

unsigned short float2h(const float& in) {
	ushort ret;

	uint c = *((uint*)&in);

	uint t1, t2, t3;
	t1 = c & 0x7fffffff;
	t2 = c & 0x80000000;
	t3 = c & 0x7f800000;

	t1 >>= 13;
	t2 >>= 16;
	t1 -= 0x1c000;

	t1 = (t3 < 0x38800000 ? 0 : t1);
	t1 = (t3 > 0x8e000000 ? 0x7bff : t1);

	t1 |= t2;
	ret = t1;
	return ret;
}
