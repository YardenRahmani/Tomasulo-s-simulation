#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

struct {
	FILE* config_file;
	FILE* meminst_file;
	FILE* reg_file;
	FILE* instruc_file;
	FILE* cdb_file;
} files_handler;

reg regs[REG_COUNT];

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

void write_trace_cbd(inst_ll* inst, float data) {

	fprintf(files_handler.cdb_file, "%d %d ", inst->cycle_cdb, inst->pc);
	switch (inst->opcode) {
		case 2:
		case 3:
			fprintf(files_handler.cdb_file, "ADD ");
			break;
		case 4:
			fprintf(files_handler.cdb_file, "MUL ");
			break;
		case 5:
			fprintf(files_handler.cdb_file, "DIV ");
			break;
	}
	fprintf(files_handler.cdb_file, "%f ", data);
	switch (inst->opcode) {
		case 2:
		case 3:
			fprintf(files_handler.cdb_file, "ADD");
			break;
		case 4:
			fprintf(files_handler.cdb_file, "MUL");
			break;
		case 5:
			fprintf(files_handler.cdb_file, "DIV");
			break;
	}
	fprintf(files_handler.cdb_file, "%d\n", inst->tag);
}

void write_trace_inst(inst_ll* inst_head) {
	inst_ll* temp;

	while (inst_head != NULL) {
		fprintf(files_handler.instruc_file, "0%d", inst_head->opcode);
		fprintf(files_handler.instruc_file, "%c", give_reg(inst_head->dst));
		fprintf(files_handler.instruc_file, "%c", give_reg(inst_head->src0));
		fprintf(files_handler.instruc_file, "%c000 \n", give_reg(inst_head->src1));
		temp = inst_head;
		inst_head = inst_head->next;
		free(temp);
	}
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
		regs[i].vi = (float)i;
		regs[i].qi = NULL;
	}
}

float calulate_inst(reserv_station station_exec) {
	if (station_exec.cur_inst->opcode == 2) {
		return station_exec.vj + station_exec.vk;
	}
	else if (station_exec.cur_inst->opcode == 3) {
		return station_exec.vj - station_exec.vk;
	}
	else if (station_exec.cur_inst->opcode == 4) {
		return station_exec.vj * station_exec.vk;
	}
	else {
		return station_exec.vj / station_exec.vk;
	}
}

int issue_inst(inst_ll** list_head, int cycle, int* halt_flag_addr) {
	inst_ll* cur_inst = *list_head;
	func_unit* cur_unit;

	if (cur_inst->opcode == 2 || cur_inst->opcode == 3) {
		cur_unit = &add_units; // opcode to look at add stations
		}
	else if (cur_inst->opcode == 4) {
		cur_unit = &mul_units; // opcode to look at multiply stations
	}
	else if (cur_inst->opcode == 5) {
		cur_unit = &div_units; // opcode to look at division stations
	}
	else if (cur_inst->opcode == 6) {
		*halt_flag_addr = 1;
		*list_head = (*list_head)->next;
		return 1;
	}
	else {
		printf("Invalid opcode was given\n");
		*list_head = (*list_head)->next;
		return -1;
	}
	for (int i = 0; i < cur_unit->nr_reserv; i++) {
		if (cur_unit->reserv_stations[i].cur_inst == NULL) { // look for free station
			// take next instruction from fetched queue and place it in the station
			cur_unit->reserv_stations[i].cur_inst = cur_inst;
			cur_inst->tag = i; // update tag
			cur_inst->cycle_issued = cycle;
			// get reg0
			if (regs[cur_unit->reserv_stations[i].cur_inst->src0].qi == NULL) { // reg value can be used
				cur_unit->reserv_stations[i].vj = regs[cur_unit->reserv_stations[i].cur_inst->src0].vi;
			}
			else { // reg value is waiting on a station to finish writing
				cur_unit->reserv_stations[i].qj = regs[cur_unit->reserv_stations[i].cur_inst->src0].qi;
			}
			// get reg1
			if (regs[cur_unit->reserv_stations[i].cur_inst->src1].qi == NULL) { // reg value can be used
				cur_unit->reserv_stations[i].vk = regs[cur_unit->reserv_stations[i].cur_inst->src1].vi;
			}
			else { // reg value is waiting on a station to finish writing
				cur_unit->reserv_stations[i].qk = regs[cur_unit->reserv_stations[i].cur_inst->src1].qi;
			}
			// set reg_dst data as not ready
			regs[cur_unit->reserv_stations[i].cur_inst->dst].qi = &(cur_unit->reserv_stations[i]);
			*list_head = (*list_head)->next;
			return 1;
		}
	}
	return 0; // no free station
}

void check_exec_unit(func_unit* cur_unit, int cycle) {
	reserv_station station_iter;

	for (int i = 0; i < cur_unit->nr_reserv; i++) {
		// check if an instruction can be executed
		station_iter = cur_unit->reserv_stations[i];
		if (station_iter.cur_inst != NULL && station_iter.cur_inst->cycle_exec == -1) {
			if (station_iter.qj == NULL && station_iter.qk == NULL) {
				station_iter.cur_inst->cycle_exec = cycle;
			}
		}
	}
}

