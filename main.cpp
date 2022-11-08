#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

#include "constants.h"
#include "info.h"

#include "reader.cpp"
#include "njvm.cpp"

Type *type_void;
Type *type_int;

Type *make_primitive(Type::BaseType ty) {
	Type *type = new Type();
	type->type = ty;
	return type;
}

int main() {
	type_int = make_primitive(Type::INT);
	type_void = make_primitive(Type::VOID);

	ClassReader cr("HelloWorld.class");
	Class *clazz = cr.read();

	NJVM vm(clazz);
	vm.run();

	return 0;
}

