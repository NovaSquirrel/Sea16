/*
Sea16 interpreter

Copyright 2018 NovaSquirrel

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

struct cpu {
	uint16_t a;	// "left" register
	uint16_t b;	// "right" register
	uint16_t sp; // stack pointer
	uint16_t fp; // frame pointer
	uint16_t pc; // program counter
	uint8_t *ram;
};

/////////////////////////////////////////////////
// memory access
/////////////////////////////////////////////////
uint8_t instruction_byte(struct cpu *s) {
	return s->ram[s->pc++];
}

uint16_t instruction_word(struct cpu *s) {
	uint8_t temp = instruction_byte(s);
	return temp | (instruction_byte(s)<<8);
}

uint8_t read_byte(struct cpu *s, uint16_t address) {
	return s->ram[address];
}

uint16_t read_word(struct cpu *s, uint16_t address) {
	return read_byte(s, address) | (read_byte(s, address+1) << 8);
}

void write_byte(struct cpu *s, uint16_t address, uint8_t value) {
	s->ram[address] = value;
}

void write_word(struct cpu *s, uint16_t address, uint16_t value) {
	s->ram[address] = value & 255;
	s->ram[address+1] = value >> 8;
}

/////////////////////////////////////////////////
// addressing modes
/////////////////////////////////////////////////
uint16_t fast(struct cpu *s, uint8_t byte) {
	static const int16_t offsets[] = {
		-24, -22, -20, -18, -16, -14, -12, -10, -8, -6, -4, -2,
		4, 6, 8, 10
	};
	return s->fp + offsets[byte&15];
}

uint16_t near(struct cpu *s) {
	return s->fp + (int8_t)instruction_byte(s);
}

uint16_t far(struct cpu *s) {
	return s->fp + (int16_t)instruction_word(s);
}

uint16_t absolute(struct cpu *s) {
	return instruction_word(s);
}

/////////////////////////////////////////////////
// common operations
/////////////////////////////////////////////////
void push_word(struct cpu *s, uint16_t value) {
	write_byte(s, s->sp--, value>>8);
	write_byte(s, s->sp--, value&255);
}

uint16_t pop_word(struct cpu *s) {
	uint8_t temp = read_byte(s, ++s->sp);
	return temp | (read_byte(s, ++s->sp)<<8);
}

void sub_call(struct cpu *s, uint16_t address) {
	push_word(s, s->pc);
	push_word(s, s->fp);
	s->fp = s->sp + 1; // point at the old frame pointer
	s->pc = address;
}

void sub_return(struct cpu *s) {
	s->sp = s->fp - 1;
	s->fp = pop_word(s);
	s->pc = pop_word(s);
}

/////////////////////////////////////////////////
// instruction execution
/////////////////////////////////////////////////
void do_instruction(struct cpu *s) {
	uint8_t opcode = instruction_byte(s);
	uint16_t temp, temp2, temp3;

	if(opcode >= 0x00 && opcode <= 0x0f) {        // lda fast
		s->a = read_word(s, fast(s, opcode));
	} else if(opcode >= 0x10 && opcode <= 0x1f) { // ldb fast
		s->b = read_word(s, fast(s, opcode));
	} else if(opcode >= 0x20 && opcode <= 0x2f) { // push fast
		push_word(s, read_word(s, fast(s, opcode)));
	} else if(opcode >= 0x30 && opcode <= 0x3f) { // sta fast
		write_word(s, fast(s, opcode), s->a);
	} else if(opcode >= 0x40 && opcode <= 0x4f) { // lda #x
		s->a = opcode&15;
	} else if(opcode >= 0x50 && opcode <= 0x5f) { // ldb #x
		s->b = opcode&15;
	} else if(opcode >= 0x60 && opcode <= 0x6f) { // push #x
		push_word(s, opcode&15);
	} else if(opcode >= 0x70 && opcode <= 0x7f) { // add #x
		s->a += opcode&15;
	} else switch(opcode) {
		case 0x80: // lda near
			s->a = read_word(s, near(s));
			break;
		case 0x81: // lda far
			s->a = read_word(s, far(s));
			break;
		case 0x82: // ldb near
			s->b = read_word(s, near(s));
			break;
		case 0x83: // ldb far
			s->b = read_word(s, far(s));
			break;
		case 0x84: // push near
			push_word(s, read_word(s, near(s)));
			break;
		case 0x85: // push far
			push_word(s, read_word(s, far(s)));
			break;
		case 0x86: // sta near
			write_word(s, near(s), s->a);
			break;
		case 0x87: // sta far
			write_word(s, far(s), s->a);
			break;

		case 0x88: // lda #xx
			s->a = (int16_t)(int8_t)instruction_byte(s);
			break;
		case 0x89: // lda #xxxx
			s->a = instruction_word(s);
			break;
		case 0x8a: // ldb #xx
			s->b = (int16_t)(int8_t)instruction_byte(s);
			break;
		case 0x8b: // ldb #xxxx
			s->b = instruction_word(s);
			break;
		case 0x8c: // push #xx
			push_word(s, (int16_t)(int8_t)instruction_byte(s));
			break;
		case 0x8d: // push #xxxx
			push_word(s, instruction_word(s));
			break;
		case 0x8e: // add #xx
			s->a += (int16_t)(int8_t)instruction_byte(s);
			break;
		case 0x8f: // add #xxxx
			s->a += instruction_word(s);
			break;

		case 0x90: // lda abs
			s->a = read_word(s, absolute(s));
			break;
		case 0x91: // ldb abs
			s->b = read_word(s, absolute(s));
			break;
		case 0x92: // push abs
			push_word(s, read_word(s, absolute(s)));
			break;
		case 0x93: // sta abs
			write_word(s, absolute(s), s->a);
			break;

		case 0x94: // blda abs
			s->a = read_byte(s, absolute(s));
			break;
		case 0x95: // bldb abs
			s->b = read_byte(s, absolute(s));
			break;
		case 0x96: // bpush abs
			push_word(s, read_byte(s, absolute(s)));
			break;
		case 0x97: // bsta abs
			write_byte(s, absolute(s), s->a&255);
			break;

		case 0x98: // blda far
			s->a = read_word(s, far(s));
			break;
		case 0x99: // bldb far
			s->b = read_word(s, far(s));
			break;
		case 0x9a: // bpush far
			push_word(s, read_byte(s, far(s)));
			break;
		case 0x9b: // bsta far
			write_byte(s, far(s), s->a&255);
			break;

		case 0x9c: // leaa far
			s->a = far(s);
			break;
		case 0x9d: // leab far
			s->b = far(s);
			break;
		case 0x9e: // pea far
			push_word(s, far(s));
			break;
		case 0x9f: // ?
			break;

		case 0xa0: // deref
			s->a = read_word(s, s->a);
			break;
		case 0xa1: // popstore
			s->b = pop_word(s);
			write_word(s, s->b, s->a);
			break;
		case 0xa2: // bderef
			s->a = read_byte(s, s->a);
			break;
		case 0xa3: // bpopstore
			s->b = pop_word(s);
			write_byte(s, s->b, s->a&255);
			break;
		case 0xa4: // pha
			push_word(s, s->a);
			break;
		case 0xa5: // plb
			s->b = pop_word(s);
			break;
		case 0xa6: // unstack #xx
			s->sp += instruction_byte(s);
			break;
		case 0xa7: // unstack #xxxx
			s->sp += instruction_word(s);
			break;
		case 0xa8: // call xxxx
			sub_call(s, instruction_word(s));
			break;
		case 0xa9: // callptr
			sub_call(s, s->a);
			break;
		case 0xaa: // call xxxx, #yy
			sub_call(s, instruction_word(s));
			s->sp -= instruction_byte(s);
			break;
		case 0xab: // callptr #yy
			sub_call(s, s->a);
			s->sp -= instruction_byte(s);
			break;
		case 0xac: // ?
		case 0xad: // ?
			break;
		case 0xae: // return
			sub_return(s);
			break;
		case 0xaf: // swap
			temp = s->a;
			s->a = s->b;
			s->a = temp;
			break;

		case 0xb0: // ucmplt
			s->a = s->a < s->b;
			break;
		case 0xb1: // ucmple
			s->a = s->a <= s->b;
			break;
		case 0xb2: // ucmpgt
			s->a = s->a > s->b;
			break;
		case 0xb3: // ucmpge
			s->a = s->a >= s->b;
			break;
		case 0xb4: // scmplt
			s->a = (int16_t)s->a < (int16_t)s->b;
			break;
		case 0xb5: // scmple
			s->a = (int16_t)s->a <= (int16_t)s->b;
			break;
		case 0xb6: // scmpgt
			s->a = (int16_t)s->a > (int16_t)s->b;
			break;
		case 0xb7: // scmpge
			s->a = (int16_t)s->a >= (int16_t)s->b;
			break;
		case 0xb8: // cmpeq
			s->a = s->a == s->b;
			break;
		case 0xb9: // cmpne
			s->a = s->a != s->b;
			break;
		case 0xba: // not
			s->a = !s->a;
			break;
		case 0xbb: // neg
			s->a = -s->a;
			break;
		case 0xbc: // compl
			s->a = ~s->a;
			break;
		case 0xbd: // and
			s->a &= s->b;
			break;
		case 0xbe: // or
			s->a |= s->b;
			break;
		case 0xbf: // xor
			s->a ^= s->b;
			break;

		case 0xc0: // add
			s->a += s->b;
			break;
		case 0xc1: // dec
			s->a--;
			break;
		case 0xc2: // rsub
			s->a = s->b - s->a;
			break;
		case 0xc3: // sub
			s->a -= s->b;
			break;
		case 0xc4: // lshift
			s->a <<= s->b;
			break;
		case 0xc5: // double
			s->a <<= 1;
			break;
		case 0xc6: // rshift
			s->a >>= s->b;
			break;
		case 0xc7: // arshift
			s->a = ((int16_t)s->a) >> s->b;
			break;
		case 0xc8: // divu
			s->a /= s->b;
			break;
		case 0xc9: // divs
			s->a = (int16_t)s->a / (int16_t)s->b;
			break;
		case 0xca: // modu
			s->a %= s->b;
			break;
		case 0xcb: // mods
			s->a = (int16_t)s->a % (int16_t)s->b;
			break;
		case 0xcc: // mult
			s->a *= s->b;
			break;
		case 0xcd: // sloadbf x,y
			temp = instruction_byte(s);
			temp2 = temp>>4;
			temp3 = temp&15;
			s->a = (s->b >> temp2) & (0xffff >> 15-temp3);
			if(s->a & (1 << temp3))
				s->a |= 0xffff << (temp3+1);
			break;
		case 0xce: // uloadbf x,y
			temp = instruction_byte(s);
			temp2 = temp>>4;
			temp3 = temp&15;
			s->a = (s->b >> temp2) & (0xffff >> 15-temp3);
			break;
		case 0xcf: // storebf x,y
			temp = instruction_byte(s);
			temp2 = temp>>4;
			temp3 = temp&15;
			s->a = s->b & ~((0xffff >> 15-temp3) << temp2) | (s->a & (0xffff >> 15-temp3)) << temp2;
			break;

		case 0xd0: // jumpt +xx
			temp = instruction_byte(s);
			if(s->a)
				s->pc += temp + 1;
			break;
		case 0xd1: // jumpt -xx
			temp = instruction_byte(s);
			if(s->a)
				s->pc -= temp - 1;
			break;
		case 0xd2: // jumpt xxxx
			temp = instruction_word(s);
			if(s->a)
				s->pc = temp;
			break;
		case 0xd4: // jumpf +xx
			temp = instruction_byte(s);
			if(!s->a)
				s->pc += temp + 1;
			break;
		case 0xd5: // jumpf -xx
			temp = instruction_byte(s);
			if(!s->a)
				s->pc -= temp - 1;
			break;
		case 0xd6: // jumpf xxxx
			temp = instruction_word(s);
			if(!s->a)
				s->pc = temp;
			break;
		case 0xd8: // jump +xx
			s->pc += instruction_byte(s) + 1;
			break;
		case 0xd9: // jump -xx
			s->pc -= instruction_byte(s) + 1;
			break;
		case 0xda: // jump xxxx
			s->pc = instruction_word(s);
			break;
		case 0xdc: // switchrange
			temp  = instruction_word(s); // low bound, inclusive
			temp2 = instruction_word(s); // high bound, inclusive
			temp3 = instruction_word(s); // default case
			if(s->a < temp || s->a > temp2)
				s->pc = temp3; // go to default
			s->pc = read_word(s, s->pc+(s->a-temp)*2); // jump table
			break;
		case 0xdd: // switchlist
			temp = instruction_word(s); // number of cases
			while(temp) {
				temp2 = instruction_word(s); // check against
				temp3 = instruction_word(s); // address to go to
				if(s->a == temp2) {
					s->pc = temp3;
					break;
				}
			}
			// default case
			s->pc = instruction_word(s);
			break;
		case 0xde: // in xx
			temp = instruction_byte(s);			
			if(temp == 0) {
				s->a = getchar();
			}
			break;
		case 0xdf: // out xx
			temp = instruction_byte(s);
			if(temp == 0) {
				putchar(s->a);
			}
			break;

		case 0xe0: // extenda
			s->a = (int16_t)(int8_t)s->a;
			break;
		case 0xe1: // extendb
			s->b = (int16_t)(int8_t)s->b;
			break;
		case 0xe2: // copy #xxxx
			temp = instruction_word(s);
			while(temp) {
				write_byte(s, s->b++, read_byte(s, s->a++));
				temp--;
			}
			break;
		case 0xe3: // fill #xxxx
			temp = instruction_word(s);
			while(temp) {
				write_byte(s, s->b++, s->a);
				temp--;
			}
			break;
		case 0xe4: // mcmp #xxxx
			temp = instruction_word(s);
			temp2 = 0;
			while(temp) {
				if(read_byte(s, s->a) > read_byte(s, s->b)) {
					temp2 = 1;
					break;
				}
				if(read_byte(s, s->a) < read_byte(s, s->b)) {
					temp2 = -1;
					break;
				}
				s->a++;
				s->b++;
				temp--;
			}
			s->a = temp2;
			break;
		case 0xec: // lda sp
			s->a = s->sp;
			break;
		case 0xed: // sta sp
			s->sp = s->a;
			break;
		case 0xee: // zalloc #xx
			temp = instruction_byte(s);
			s->sp -= temp;
			memset(&s->ram[s->sp], 0, temp);
			break;
		case 0xef: // direct count
			{
				static const int direct_count_offsets[] = {1, 2, -1, -2};

				temp = instruction_byte(s);
				uint16_t address = fast(s, temp);
				uint16_t value = read_word(s, temp2);
				int mode = temp >>= 6; // get just the mode
				int amount = (temp>>4)&3;

				if(mode == 2) { // lda incrementing
					if(amount & 1) {
						s->a = read_word(s, value);
					} else {
						s->a = read_byte(s, value);
					}
				}
				if(mode == 3) { // sta incrementing
					if(amount & 1) {
						write_word(s, value, s->a);
					} else {
						write_byte(s, value, s->a);
					}
				}

				value += direct_count_offsets[amount];

				write_word(s, address, value);
				if(mode == 1) // get new value
					s->a = value;
			}
			break;

		default:
			puts("Illegal opcode");
			break;
	}

}

/////////////////////////////////////////////////
// main section
/////////////////////////////////////////////////
uint8_t all_ram[0x10000];

int main(int argc, char *argv[]) {
	struct cpu c;
	memset(&c, 0, sizeof(c));
	c.ram = all_ram;
	c.sp = 0xffff;
	c.fp = 0xffff;

	// infinite loop
	all_ram[0]  = 0xd9;
	all_ram[1]  = 0x01;
	
	if(argc > 1) {
		FILE *file = fopen(argv[1], "rb");
		if(!file)
			return -1;
		fread(all_ram, 1, 0x10000, file);
		fclose(file);
	}

	int trace = 0;
	for(int i = 2; i<argc; i++) {
		if(!strcmp(argv[i], "--trace"))
			trace = 1;
	}

	for(int i=0; i<20; i++) {
		if(trace)
			printf("A:%.4x B:%.4x PC:%.4x SP:%.4x FP:%.4x \n", c.a, c.b, c.pc, c.sp, c.fp);
		do_instruction(&c);
	}


}
