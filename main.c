#define _CRT_SECURE_NO_WARNINGS
#define MEM_SIZE 4096
#define INSTRUCTION_LEN 8
#define REG_COUNT 16

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct {
	FILE *config_file;
	FILE *meminst_file;
	FILE *reg_file;
	FILE *instruc_file;
	FILE *cdb_file;
} files_handler;

float regs[REG_COUNT];

struct {
	int add_nr_units;
	int add_nr_reserv;
	int add_delay;
	int mul_nr_units;
	int mul_nr_reserv;
	int mul_delay;
	int div_nr_units;
	int div_nr_reserv;
	int div_delay;
} func_units;

typedef struct inst_linked_list {
	int pc;
	int opcode;
	int dst;
	int src0;
	int src1;
	struct inst_linked_list* next;
} inst_ll;

inst_ll* new_inst_ll() {
	inst_ll* new_inst = NULL;
	new_inst = (inst_ll*)malloc(sizeof(inst_ll));
	if (new_inst == NULL)
		printf("New instruction allocation falied\n");
	else {
		new_inst->pc = -1;
		new_inst->opcode = -1;
		new_inst->dst = -1;
		new_inst->src0 = -1;
		new_inst->src1 = -1;
		new_inst->next = NULL;
	}
	return new_inst;
}

int get_args(int argc, char *argv[]) {

	if (argc != 6) {
		printf("Wrong amount of arguments\n");
		return -1;
	}
	files_handler.config_file = fopen(argv[1], "r");
	files_handler.meminst_file = fopen(argv[2], "r");
	files_handler.reg_file = fopen(argv[3], "w");
	files_handler.instruc_file = fopen(argv[4], "w");
	files_handler.cdb_file = fopen(argv[5], "w");
	if (!(files_handler.config_file && files_handler.meminst_file && files_handler.reg_file
		&& files_handler.instruc_file && files_handler.cdb_file)) {
		printf("Error opening files\n");
		return -1;
	}
	return 0;
}

void close_files() {
	fclose(files_handler.config_file);
	fclose(files_handler.meminst_file);
	fclose(files_handler.reg_file);
	fclose(files_handler.instruc_file);
	fclose(files_handler.cdb_file);
}

void set_unit(char* param_name, int param_val) {
	//printf("i have value %d for param %s\n", param_val, param_name);
	if (strcmp(param_name, "add_nr_units") == 0)
		func_units.add_nr_units = param_val;
	else if (strcmp(param_name, "mul_nr_units") == 0)
		func_units.mul_nr_units = param_val;
	else if (strcmp(param_name, "div_nr_units") == 0)
		func_units.div_nr_units = param_val;
	else if (strcmp(param_name, "add_nr_reserv") == 0)
		func_units.add_nr_reserv = param_val;
	else if (strcmp(param_name, "mul_nr_reserv") == 0)
		func_units.mul_nr_reserv = param_val;
	else if (strcmp(param_name, "div_nr_reserv") == 0)
		func_units.div_nr_reserv = param_val;
	else if (strcmp(param_name, "add_delay") == 0)
		func_units.add_delay = param_val;
	else if (strcmp(param_name, "mul_delay") == 0)
		func_units.mul_delay = param_val;
	else if (strcmp(param_name, "div_delay") == 0)
		func_units.div_delay = param_val;
	else
		printf("no match for parameter %s in cfg.txt\n", param_name);
}

char read_word(char** buffer, int size, FILE* read_origin) {
	char c;
	int read = 0;

	while ((c = fgetc(read_origin)) != EOF && c != '\n' && c != ' ') {
		printf("i read %c\n", c);
		if (read_origin == files_handler.config_file && read == size - 1) {
			size = 2 * size;
			if (NULL == (*buffer = (char*)realloc(*buffer, size * sizeof(char)))) {
				printf("Memory allocation failed\n");
				return -1;
			}
		}
		if (read_origin == files_handler.meminst_file && read > 7)
			continue;
		(*buffer)[read] = c;
		read++;
	}
	if (read_origin == files_handler.config_file) (*buffer)[read] = '\0';
	return c;
}

