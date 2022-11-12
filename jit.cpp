extern "C" {
    void print_int(int a) {
        printf("%d\n", a);
    }
}


namespace jit {
    using namespace llvm;
    using namespace llvm::orc;

#define STR_REF(x) StringRef((const char * ) x.data, x.length)

    struct ControlFlow {
        Array<u16> offsets;
        Array<BasicBlock *> blocks;

        void add(u16 offset, BasicBlock *block) {
            offsets.add(offset);
            blocks.add(block);
        }

        BasicBlock *find(u16 offset) {
            for (s64 i = 0; i < offsets.length; ++i)
                if (offsets[i] == offset)
                    return blocks[i];

            return 0;
        }
    };

    struct Jit : Backend {
        LLVMContext context;
        std::unique_ptr<Module> module;
        IRBuilder<> *irb;
        Value **stack;
        Value **locals;

        ControlFlow control_flow;

        Type *llty_i8;
        Type *llty_i32;
        Type *llty_void;

        // TODO: move somewhere else later
        Function *print_int_fn = 0;

        Jit(Class *main_clazz) : Backend(main_clazz) {
            InitializeAllTargetInfos();
            InitializeAllTargets();
            InitializeAllTargetMCs();
            InitializeAllAsmParsers();
            InitializeAllAsmPrinters();

            module = make_unique<Module>("njit", context);
            irb = new IRBuilder<>(context);

            llty_i8 = Type::getInt8Ty(context);
            llty_i32 = Type::getInt32Ty(context);
            llty_void = Type::getVoidTy(context);

            // TODO: move somewhere else later
            auto print_int_fn_ty = FunctionType::get(llty_void, {llty_i32}, false);
            print_int_fn = Function::Create(print_int_fn_ty, Function::ExternalLinkage, "print_int", *module);
        }

        void run() override {
            for (u16 i = 0; i < clazz->methods_count; ++i) {
                convert_method(&clazz->methods[i]);
            }

            finalize();
        }

        void finalize() {
            // optimize();
            module->print(outs(), 0);
            if (verifyModule(*module, &outs())) {
                return;
            }

            EngineBuilder eb = EngineBuilder(std::move(module));

            ExecutionEngine *ee = eb.create();

            void (*print_int_ptr)(int) = print_int;
            ee->addGlobalMapping(print_int_fn, (void *)print_int_ptr);

            void (*main)() = (void (*)())(intptr_t) ee->getFunctionAddress("main");
            main();
        }

        void SetInsertBlock(BasicBlock *bb) {
            if (!irb->GetInsertBlock()->getTerminator()) {
                irb->CreateBr(bb);
            }

            irb->SetInsertPoint(bb);
        }