void check_exec(int cycle) {
	check_exec_unit(&add_units, cycle);
	check_exec_unit(&mul_units, cycle);
	check_exec_unit(&div_units, cycle);
}

void update_station_cbd(func_unit* cur_unit, reserv_station* finished_station_addr, float data) {
	reserv_station station_iter;

	for (int i = 0; i < cur_unit->nr_reserv; i++) {
		// look for a reserve station waiting of the result
		station_iter = cur_unit->reserv_stations[i];
		if (station_iter.cur_inst != NULL && station_iter.qj == finished_station_addr) {
			station_iter.vj = data;
			station_iter.qj = NULL;
		}
		if (station_iter.cur_inst != NULL && station_iter.qk == finished_station_addr) {
			station_iter.vk = data;
			station_iter.qk = NULL;
		}
	}
}

void broadcast_cbd(reserv_station* finished_station_addr, int cycle) {
	float data = calulate_inst(*finished_station_addr);
	regs[finished_station_addr->cur_inst->dst].vi = data;
	regs[finished_station_addr->cur_inst->dst].qi = NULL;
	finished_station_addr->cur_inst->cycle_cdb = cycle;
	update_station_cbd(&add_units, finished_station_addr, data);
	update_station_cbd(&mul_units, finished_station_addr, data);
	update_station_cbd(&div_units, finished_station_addr, data);
	write_trace_cbd(finished_station_addr->cur_inst, regs[finished_station_addr->cur_inst->dst].vi);
}

void write_cbd_unit(func_unit* cur_unit, int cycle) {
	reserv_station station_iter;

	for (int i = 0; i < cur_unit->nr_reserv; i++) {
		// check if an instruction that started previously is finished
		station_iter = cur_unit->reserv_stations[i];
		if (station_iter.cur_inst != NULL && station_iter.cur_inst->cycle_exec != -1) {
			if (cycle - station_iter.cur_inst->cycle_exec >= cur_unit->delay) {
				broadcast_cbd(&station_iter, cycle);
				station_iter.cur_inst = NULL;
			}
		}
	}
}

void write_cbd(int cycle) {
	write_cbd_unit(&add_units, cycle);
	write_cbd_unit(&mul_units, cycle);
	write_cbd_unit(&div_units, cycle);
}

int look_for_busy_station(func_unit cur_unit) {
	reserv_station station_iter;

	for (int i = 0; i < cur_unit.nr_reserv; i++) {
		// check if an instruction that started previously is finished
		station_iter = cur_unit.reserv_stations[i];
		if (station_iter.cur_inst != NULL) {
			return 1;
		}
	}
	return 0;
}

int look_for_busy() {
	if (!look_for_busy_station(add_units)) {
		if (!look_for_busy_station(mul_units)) {
			if (!look_for_busy_station(div_units)) {
				return 0;
			}
		}
	}
	return 1;
}

inst_ll* tomasulo(inst_ll* memin_head) {
	inst_ll* fetched = NULL, * memin_new_head = NULL;
	inst_ll* iter, * inst_holder;
	int cycle = 0, flag = 1, halt_flag = 0;

	while (!halt_flag) { // reading intructions		
		// EXECUTE
		check_exec(cycle);

		// WRITE CBD
		write_cbd(cycle);

		// ISSUE
		if (fetched != NULL) {
			int issued = issue_inst(&fetched, cycle, &halt_flag);
			if (fetched != NULL && issued)
				issue_inst(&fetched, cycle, &halt_flag);
		}

		// FETCH
		if (memin_head != NULL) {
			inst_holder = fetch_inst(&memin_head); // fetch first
			if (fetched == NULL) { // fetched is empty
				fetched = inst_holder;
				if (flag) { // save new instruction head
					memin_new_head = fetched;
					flag = 0;
				}
				iter = inst_holder;
			}
			else {
				iter = fetched; // begining of fetched instructions waiting to issue
				while (iter->next != NULL) iter = iter->next; // go to end of line
				iter->next = inst_holder; // add first instruction
			}
			inst_holder = fetch_inst(&memin_head); // fetch second
			if (inst_holder != NULL) {
				while (iter->next != NULL) iter = iter->next; // go to end of line
				iter->next = inst_holder; // add second instruction
			}
		}
		cycle++;
	}
	while (1) {
		// EXECUTE
		check_exec(cycle);

		// WRITE CBD
		write_cbd(cycle);
		if (!look_for_busy()) break;
		cycle++;
	}
	return memin_new_head;
}

int main(int argc, char* argv[]) {
	inst_ll* memin_head = NULL;

	if (0 != open_files(argc, argv))
		return -1;
	set_func_units();
	memin_head = set_memin();
	set_regs();
	tomasulo(memin_head);
	write_trace_inst(memin_head);
	close_files();
	free_reserv_stations();
	return 0;
}