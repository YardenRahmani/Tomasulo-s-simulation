#define _CRT_SECURE_NO_WARNINGS
#define MEM_SIZE 4096
#define INSTRUCTION_LEN 8
#define REG_COUNT 16

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct inst_linked_list {
	int pc;
	int opcode;
	int dst;
	int src0;
	int src1;
	struct inst_linked_list* next;
	int tag;
	int cycle_issued;
	int cycle_exec_start;
	int cycle_exec_end;
	int cycle_cdb;
} inst_ll;

typedef struct reserv_station {
	inst_ll* cur_inst;
	float vj;
	float vk;
	struct reserv_station* qj;
	struct reserv_station* qk;
} reserv_station;

typedef struct function_unit {
	int nr_units;
	int nr_units_busy;
	int nr_reserv;
	reserv_station* reserv_stations;
	int delay;
	int cdb_free;
} func_unit;

typedef struct reg {
	float vi;
	reserv_station* qi;
} reg;

struct {
	FILE* config_file;
	FILE* meminst_file;
	FILE* reg_file;
	FILE* instrace_file;
	FILE* cdb_file;
} files_handler;

reg regs[REG_COUNT];

inst_ll* new_inst() {
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
		new_inst->tag = -1;
		new_inst->cycle_issued = -1;
		new_inst->cycle_exec_start = -1;
		new_inst->cycle_exec_end = -1;
		new_inst->cycle_cdb = -1;
	}
	return new_inst;
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

int give_reg(int reg_num) {
	if (reg_num < 10) {
		return '0' + reg_num;
	}
	else {
		return 'A' + reg_num - 10;
	}
}

int intlen(int num) {
	int count = 0;

	while (num > 0) {
		num = num / 10;
		count++;
	}
	return count;
}

void set_finished_unit(func_unit* cur_unit) {
	reserv_station* iter = NULL;

	for (int i = 0; i < cur_unit->nr_reserv; i++) {
		iter = (cur_unit->reserv_stations) + i;
		if (iter->cur_inst != NULL && iter->cur_inst->cycle_cdb != -1) {
			// if the station sent data on cdb, station is free for next instruction
			iter->cur_inst = NULL;
			cur_unit->nr_units_busy--;
		}
	}
	if (!cur_unit->cdb_free) { // transmiting on cdb taked at most 1 cycle
		cur_unit->cdb_free = 1;
	}
}

void set_finished_comp(func_unit* units_addr_arr[3]) {
	// set components the are finished (cdb, computation units) as free
	set_finished_unit(units_addr_arr[0]);
	set_finished_unit(units_addr_arr[1]);
	set_finished_unit(units_addr_arr[2]);
}

void free_reserv_stations(func_unit* units_addr_arr[3]) {

	free(units_addr_arr[0]->reserv_stations);
	free(units_addr_arr[1]->reserv_stations);
	free(units_addr_arr[2]->reserv_stations);
}

int open_files(int argc, char* argv[]) {

	if (argc != 6) {
		printf("Wrong amount of arguments\n");
		return -1;
	}
	files_handler.config_file = fopen(argv[1], "r");
	files_handler.meminst_file = fopen(argv[2], "r");
	files_handler.reg_file = fopen(argv[3], "w");
	files_handler.instrace_file = fopen(argv[4], "w");
	files_handler.cdb_file = fopen(argv[5], "w");
	if (!(files_handler.config_file && files_handler.meminst_file && files_handler.reg_file
		&& files_handler.instrace_file && files_handler.cdb_file)) {
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
		if (inst_head->opcode == 6){
			while (inst_head != NULL) {
				temp = inst_head;
				inst_head = inst_head->next;
				free(temp);
			}
			return;
		}
		else {
			fprintf(files_handler.instrace_file, "0%d", inst_head->opcode);
			fprintf(files_handler.instrace_file, "%c", give_reg(inst_head->dst));
			fprintf(files_handler.instrace_file, "%c", give_reg(inst_head->src0));
			fprintf(files_handler.instrace_file, "%c000 ", give_reg(inst_head->src1));
			fprintf(files_handler.instrace_file, "%d ", inst_head->pc);
			switch (inst_head->opcode) {
			case 2:
			case 3:
				fprintf(files_handler.instrace_file, "ADD");
				break;
			case 4:
				fprintf(files_handler.instrace_file, "MUL");
				break;
			case 5:
				fprintf(files_handler.instrace_file, "DIV");
				break;
			}
			fprintf(files_handler.instrace_file, "%d ", inst_head->tag);
			fprintf(files_handler.instrace_file, "%d %d ", inst_head->cycle_issued, inst_head->cycle_exec_start);
			fprintf(files_handler.instrace_file, "%d %d\n", inst_head->cycle_exec_end, inst_head->cycle_cdb);
			temp = inst_head;
			inst_head = inst_head->next;
			free(temp);
		}
	}
}