        void convert_opcode() {
            u8 opcode = fetch_u8();

            u8 inst_type = inst_types[opcode];
            if (inst_type == INST_LABEL || inst_type == INST_LABELW) {
                BasicBlock *bb = get_or_create_block(base_offset());
                SetInsertBlock(bb);
            } else {
                BasicBlock *bb = control_flow.find(base_offset());
                if (bb) {
                    SetInsertBlock(bb);
                }
            }

            switch (opcode) {
                case OP_ICONST_0:
                case OP_ICONST_1:
                case OP_ICONST_2:
                case OP_ICONST_3:
                case OP_ICONST_4:
                case OP_ICONST_5: {
                    push(make_int(opcode - 3));
                } break;
                case OP_BIPUSH: {
                    push(make_int(fetch_u8()));
                } break;
                case OP_SIPUSH: {
                    push(make_int(fetch_u16()));
                } break;
                case OP_ILOAD: {
                    load(fetch_u8());
                } break;
                case OP_ILOAD_0:
                case OP_ILOAD_1:
                case OP_ILOAD_2:
                case OP_ILOAD_3: {
                    load(opcode - 0x1a);
                } break;
                case OP_ISTORE: {
                    store(fetch_u8());
                } break;
                case OP_ISTORE_0:
                case OP_ISTORE_1:
                case OP_ISTORE_2:
                case OP_ISTORE_3: {
                    store(opcode - 0x3b);
                } break;
                case OP_POP: {
                    pop();
                } break;
                case OP_DUP: {
                    Value *val = pop();
                    push(val);
                    push(val);
                } break;
                case OP_IADD:
                case OP_ISUB:
                case OP_IMUL:
                case OP_IDIV:
                case OP_IREM:
                case OP_ISHL:
                case OP_ISHR:
                case OP_IAND:
                case OP_IOR: {
                    Value *l = pop();
                    Value *r = pop();
                    Instruction::BinaryOps op;

                    switch (opcode) {
                        case OP_IADD: op = Instruction::BinaryOps::Add; break;
                        case OP_ISUB: op = Instruction::BinaryOps::Sub; break;
                        case OP_IMUL: op = Instruction::BinaryOps::Mul; break;
                        case OP_IDIV: op = Instruction::BinaryOps::SDiv; break;
                        case OP_IREM: op = Instruction::BinaryOps::SRem; break;
                        case OP_ISHL: op = Instruction::BinaryOps::AShr; break;
                        case OP_ISHR: op = Instruction::BinaryOps::AShr; break;
                        case OP_IAND: op = Instruction::BinaryOps::And; break;
                        case OP_IOR: op = Instruction::BinaryOps::Or; break;
                    }

                    push(irb->CreateBinOp(op, l, r));
                } break;
                case OP_IFEQ:
                case OP_IFNE:
                case OP_IFLT:
                case OP_IFGE:
                case OP_IFGT:
                case OP_IFLE: {
                    Value *zero = make_int(0);
                    Value *val = pop();
                    u16 off = base_offset() + fetch_u16();
                    CmpInst::Predicate op;

                    switch (opcode) {
                        case OP_IFEQ: op = ICmpInst::ICMP_EQ; break;
                        case OP_IFNE: op = ICmpInst::ICMP_NE; break;
                        case OP_IFLT: op = ICmpInst::ICMP_SLT; break;
                        case OP_IFGE: op = ICmpInst::ICMP_SGE; break;
                        case OP_IFGT: op = ICmpInst::ICMP_SGT; break;
                        case OP_IFLE: op = ICmpInst::ICMP_SLE; break;
                    }

                    create_cond_jump(op, val, zero, off);
                } break;
                case OP_IF_ICMPEQ:
                case OP_IF_ICMPNE:
                case OP_IF_ICMPLT:
                case OP_IF_ICMPGE:
                case OP_IF_ICMPGT:
                case OP_IF_ICMPLE: {
                    Value *l = pop();
                    Value *r = pop();
                    u16 off = base_offset() + fetch_u16();

                    CmpInst::Predicate op;

                    switch (opcode) {
                        case OP_IF_ICMPEQ: op = ICmpInst::ICMP_EQ; break;
                        case OP_IF_ICMPNE: op = ICmpInst::ICMP_NE; break;
                        case OP_IF_ICMPLT: op = ICmpInst::ICMP_SLT; break;
                        case OP_IF_ICMPGE: op = ICmpInst::ICMP_SGE; break;
                        case OP_IF_ICMPGT: op = ICmpInst::ICMP_SGT; break;
                        case OP_IF_ICMPLE: op = ICmpInst::ICMP_SLE; break;
                    }

                    create_cond_jump(op, l, r, off);
                } break;
                case OP_INEG: {
                    Value *v = pop();
                    v = irb->CreateNeg(v);
                    push(v);
                } break;
                case OP_IINC: {
                    u8 index = fetch_u8();
                    Value *local = load_local(index);
                    Value *added = irb->CreateAdd(local, make_int(1));
                    store(index, added);
                } break;
                case OP_RETURN:
                    irb->CreateRetVoid();
                    break;
                case OP_IRETURN:
                    irb->CreateRet(pop());
                    break;
                case OP_GETSTATIC: {
                    u16 field_index = fetch_u16();

                    CP_Info field_ref = get_cp_info(field_index);
                    CP_Info class_name = get_class_name(field_ref.class_index);
                    CP_Info member_name = get_member_name(field_ref.name_and_type_index);

                    /* TODO:  */
                    sp++;
                } break;
                case OP_INVOKEVIRTUAL: {
                    u16 method_index = fetch_u16();

                    CP_Info method_ref = get_cp_info(method_index);
                    CP_Info class_name = get_class_name(method_ref.class_index);
                    CP_Info member_name = get_member_name(method_ref.name_and_type_index);

                    if (class_name.utf8 == "java/io/PrintStream" && member_name.utf8 == "println") {
                        call(module->getFunction("print_int"), 1, true);
                    } else {
                        Method *m = find_method(class_name.utf8, member_name.utf8);
                        if (m) {
                            call(m, true);
                        }
                    }
                } break;
                case OP_INVOKESPECIAL: {
                    u16 method_index = fetch_u16();
                    CP_Info method_ref = get_cp_info(method_index);
                    CP_Info class_name = get_class_name(method_ref.class_index);
                    CP_Info member_name = get_member_name(method_ref.name_and_type_index);

                    Method *m = find_method(class_name.utf8, member_name.utf8);
                    if (m) {
                        call(m, true);
                    }
                } break;
                case OP_INVOKESTATIC: {
                    u16 method_index = fetch_u16();

                    CP_Info method_ref = get_cp_info(method_index);
                    CP_Info member_name = get_member_name(method_ref.name_and_type_index);

                    Method *m = find_method(member_name.utf8);
                    if (m) {
                        call(m, false);
                    }
                } break;
                case OP_NEW: {
                    u16 index = fetch_u16();

                    CP_Info constant_clazz = get_cp_info(index);
                    CP_Info class_name = get_cp_info(constant_clazz.name_index);

                    // push(make_object(class_name.utf8));
                } break;
            }
        }

