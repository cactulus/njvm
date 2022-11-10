#include <cassert>
#include <cstdlib>
#include <cstring>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include "llvm/IR/Verifier.h"
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

#include "common.h"

#include "constants.h"
#include "info.h"

#include "reader.cpp"
#include "njvm.cpp"
#include "jit.cpp"
#include "interpreter.cpp"

NType *type_void;
NType *type_int;

NType *make_primitive(NType::BaseType ty) {
    NType *type = new NType();
	type->type = ty;
	return type;
}

int main() {
	type_int = make_primitive(NType::INT);
	type_void = make_primitive(NType::VOID);

    /* Temp for tests */
    system("javac HelloWorld.java");
    const char *class_file = "HelloWorld.class";

	ClassReader cr(class_file);
	Class *clazz = cr.read();

    /*
    Interpreter interp(clazz);
    interp.run();
*/
    jit::Jit jit(clazz);
    jit.run();

	return 0;
}

