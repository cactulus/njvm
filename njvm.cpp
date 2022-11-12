void string_print(String str) {
    printf("%.*s", str.length, str.data);
}

void string_println(String str) {
    printf("%.*s\n", str.length, str.data);
}


struct Backend {
    u8 inst_types[220];
    Method *method;
    Class *clazz;
    u8 *ip;
    u8 sp;

    Backend(Class *main_class) {
        clazz = main_class;

        char s[] = "AAAAAAAAAAAAAAAABCLMMDDDDDEEEEEEEEEEEEEEEEEEEEAAAAAAAADD"
                   "DDDEEEEEEEEEEEEEEEEEEEEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                   "AAAAAAAAAAAAAAAAANAAAAAAAAAAAAAAAAAAAAJJJJJJJJJJJJJJJJDOPAA"
                   "AAAAGGGGGGGHIFBFAAFFAARQJJKKJJJJJJJJJJJJJJJJJJ";

        for (int i = 0; i < 220; ++i) {
            inst_types[i] = (u8) (s[i] - 'A');
        }
    }

    virtual void run() = 0;

    Method *find_method(String class_name, String name) {
        if (class_name != clazz->name) {
            return 0;
        }

        for (u16 i = 0; i < clazz->methods_count; ++i) {
            Method *m = &clazz->methods[i];
            if (m->name == name) {
                return m;
            }
        }

        return 0;
    }

    Method *find_method(String name) {
        for (u16 i = 0; i < clazz->methods_count; ++i) {
            Method *m = &clazz->methods[i];
            if (m->name == name) {
                return m;
            }
        }

        printf("Can't find function %.*s\n", name.length, name.data);
        return 0;
    }

    /* Finds class in current class only */
    Method *find_method(const char *mname) {
        for (u16 i = 0; i < clazz->methods_count; ++i) {
            Method *m = &clazz->methods[i];

            if (m->name == mname) {
                return m;
            }
        }

        printf("Can't find method '%s'\n", mname);
        return 0;
    }

    Code find_code(Method *m) {
        if (m->code.code != 0)
            return m->code;

        Code info;

        for (u16 i = 0; i < m->attributes_count; ++i) {
            Attribute *a = &m->attributes[i];
            if (a->name == "Code") {
                Reader r(a->info, a->attribute_length);
                info.max_stack = r.read_u16();
                info.max_locals = r.read_u16();
                info.code_length = r.read_u32();

                info.code = (u8 *) malloc(info.code_length);
                for (u32 i = 0; i < info.code_length; ++i) {
                    info.code[i] = r.read_u8();
                }
            }
        }

        m->code = info;
        return info;
    }

    CP_Info get_cp_info(u16 index) {
        return clazz->constant_pool[index - 1];
    }

    CP_Info get_class_name(u16 class_index) {
        CP_Info ci = get_cp_info(class_index);
        return get_cp_info(ci.name_index);
    }

    CP_Info get_member_name(u16 name_and_type_index) {
        CP_Info ci = get_cp_info(name_and_type_index);
        return get_cp_info(ci.name_index);
    }

    u8 fetch_u8() {
        return *ip++;
    }

    u16 fetch_u16() {
        u16 f = fetch_u8();
        u16 s = fetch_u8();
        return (f << 8) | s;
    }

    u16 base_offset() {
        return ip - method->code.code - 1;
    }
};