        void call(Method *m, bool on_object) {
            if (!m->llvm_ref) {
                convert_method(m);
            }

            Value *ret_val = call(m->llvm_ref, m->type->parameters.length, on_object);

            if (m->type->return_type->type == NType::INT) {
                push(ret_val);
            }
        }

        Value *call(Function *f, s64 arg_count, bool on_object) {
            Array<Value *> args;
            for (u16 i = 0; i < arg_count; ++i) {
                args.add(pop());
            }

            if (on_object) {
                sp--;
            }

            return irb->CreateCall(f, ArrayRef(args.data, args.length));;
        }

        Function *convert_method(Method *m) {
            Function *fn = convert_function_header(m);
            m->llvm_ref = fn;
            Code ci = find_code(m);
            method = m;

            function_setup(ci);

            u16 i = 0;
            for (auto &arg : fn->args()) {
                if (m->type->parameters[i]->type == NType::INT) {
                    store(i, &arg);
                }
                ++i;
            }

            while (ip < ci.code + ci.code_length) {
                convert_opcode();
            }

            return fn;
        }

        Function *convert_function_header(Method *m) {
            Type *ret_type = convert_type(m->type->return_type);

            Array<Type *> params;
            for (auto pty: m->type->parameters) {
                if (pty->type == NType::INT) {
                    params.add(convert_type(pty));
                }
            }

            String fn_name = m->name;
            if (fn_name != "main") {
                fn_name = clazz->name + to_string(".") + fn_name;
            }

            auto fty = FunctionType::get(ret_type, ArrayRef(params.data, params.length), false);
            auto fn = Function::Create(fty, Function::ExternalLinkage, STR_REF(fn_name), *module);

            BasicBlock *bb = BasicBlock::Create(context, "", fn);
            irb->SetInsertPoint(bb);

            return fn;
        }

        void function_setup(Code ci) {
            stack = (Value **) malloc(ci.max_stack * sizeof(Value *));
            locals = (Value **) malloc(ci.max_locals * sizeof(Value *));
            ip = ci.code;
            sp = 0;

            for (u16 i = 0; i < ci.max_stack; ++i) {
                AllocaInst *a = irb->CreateAlloca(llty_i32, 0, "s" + std::to_string(i));
                stack[i] = a;
            }

            for (u16 i = 0; i < ci.max_locals; ++i) {
                AllocaInst *a = irb->CreateAlloca(llty_i32, 0, "l" + std::to_string(i));
                locals[i] = a;
            }
        }

        Type *convert_type(NType *type) {
            switch (type->type) {
                case NType::ARRAY:
                    return convert_type(type->element_type)->getPointerTo();
                case NType::CLASS: {
                    if (type->clazz_name == "java/lang/String") {
                        return llty_i8->getPointerTo();
                    }
                    assert(0 && "Class not implemented");
                    return 0;
                }
                case NType::INT:
                    return llty_i32;
                case NType::VOID:
                    return llty_void;
            }

            assert(0 && "Type conversion not implemented");
            return 0;
        }

        void create_cond_jump(CmpInst::Predicate op, Value *l, Value * r, u16 off) {
            BasicBlock *after = BasicBlock::Create(context, "", method->llvm_ref);

            BasicBlock *target = get_or_create_block(off);

            Value *cmp = irb->CreateICmp(op, l, r);
            irb->CreateCondBr(cmp, after, target);

            SetInsertBlock(after);
        }

        BasicBlock *get_or_create_block(u16 off) {
            BasicBlock * bb = control_flow.find(off);

            if (!bb) {
                bb = BasicBlock::Create(context, "", method->llvm_ref);
                control_flow.add(off, bb);
            }

            return bb;
        }

        void push(Value *val) {
            irb->CreateStore(val, stack[sp++]);
        }

        Value *pop() {
            return irb->CreateLoad(stack[--sp]);
        }

        void store(u8 index, Value *value) {
            irb->CreateStore(value, locals[index]);
        }

        void store(u8 index) {
            store(index, pop());
        }

        void load(u8 index) {
            push(load_local(index));
        }

        Value *load_local(u8 index) {
            return irb->CreateLoad(locals[index]);
        }

        void optimize() {
            legacy::PassManager *pm = new legacy::PassManager();
            PassManagerBuilder pmb;
            pmb.OptLevel = 2;
            pmb.SizeLevel = 0;
            pmb.DisableUnrollLoops = false;
            pmb.LoopVectorize = true;
            pmb.SLPVectorize = true;
            pmb.populateModulePassManager(*pm);
            pm->run(*module);
        }

        Value *make_int(s32 v) {
            return ConstantInt::get(llty_i32, v);
        }
    };
}