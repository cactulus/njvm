#include <cassert>
#include <cstdlib>
#include <cstring>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

#include "common.h"

#include "constants.h"
#include "info.h"

#include "reader.cpp"
#include "njvm.cpp"
#include "njit.cpp"

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

    /* Temp for tests */
    system("javac HelloWorld.java");
    const char *class_file = "HelloWorld.class";

	ClassReader cr(class_file);
	Class *clazz = cr.read();

    /*
	NJVM vm(clazz);
	vm.run();*/

    NJIT jit(clazz);
    jit.run();

	return 0;
}

