#include "utils.h"

int strlen(const char *s) 
{
	int len = 0;
	while (s[len])
		++len;
	return len;
}