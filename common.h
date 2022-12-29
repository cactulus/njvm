#ifndef COMMON_H_
#define COMMON_H_

#include <assert.h>
#include <cstdarg>
#include <cstdio>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

struct String {
	u8 *data = 0;
	u16 length = 0;

	String() {
		data = 0;
	}

	char operator[](u16 index) const {
		assert(index >= 0 && index < length);

		return data[index];
	}

	String substring(u16 start, u16 slen) {
    assert(start < length && start+slen <= length);

    String s;
    s.data = data + start;
    s.length = slen;
    return  s;
  }
};

inline String to_string(const char *c_string) {
  String s;
  s.data = (u8 *) c_string;
  s.length = strlen(c_string);
  return s;
}

inline char *to_c_string(String s) {
  auto length = s.length;

  char *mem = (char *)malloc(length + 1);
  memcpy(mem, s.data, length);
  mem[s.length] = 0;
  return mem;
}

inline String operator+(const String &s, const String &t) {
  String r;
  r.length = s.length + t.length,
  r.data = (u8 *) malloc(r.length);
  memcpy(r.data, s.data, s.length);
  memcpy(r.data + s.length, t.data, t.length);
  return r;
}

inline bool operator==(const String &s, const String &t) {
  if (s.length != t.length) return false;
  if (s.data == 0 && t.data != 0) return false;
  if (t.data == 0 && s.data != 0) return false;
  if (s.data == 0 && t.data == 0) return true;

  for (s64 i = 0; i < s.length; ++i) {
      if (s[i] != t[i]) return false;
  }

  return true;
}

inline bool operator==(const String &s, const char *t) {
  auto tlen = strlen(t);
  if (s.length != tlen) return false;
  if (s.data == 0 && t != 0) return false;
  if (t == 0 && s.data != 0) return false;
  if (s.data == 0 && t == 0) return true;

  for (s64 i = 0; i < s.length; ++i) {
      if (s[i] != t[i]) return false;
  }

  return true;
}

inline bool operator!=(const String &s, const String &t) {
  return !(s == t);
}

inline String copy_string(String s) {
  String out;
  out.length = s.length;

  auto length = s.length;
  if (s.data && s.length) {
      out.data = (u8 *)malloc(length);
      memcpy(out.data, s.data, length);
  }
  return out;
}

inline String basepath(String s) {
  while (s.length && (s[s.length-1] == '/' || s[s.length-1] == '\\')) {
      s.length--;
  }
  while (s.length) {
      if (s[s.length-1] == '/' || s[s.length-1] == '\\') return s;

      s.length--;
  }

  return s;
}

inline String basename(String s) {
  auto length = s.length;

  s64 skip = 0;
  while (length && (s[length-1] == '/' || s[length-1] == '\\')) {
      length--;
      skip++;
  }

  while (length) {
      if (s[length-1] == '/' || s[length-1] == '\\') {
          String out;
          out.data   = s.data + length;
          out.length = s.length - (length + skip);
          return out;
      }

      length--;
  }

  return s;
}

inline void convert_to_back_slashes(char *c) {
  while (*c) {
      if (*c == '/') {
          *c = '\\';
      }

      ++c;
  }
}

inline void convert_to_forward_slashes(char *c) {
  while (*c) {
      if (*c == '\\') {
          *c = '/';
      }

      ++c;
  }
}

template<typename T>
struct Array {
  T *data = 0;
  s64 length = 0;
  s64 allocated = 0;

  const int NEW_MEM_CHUNK_ELEMENT_COUNT =  16;

  Array(s64 reserve_amount = 0) {
      reserve(reserve_amount);
  }

  ~Array() {
      reset();
  }

  void reserve(s64 amount) {
      if (amount <= 0) amount = NEW_MEM_CHUNK_ELEMENT_COUNT;
      if (amount <= allocated) return;

      T *new_mem = (T *)malloc(amount * sizeof(T));

      if (data) {
          memcpy(new_mem, data, length * sizeof(T));
          free(data);
      }

      data = new_mem;
      allocated = amount;
  }

  void resize(s64 amount) {
    reserve(amount);
    length = amount;
  }

  void add(T element) {
      if (length+1 >= allocated) reserve(allocated * 2);

      data[length] = element;
      length += 1;
  }

  T unordered_remove(s64 index) {
      assert(index >= 0 && index < length);
      assert(length);

      T last = pop();
      if (index < length) {
          (*this)[index] = last;
      }

      return last;
  }

  T ordered_remove(s64 index) {
      assert(index >= 0 && index < length);
      assert(length);

      T item = (*this)[index];
      memmove(data + index, data + index + 1, ((length - index) - 1) * sizeof(T));

      length--;
      return item;
  }

  T pop() {
      assert(length > 0);
      T result = data[length-1];
      length -= 1;
      return result;
  }

  void clear() {
      length = 0;
  }

  void reset() {
      length = 0;
      allocated = 0;

      if (data) free(data);
      data = 0;
  }

  T &operator[] (s64 index) {
      assert(index >= 0 && index < length);
      return data[index];
  }

  T *begin() {
      return &data[0];
  }

  T *end() {
      return &data[length];
  }
};

#endif