void write_regout() {
	for (int i = 0; i < REG_COUNT; i++) {
		fprintf(files_handler.reg_file, "%f\n", regs[i].vi);
	}
}

void close_files() {
	fclose(files_handler.reg_file);
	fclose(files_handler.instrace_file);
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

void set_unit(char* param_name, int param_val, func_unit* units_addr_arr[3]) {
	
	if (strcmp(param_name, "add_nr_units") == 0) {
		units_addr_arr[0]->nr_units = param_val;
		units_addr_arr[0]->nr_units_busy = 0;
		units_addr_arr[0]->cdb_free = 1;
	}
	else if (strcmp(param_name, "mul_nr_units") == 0) {
		units_addr_arr[1]->nr_units = param_val;
		units_addr_arr[1]->nr_units_busy = 0;
		units_addr_arr[1]->cdb_free = 1;
	}
	else if (strcmp(param_name, "div_nr_units") == 0) {
		units_addr_arr[2]->nr_units = param_val;
		units_addr_arr[2]->nr_units_busy = 0;
		units_addr_arr[2]->cdb_free = 1;
	}
	else if (strcmp(param_name, "add_nr_reservation") == 0) {
		units_addr_arr[0]->nr_reserv = param_val;
		units_addr_arr[0]->reserv_stations = set_reserv(param_val);
	}
	else if (strcmp(param_name, "mul_nr_reservation") == 0) {
		units_addr_arr[1]->nr_reserv = param_val;
		units_addr_arr[1]->reserv_stations = set_reserv(param_val);
	}
	else if (strcmp(param_name, "div_nr_reservation") == 0) {
		units_addr_arr[2]->nr_reserv = param_val;
		units_addr_arr[2]->reserv_stations = set_reserv(param_val);
	}
	else if (strcmp(param_name, "add_delay") == 0)
		units_addr_arr[0]->delay = param_val;
	else if (strcmp(param_name, "mul_delay") == 0)
		units_addr_arr[1]->delay = param_val;
	else if (strcmp(param_name, "div_delay") == 0)
		units_addr_arr[2]->delay = param_val;
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

void set_func_units(func_unit* units_addr_arr[3]) {
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
		if (got_param && got_val) set_unit(param_buffer, atoi(val_buffer), units_addr_arr);
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

void fetch_inst(inst_ll* memin_head, inst_ll** fetched_addr, int* start_flag) {

	if (*start_flag) { // fetch the first instruction
		*fetched_addr = memin_head;
		*start_flag = 0;
	}
	else { // fetch the next instrucion from instruction memory
		*fetched_addr = (*fetched_addr)->next;
	}
}

int issue_inst(inst_ll* memin_head, inst_ll** issued_addr, int cycle, int* start_flag, func_unit* units_addr_arr[3]) {
	func_unit* cur_unit;
	inst_ll* cur_inst;

	if (*start_flag) {
		cur_inst = memin_head;
	}
	else {
		cur_inst = (*issued_addr)->next;
		if (cur_inst == NULL) {  // all instruction were issued
			*issued_addr = (*issued_addr)->next;
			return 0; // no need to issue more
		}
	}

	if (cur_inst->opcode == 2 || cur_inst->opcode == 3) {
		cur_unit = units_addr_arr[0]; // opcode to look at add stations
		}
	else if (cur_inst->opcode == 4) {
		cur_unit = units_addr_arr[1]; // opcode to look at multiply stations
	}
	else if (cur_inst->opcode == 5) {
		cur_unit = units_addr_arr[2]; // opcode to look at division stations
	}
	else if (cur_inst->opcode == 6) {
		*issued_addr = NULL; // mark all issued
		return -1; // halt, no more fetch and issue
	}
	else {
		printf("Invalid opcode was given\n");
		*issued_addr = (*issued_addr)->next; // advance issue ptr
		return 1; // might be more to issue
	}
	for (int i = 0; i < cur_unit->nr_reserv; i++) {
		if (cur_unit->reserv_stations[i].cur_inst == NULL) { // look for free station
			// take next instruction from fetched queue and place it in the station
			cur_unit->reserv_stations[i].cur_inst = cur_inst;
			cur_inst->tag = i; // update tag
			cur_inst->cycle_issued = cycle;
			// get reg0
			if (regs[cur_inst->src0].qi == NULL) { // reg value can be used
				cur_unit->reserv_stations[i].vj = regs[cur_inst->src0].vi;
			}
			else { // reg value is waiting on a station to finish writing
				cur_unit->reserv_stations[i].qj = regs[cur_inst->src0].qi;
			}
			// get reg1
			if (regs[cur_inst->src1].qi == NULL) { // reg value can be used
				cur_unit->reserv_stations[i].vk = regs[cur_inst->src1].vi;
			}
			else { // reg value is waiting on a station to finish writing
				cur_unit->reserv_stations[i].qk = regs[cur_inst->src1].qi;
			}
			// set reg_dst data as not ready
			regs[cur_inst->dst].qi = &((cur_unit->reserv_stations)[i]);
			if (*start_flag) {
				*issued_addr = memin_head;
				*start_flag = 0;
			}
			else {
				*issued_addr = (*issued_addr)->next; // advance issue ptr
			}
			return 1; // might be more to issue
		}
	}
	return 0; // no free station, dont try to look again in this cycle
}

void check_exec_unit(func_unit* cur_unit, int cycle) {
	reserv_station* station_iter;

	for (int i = 0; i < cur_unit->nr_reserv; i++) {
		// check if an instruction can be executed
		station_iter = (cur_unit->reserv_stations) + i;
		if (station_iter->cur_inst != NULL) {
			// if instruction was not executed and there is a free unit
			if (station_iter->cur_inst->cycle_exec_start == -1 && cur_unit->nr_units_busy < cur_unit->nr_units) {
				// and if the source registers are ready
				if (station_iter->qj == NULL && station_iter->qk == NULL) {
					station_iter->cur_inst->cycle_exec_start = cycle; // ready to execute
					cur_unit->nr_units_busy++; // mark the unit as busy
				}
			}
			else if (station_iter->cur_inst->cycle_exec_end == -1){
				// check if was executed but not finished yet
				if (cycle - station_iter->cur_inst->cycle_exec_start + 1 >= cur_unit->delay) {
					// check if will finish in this cycle
					station_iter->cur_inst->cycle_exec_end = cycle;
				}
			}
		}
	}
}

void check_exec(int cycle, func_unit* units_addr_arr[3]) {
	check_exec_unit(units_addr_arr[0], cycle);
	check_exec_unit(units_addr_arr[1], cycle);
	check_exec_unit(units_addr_arr[2], cycle);
}

void update_station_cbd(func_unit* cur_unit, reserv_station* finished_station_addr, float data) {
	reserv_station* station_iter;

	for (int i = 0; i < cur_unit->nr_reserv; i++) {
		// look for a reserve station waiting of the result
		station_iter = (cur_unit->reserv_stations) + i;
		if (station_iter->cur_inst != NULL && station_iter->qj == finished_station_addr) {
			station_iter->vj = data;
			station_iter->qj = NULL;
		}
		if (station_iter->cur_inst != NULL && station_iter->qk == finished_station_addr) {
			station_iter->vk = data;
			station_iter->qk = NULL;
		}
	}
}

void broadcast_cbd(reserv_station* finished_station_addr, int cycle, func_unit* units_addr_arr[3]) {
	float data = calulate_inst(*finished_station_addr);
	if (regs[finished_station_addr->cur_inst->dst].qi == finished_station_addr) {
		regs[finished_station_addr->cur_inst->dst].vi = data;
		regs[finished_station_addr->cur_inst->dst].qi = NULL;
	}
	update_station_cbd(units_addr_arr[0], finished_station_addr, data);
	update_station_cbd(units_addr_arr[1], finished_station_addr, data);
	update_station_cbd(units_addr_arr[2], finished_station_addr, data);
	write_trace_cbd(finished_station_addr->cur_inst, regs[finished_station_addr->cur_inst->dst].vi);
}

void write_cbd_unit(func_unit* cur_unit, int cycle, func_unit* units_addr_arr[3]) {
	reserv_station* station_iter;

	for (int i = 0; i < cur_unit->nr_reserv; i++) {
		// check if an instruction that started previously is finished
		station_iter = (cur_unit->reserv_stations) + i;
		if (station_iter->cur_inst != NULL && station_iter->cur_inst->cycle_exec_end != -1) {
			if (station_iter->cur_inst->cycle_cdb == -1 && cur_unit->cdb_free) { // check if cdb is free to broadcast
				cur_unit->cdb_free = 0; // mark unit cdb as busy
				station_iter->cur_inst->cycle_cdb = cycle;
				broadcast_cbd(station_iter, cycle, units_addr_arr);
			}
		}
	}
}

void write_cbd(int cycle, func_unit* units_addr_arr[3]) {
	write_cbd_unit(units_addr_arr[0], cycle, units_addr_arr);
	write_cbd_unit(units_addr_arr[1], cycle, units_addr_arr);
	write_cbd_unit(units_addr_arr[2], cycle, units_addr_arr);
}

int look_for_busy_station(func_unit* cur_unit) {
	reserv_station* station_iter;

	for (int i = 0; i < cur_unit->nr_reserv; i++) {
		// check if an instruction that started previously is finished
		station_iter = (cur_unit->reserv_stations) + i;
		if (station_iter->cur_inst != NULL) {
			return 1;
		}
	}
	return 0;
}

int look_for_busy(func_unit* units_addr_arr[3]) {
	if (!look_for_busy_station(units_addr_arr[0])) {
		if (!look_for_busy_station(units_addr_arr[1])) {
			if (!look_for_busy_station(units_addr_arr[2])) {
				return 0;
			}
		}
	}
	return 1;
}

void tomasulo(inst_ll* memin_head, func_unit* units_addr_arr[3]) {

	inst_ll* fetched = NULL, * issued = NULL;
	int cycle = 0, fetch_flag = 1, issue_flag = 1, issue_ret;

	while (1) { // reading intructions		
		// WRITE CBD
		write_cbd(cycle, units_addr_arr);
		
		// EXECUTE
		check_exec(cycle, units_addr_arr);

		// ISSUE
		if (issued != fetched) { // some instructions were fetched but not yet issued
			issue_ret = issue_inst(memin_head, &issued, cycle, &issue_flag, units_addr_arr);
			if (issue_ret == 1 && issued != fetched) { // one issued this cycle and there is more fetched
				issue_ret = issue_inst(memin_head, &issued, cycle, &issue_flag, units_addr_arr);
				if (issue_ret == -1) { // halt issued
					cycle++;
					break;
				}
			}
			else if (issue_ret == -1) {
				cycle++;
				break;
			}
		}

		// FETCH
		if (fetched != NULL || fetch_flag) {
			fetch_inst(memin_head, &fetched, &fetch_flag); // fetch first
			if (fetched != NULL) { // fetched is empty
				fetch_inst(memin_head, &fetched, &fetch_flag); // fetch second
			}
		}
		cycle++;
		set_finished_comp(units_addr_arr);
	}
	while (1) {
		// WRITE CBD
		write_cbd(cycle, units_addr_arr);
		
		// EXECUTE
		check_exec(cycle, units_addr_arr);

		if (!look_for_busy(units_addr_arr)) break;
		cycle++;
		set_finished_comp(units_addr_arr);
	}
}

int main(int argc, char* argv[]) {
	inst_ll* memin_head = NULL;
	func_unit add_units, mul_units, div_units;
	func_unit* units_addr_arr[3] = { &add_units, &mul_units, &div_units };

	if (0 != open_files(argc, argv))
		return -1;
	set_func_units(units_addr_arr);
	fclose(files_handler.config_file);
	memin_head = set_memin();
	fclose(files_handler.meminst_file);
	set_regs();
	if (memin_head != NULL) {
		tomasulo(memin_head, units_addr_arr);
		write_trace_inst(memin_head);
	}
	write_regout();
	close_files();
	free_reserv_stations(units_addr_arr);
	return 0;
}