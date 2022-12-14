#include <cassert>
#include <cstdlib>
#include <cstring>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/MCJIT.h>
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
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

#ifdef _WIN32
#include <llvm/Support/TargetRegistry.h>
#else
#include <llvm/MC/TargetRegistry.h>
#endif

#include "testing/testing.h"
#include "common.h"

#include "constants.h"
#include "info.h"

#include "reader.cpp"
#include "njvm.cpp"
#include "jit.cpp"
#include "interpreter.cpp"

NType *type_void;
NType *type_bool;
NType *type_byte;
NType *type_short;
NType *type_int;
NType *type_long;

NType *make_primitive(NType::BaseType ty) {
  NType *type = new NType();
	type->type = ty;
	return type;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: njvm <CLASS-FILE>");
        return EXIT_FAILURE;
    }

    const char *class_file = argv[1];

  type_bool = make_primitive(NType::BOOL);
  type_byte = make_primitive(NType::BYTE);
  type_short = make_primitive(NType::SHORT);
  type_int = make_primitive(NType::INT);
    type_long = make_primitive(NType::LONG);
    type_void = make_primitive(NType::VOID);


	ClassReader cr(class_file);
	Class *clazz = cr.read();

  jit::Jit jit(clazz);
  jit.run();

	return 0;
}