void set_func_units() {
	char* param_buffer = NULL, * val_buffer = NULL;
	int got_param = 0, got_val = 0, flag = 1;
	char c;

	while (flag) {
		if (NULL == (param_buffer = (char*)malloc(5 * sizeof(char)))) {
			printf("Memory allocation failed\n");
			return;
		}
		c = read_word(&param_buffer, 5, files_handler.config_file);
		switch (c) {
			case EOF:
				free(param_buffer);
				return;
			case '\n':
				printf("Parameter %s in cfg.txt has no value\n", param_buffer);
				free(param_buffer);
				continue;
			case ' ':
				got_param = 1;
				break;
		}
		if ('=' != fgetc(files_handler.config_file)
			|| ' ' != fgetc(files_handler.config_file)) {
			printf("Wrong format for %s in cfg.txt\n", param_buffer);
			free(param_buffer);
			return;
		}
		if (NULL == (val_buffer = (char*)malloc(5 * sizeof(char)))) {
			printf("Memory allocation failed\n");
			free(param_buffer);
			return;
		}
		c = read_word(&val_buffer, 5, files_handler.config_file);
		switch (c) {
			case EOF:
				flag = 0;
			case '\n':
				got_val = 1;
				break;
			case ' ':
				printf("Too many arguments in cfg.txt line\n");
				while ((c = fgetc(files_handler.config_file)) != EOF && c != '\n');
				break;
		}
		if (got_param && got_val) set_unit(param_buffer, atoi(val_buffer));
		free(param_buffer);
		free(val_buffer);
		got_param = 0;
		got_val = 0;
	}
}

int get_reg(char reg_char) {
	if (reg_char >= '0' && reg_char <= '9')
		return reg_char - '0';
	else if (reg_char >= 'a' && reg_char <= 'f')
		return 10 + reg_char - 'a';
	else if (reg_char >= 'A' && reg_char <= 'F')
		return 10 + reg_char - 'A';
	else {
		printf("Wrong register for instruction, setting as reg0\n");
		return 0;
	}
}

inst_ll* set_memin() {
	int pc_count;
	char c = 0, * temp_inst;
	inst_ll* head = NULL, * list_ptr = NULL, * prev_ptr;

	for (pc_count = 0; pc_count < 4096; pc_count++) {
		temp_inst = (char*)malloc(INSTRUCTION_LEN * sizeof(char));
		if (temp_inst == NULL) {
			printf("Allocation for instruction failed\n");
			return head;
		}
		memset(temp_inst, 0, INSTRUCTION_LEN);
		c = read_word(&temp_inst, 0, files_handler.meminst_file);
		if (temp_inst[1] == '0') return head; // empty line at end of file, got no instruction
		if (head == NULL) {
			head = new_inst_ll();
			list_ptr = head;
		}
		else {
			list_ptr->next = new_inst_ll();
			list_ptr = list_ptr->next;
		}
		list_ptr->pc = pc_count;
		list_ptr->opcode = temp_inst[1] - '0';
		list_ptr->dst = get_reg(temp_inst[2]);
		list_ptr->src0 = get_reg(temp_inst[3]);
		list_ptr->src1 = get_reg(temp_inst[4]);
		free(temp_inst);
		if (c == EOF) { // file ended with no '\n'
			break;
		}
	}
	return head;
}

void set_regs() {
	for (int i = 0; i < 16; i++) {
		regs[i] = (float)i;
	}
}

void tomasulo(inst_ll* memin_head) {
	inst_ll* prev_ptr;

	while (memin_head != NULL) {
		printf("%d\n", memin_head->opcode);
		prev_ptr = memin_head;
		memin_head = memin_head->next;
		free(prev_ptr);
	}
}

int main(int argc, char* argv[]) {
	inst_ll* memin_head = NULL;
	int i, j;
	if (0 != get_args(argc, argv))
		return -1;
	set_func_units();
	memin_head = set_memin();
	set_regs();
	if (memin_head != NULL) tomasulo(memin_head);
	close_files();
	return 0;
}