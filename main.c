#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

struct {
	FILE *config_file;
	FILE *meminst_file;
	FILE *reg_file;
	FILE *instruc_file;
	FILE *cdb_file;
} files_handler;

float regs[REG_COUNT];
func_unit add_units;
func_unit mul_units;
func_unit div_units;

void free_reserv_stations() {

	free(add_units.reserv_stations);
	free(mul_units.reserv_stations);
	free(div_units.reserv_stations);
}

int open_files(int argc, char* argv[]) {

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

reserv_station* set_reserv(int reserv_num) {
	reserv_station* cur_reserv = (reserv_station*)malloc(reserv_num * sizeof(reserv_station));

	if (cur_reserv != NULL) {
		for (int i = 0; i < reserv_num; i++) {
			cur_reserv[i].cur_inst = NULL;
			cur_reserv[i].vj = -1;
			cur_reserv[i].vk = -1;
			cur_reserv[i].qj = NULL;
			cur_reserv[i].qk = NULL;
		}
	}
	return cur_reserv;
}

void set_unit(char* param_name, int param_val) {
	
	if (strcmp(param_name, "add_nr_units") == 0)
		add_units.nr_units = param_val;
	else if (strcmp(param_name, "mul_nr_units") == 0)
		mul_units.nr_units = param_val;
	else if (strcmp(param_name, "div_nr_units") == 0)
		div_units.nr_units = param_val;
	else if (strcmp(param_name, "add_nr_reservation") == 0) {
		add_units.nr_reserv = param_val;
		add_units.reserv_stations = set_reserv(param_val);
	}
	else if (strcmp(param_name, "mul_nr_reservation") == 0) {
		mul_units.nr_reserv = param_val;
		mul_units.reserv_stations = set_reserv(param_val);
	}
	else if (strcmp(param_name, "div_nr_reservation") == 0) {
		div_units.nr_reserv = param_val;
		div_units.reserv_stations = set_reserv(param_val);
	}
	else if (strcmp(param_name, "add_delay") == 0)
		add_units.delay = param_val;
	else if (strcmp(param_name, "mul_delay") == 0)
		mul_units.delay = param_val;
	else if (strcmp(param_name, "div_delay") == 0)
		div_units.delay = param_val;
	else
		printf("no match for parameter %s in cfg.txt\n", param_name);
}

char read_word(char** buffer, int size, FILE* read_origin) {
	char c;
	int read = 0;

	while ((c = fgetc(read_origin)) != EOF && c != '\n' && c != ' ') {
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
	inst_ll* head = NULL, * list_ptr = NULL;

	for (pc_count = 0; pc_count < 4096; pc_count++) {
		temp_inst = (char*)malloc(INSTRUCTION_LEN * sizeof(char));
		if (temp_inst == NULL) {
			printf("Allocation for instruction failed\n");
			return head;
		}
		memset(temp_inst, 0, INSTRUCTION_LEN);
		c = read_word(&temp_inst, 0, files_handler.meminst_file);
		if (temp_inst[0] == '\0') {
			free(temp_inst);
			return head; // empty line at end of file, got no instruction
		}
		if (head == NULL) {
			head = new_inst();
			list_ptr = head;
		}
		else {
			list_ptr->next = new_inst();
			list_ptr = list_ptr->next;
		}
		if (list_ptr != NULL) {
			list_ptr->pc = pc_count;
			list_ptr->opcode = temp_inst[1] - '0';
			list_ptr->dst = get_reg(temp_inst[2]);
			list_ptr->src0 = get_reg(temp_inst[3]);
			list_ptr->src1 = get_reg(temp_inst[4]);
		}
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

int look_for_station(inst_ll** list_head) {
	inst_ll* next_inst = *list_head;
	if (next_inst->opcode == 2 || next_inst->opcode == 3) {
		for (int i = 0; i < add_units.nr_reserv; i++) {
			if (add_units.reserv_stations[i].cur_inst == NULL) {
				*list_head = (*list_head)->next;
				add_units.reserv_stations[i].cur_inst = next_inst;
				printf("%d: 0%d%d%d%d000\n", next_inst->pc, next_inst->opcode, next_inst->dst, next_inst->src0, next_inst->src1);
				next_inst->next = NULL;
				free(next_inst);
				return 1;
			}
		}
	}
	else if (next_inst->opcode == 4) {
		for (int i = 0; i < mul_units.nr_reserv; i++) {
			if (mul_units.reserv_stations[i].cur_inst == NULL) {
				*list_head = (*list_head)->next;
				mul_units.reserv_stations[i].cur_inst = next_inst;
				// check registers
				printf("%d: 0%d%d%d%d000\n", next_inst->pc, next_inst->opcode, next_inst->dst, next_inst->src0, next_inst->src1);
				next_inst->next = NULL;
				free(next_inst);
				return 1;
			}
		}
	}
	else if (next_inst->opcode == 5) {
		for (int i = 0; i < div_units.nr_reserv; i++) {
			if (div_units.reserv_stations[i].cur_inst == NULL) {
				*list_head = (*list_head)->next;
				div_units.reserv_stations[i].cur_inst = next_inst;
				// check registers
				printf("%d: 0%d%d%d%d000\n", next_inst->pc, next_inst->opcode, next_inst->dst, next_inst->src0, next_inst->src1);
				next_inst->next = NULL;
				free(next_inst);
				return 1;
			}
		}
	}
	else if (next_inst->opcode == 6) {
		printf("I got halt, and?\n");
		free(next_inst);
		return 1;
	}
	else {
		printf("Invalid opcode was given\n");
		*list_head = (*list_head)->next;
		free(next_inst);
		return -1;
	}
	return 0; // no free station
}

void tomasulo(inst_ll* memin_head) {
	inst_ll* fetched = NULL, * iter, * inst_holder;
	int cycle = 0;

	while (memin_head != NULL || fetched != NULL) { // reading intructions
		// EXECUTE


		// ISSUE
		if (fetched != NULL) {
			int issued = look_for_station(&fetched);
			if (fetched != NULL && issued)
				look_for_station(&fetched);
		}

		// FETCH
		if (memin_head != NULL) {
			inst_holder = get_next_inst(&memin_head); // fetch first
			if (fetched == NULL) { // fetched is empty
				fetched = inst_holder;
				iter = inst_holder;
			}
			else {
				iter = fetched; // begining of fetched instructions waiting to issue
				while (iter->next != NULL) iter = iter->next; // go to end of line
				iter->next = inst_holder; // add first instruction
			}
			inst_holder = get_next_inst(&memin_head); // fetch second
			if (inst_holder != NULL) {
				while (iter->next != NULL) iter = iter->next; // go to end of line
				iter->next = inst_holder; // add second instruction
			}
		}
		cycle++;
	}
}

int main(int argc, char* argv[]) {
	inst_ll* memin_head = NULL;

	if (0 != open_files(argc, argv))
		return -1;
	set_func_units();
	memin_head = set_memin();
	set_regs();
	if (memin_head != NULL) tomasulo(memin_head);
	close_files();
	free_reserv_stations();
	return 0;
}