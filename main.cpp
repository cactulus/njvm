#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "constants.h"
#include "info.h"

#include "reader.cpp"
#include "njvm.cpp"

int main() {
	Reader r("HelloWorld.class");
	Class_File cf;

	read_class_file(&r, &cf);

	NJVM vm(&cf);
	vm.run();

	return 0;
}

