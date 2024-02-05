#define _CRT_SECURE_NO_WARNINGS
#define MEM_SIZE 4096
#define INST_Q_MAX_SIZE 16
#define INSTRUCTION_LEN 8
#define NUM_OF_FILES 5
#define REG_COUNT 16

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"

reg regs[REG_COUNT]; // global registers array

// ****files management****

int open_files(int argc, char* argv[], FILE* files_handler[NUM_OF_FILES]) {
	// function to open all files and check for errors
	if (argc != 6) {
		printf("Wrong amount of arguments\n");
		return -1;
	}
	files_handler[CONFIG] = fopen(argv[1], "r");
	files_handler[INST_MEM] = fopen(argv[2], "r");
	files_handler[REG_OUT] = fopen(argv[3], "w");
	files_handler[TRACE_INST] = fopen(argv[4], "w");
	files_handler[TRACE_CDB] = fopen(argv[5], "w");
	for (int i = 0; i < NUM_OF_FILES; i++) {
		if (!files_handler[i]) {
			printf("Error opening files\n");
			return -1;
		}
	}
	return 0;
}

void write_trace_cdb(inst_ll* inst, float data, FILE* cdb_file) {
	// function writing trace CDB, will be called in run for every cdb access and write 1 line
	fprintf(cdb_file, "%d %d ", inst->cycle_cdb, inst->pc);
	switch (inst->opcode) {
	case 2:
	case 3:
		fprintf(cdb_file, "ADD ");
		break;
	case 4:
		fprintf(cdb_file, "MUL ");
		break;
	case 5:
		fprintf(cdb_file, "DIV ");
		break;
	}
	fprintf(cdb_file, "%f ", data);
	switch (inst->opcode) {
	case 2:
	case 3:
		fprintf(cdb_file, "ADD");
		break;
	case 4:
		fprintf(cdb_file, "MUL");
		break;
	case 5:
		fprintf(cdb_file, "DIV");
		break;
	}
	fprintf(cdb_file, "%d\n", inst->tag);
}

void write_trace_inst(inst_ll* inst_head, FILE* instrace_file) {
	// function to write instruction trace and free allocated instructions linked list
	// will be called once the algorithm is done simulating
	inst_ll* temp;

	while (inst_head != NULL) {
		if (inst_head->opcode == 6) { // halt reached
			while (inst_head != NULL) {
				temp = inst_head;
				inst_head = inst_head->next;
				free(temp);
			}
			return;
		}
		else {
			fprintf(instrace_file, "0%d", inst_head->opcode);
			fprintf(instrace_file, "%c", give_reg(inst_head->dst));
			fprintf(instrace_file, "%c", give_reg(inst_head->src0));
			fprintf(instrace_file, "%c000 ", give_reg(inst_head->src1));
			fprintf(instrace_file, "%d ", inst_head->pc);
			switch (inst_head->opcode) {
			case 2:
			case 3:
				fprintf(instrace_file, "ADD");
				break;
			case 4:
				fprintf(instrace_file, "MUL");
				break;
			case 5:
				fprintf(instrace_file, "DIV");
				break;
			}
			fprintf(instrace_file, "%d ", inst_head->tag);
			fprintf(instrace_file, "%d %d ", inst_head->cycle_issued, inst_head->cycle_exec_start);
			fprintf(instrace_file, "%d %d\n", inst_head->cycle_exec_end, inst_head->cycle_cdb);
			temp = inst_head;
			inst_head = inst_head->next;
			free(temp);
		}
	}
}

void write_regout(FILE* reg_file) {
	// function to write regout file at the end of simulation
	for (int i = 0; i < REG_COUNT; i++) {
		fprintf(reg_file, "%f\n", regs[i].vi);
	}
}

char read_word(char** buffer, int size, FILE* read_ptr, FileType read_origin) {
	// function that reads one word from a specific file
	char c, * temp = NULL;
	int read = 0;

	while ((c = fgetc(read_ptr)) != EOF && c != '\n' && c != ' ') {
		if (read_origin == CONFIG && read == size - 1) {
			// if it is the config file, dynamic read
			size = 2 * size;
			if (NULL == (temp = (char*)realloc(*buffer, size * sizeof(char)))) {
				printf("Memory allocation failed\n");
				return -1;
			}
			else {
				// saving realloc to temp first will prevent data lose on allocation failure
				*buffer = temp;
			}
		}
		if (read_origin == INST_MEM && read > 7) {
			// from isntruction memory each word will be at most 8 chars, rest ignored
			continue;
		}
		(*buffer)[read] = c; //insert read char to buffer
		read++;
	}
	if (read_origin == CONFIG) (*buffer)[read] = '\0'; // config will be saved as string,instuctions as list of chars.
	return c;
}

// ****setting simulation****

void set_unit(char* param_name, int param_val, func_unit* units_addr_arr[3]) {
	// function to set a unit in the proccessor by parameter name and its value
	if (strcmp(param_name, "add_nr_units") == 0) {
		units_addr_arr[ADD_FUNC]->nr_units = param_val;
		units_addr_arr[ADD_FUNC]->nr_units_busy = 0;
		units_addr_arr[ADD_FUNC]->cdb_free = 1;
	}
	else if (strcmp(param_name, "mul_nr_units") == 0) {
		units_addr_arr[MUL_FUNC]->nr_units = param_val;
		units_addr_arr[MUL_FUNC]->nr_units_busy = 0;
		units_addr_arr[MUL_FUNC]->cdb_free = 1;
	}
	else if (strcmp(param_name, "div_nr_units") == 0) {
		units_addr_arr[DIV_FUNC]->nr_units = param_val;
		units_addr_arr[DIV_FUNC]->nr_units_busy = 0;
		units_addr_arr[DIV_FUNC]->cdb_free = 1;
	}
	else if (strcmp(param_name, "add_nr_reservation") == 0) {
		units_addr_arr[ADD_FUNC]->nr_reserv = param_val;
		units_addr_arr[ADD_FUNC]->reserv_stations = create_reserv_station(param_val);
	}
	else if (strcmp(param_name, "mul_nr_reservation") == 0) {
		units_addr_arr[MUL_FUNC]->nr_reserv = param_val;
		units_addr_arr[MUL_FUNC]->reserv_stations = create_reserv_station(param_val);
	}
	else if (strcmp(param_name, "div_nr_reservation") == 0) {
		units_addr_arr[DIV_FUNC]->nr_reserv = param_val;
		units_addr_arr[DIV_FUNC]->reserv_stations = create_reserv_station(param_val);
	}
	else if (strcmp(param_name, "add_delay") == 0)
		units_addr_arr[ADD_FUNC]->delay = param_val;
	else if (strcmp(param_name, "mul_delay") == 0)
		units_addr_arr[MUL_FUNC]->delay = param_val;
	else if (strcmp(param_name, "div_delay") == 0)
		units_addr_arr[DIV_FUNC]->delay = param_val;
	else
		printf("no match for parameter %s in cfg.txt\n", param_name);
}

void set_processor(func_unit* units_addr_arr[3], FILE* config_file) {
	// function to read config file
	char* param_buffer = NULL, * val_buffer = NULL;
	int got_param = 0, got_val = 0, flag = 1;
	char c;

	while (flag) {
		// read first word, parameter
		if (NULL == (param_buffer = (char*)malloc(5 * sizeof(char)))) {
			printf("Memory allocation failed\n");
			return;
		}
		c = read_word(&param_buffer, 5, config_file, CONFIG);
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

		// skip spaces and equation sign
		if ('=' != fgetc(config_file)
			|| ' ' != fgetc(config_file)) {
			printf("Wrong format for %s in cfg.txt\n", param_buffer);
			free(param_buffer);
			return;
		}

		// read second word, value
		if (NULL == (val_buffer = (char*)malloc(5 * sizeof(char)))) {
			printf("Memory allocation failed\n");
			free(param_buffer);
			return;
		}
		c = read_word(&val_buffer, 5, config_file, CONFIG);
		switch (c) {
			case EOF:
				flag = 0;
			case '\n':
				got_val = 1;
				break;
			case ' ':
				printf("Too many arguments in cfg.txt line\n");
				while ((c = fgetc(config_file)) != EOF && c != '\n');
				break;
		}

		// set parameter with value, and reset for next line
		if (got_param && got_val) set_unit(param_buffer, atoi(val_buffer), units_addr_arr);
		free(param_buffer);
		free(val_buffer);
		got_param = 0;
		got_val = 0;
	}
}

inst_ll* set_memin(FILE* meminst_file) {
	// function reading the instruction memory from file and save it in the program as linked list for simulation
	int pc_count;
	char c = 0, * temp_inst = NULL;
	inst_ll* head = NULL, * list_ptr = NULL;

	for (pc_count = 0; pc_count < 4096; pc_count++) {
		temp_inst = (char*)malloc(INSTRUCTION_LEN * sizeof(char));
		if (temp_inst == NULL) {
			printf("Allocation for instruction failed\n");
			return head;
		}
		memset(temp_inst, 0, INSTRUCTION_LEN);
		c = read_word(&temp_inst, 0, meminst_file, INST_MEM);
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
	// setting registers to initial values
	for (int i = 0; i < 16; i++) {
		regs[i].vi = (float)i;
		regs[i].qi = NULL;
	}
}

// ****simulation tomasulo****

float calulate_inst(reserv_station station_exec) {
	// calculating the result of an instruction
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

int fetch_inst(inst_ll* memin_head, inst_ll** fetched_addr, int* start_flag) {
	// simulating fetch stage

	if (*start_flag) { // fetch the first instruction
		*fetched_addr = memin_head;
		*start_flag = 0;
	}
	else { // fetch the next instrucion from instruction memory
		*fetched_addr = (*fetched_addr)->next;
	}
	if (*fetched_addr != NULL) {
		return 1;
	}
	return 0;
}

int issue_inst(inst_ll* memin_head, inst_ll** issued_addr, int cycle, int* start_flag, int* halt_flag, func_unit* units_addr_arr[3]) {
	// simulating issue stage

	func_unit* cur_unit;
	inst_ll* cur_inst;

	if (*start_flag) { // if no instructions were issued yet, next is the memin head
		cur_inst = memin_head;
	}
	else {
		cur_inst = (*issued_addr)->next;
		if (cur_inst == NULL) {  // all instruction were issued
			*issued_addr = (*issued_addr)->next;
			return 0;
		}
	}

	if (cur_inst->opcode == 2 || cur_inst->opcode == 3) {
		cur_unit = units_addr_arr[ADD_FUNC]; // opcode to look at add stations
		}
	else if (cur_inst->opcode == 4) {
		cur_unit = units_addr_arr[MUL_FUNC]; // opcode to look at multiply stations
	}
	else if (cur_inst->opcode == 5) {
		cur_unit = units_addr_arr[DIV_FUNC]; // opcode to look at division stations
	}
	else if (cur_inst->opcode == 6) {
		*issued_addr = NULL; // mark all issued
		*halt_flag = 1;
		return 1; // halt issued
	}
	else {
		printf("Invalid opcode was given\n");
		*issued_addr = (*issued_addr)->next; // advance issue ptr
		return 1; // slot freed from fetch queue
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
			regs[cur_inst->dst].qi = (cur_unit->reserv_stations) + i;
			if (*start_flag) {
				*issued_addr = memin_head; // mark the first instruction of memin as issued
				*start_flag = 0;
			}
			else {
				*issued_addr = (*issued_addr)->next; // advance issued ptr
			}
			return 1;
		}
	}
	return 0; // no free station, no instruction was issued
}

void exec_inst(int cycle, func_unit* units_addr_arr[3]) {
	// simulating execute stage
	
	func_unit* unit_iter = NULL;
	reserv_station* station_iter = NULL;
	
	for (int j = 0; j < NUM_OF_FUNC; j++) {
		unit_iter = units_addr_arr[j]; // iterate over the 3 functions
		for (int i = 0; i < unit_iter->nr_reserv; i++) {
			station_iter = (unit_iter->reserv_stations) + i; // iterate over reservation stations of that function
			if (station_iter->cur_inst != NULL) {
				// if instruction was not executed yet and there is a free unit
				if (station_iter->cur_inst->cycle_exec_start == -1 && unit_iter->nr_units_busy < unit_iter->nr_units) {
					// and if the source registers are ready
					if (station_iter->qj == NULL && station_iter->qk == NULL) {
						station_iter->cur_inst->cycle_exec_start = cycle; // execute in this round
						unit_iter->nr_units_busy++; // mark the unit as busy
					}
				}
				else if (station_iter->cur_inst->cycle_exec_start != -1 && station_iter->cur_inst->cycle_exec_end == -1) {
					// check if instruction was executed but not finished yet
					if (cycle - station_iter->cur_inst->cycle_exec_start + 1 >= unit_iter->delay) {
						// check if will finish in this cycle
						station_iter->cur_inst->cycle_exec_end = cycle;
					}
				}
			}
		}
	}
}

void update_stations_cdb(func_unit* units_addr_arr[3], reserv_station* finished_station_addr, float data) {
	// function to update all stations on cdb

	func_unit* unit_iter = NULL;
	reserv_station* station_iter = NULL;

	for (int j = 0; j < NUM_OF_FUNC; j++) {
		unit_iter = units_addr_arr[j]; // iterate over the 3 functions
		for (int i = 0; i < unit_iter->nr_reserv; i++) {
			// look for a reserve station waiting for the result
			station_iter = (unit_iter->reserv_stations) + i;
			if (station_iter->cur_inst != NULL && station_iter->qj == finished_station_addr) {
				station_iter->vj = data;
			}
			if (station_iter->cur_inst != NULL && station_iter->qk == finished_station_addr) {
				station_iter->vk = data;
			}
		}
	}
}

void write_cdb(int cycle, func_unit* units_addr_arr[3], FILE* cdb_file) {
	// simulating write cdb stage

	func_unit* unit_iter = NULL;
	reserv_station* station_iter = NULL;

	for (int j = 0; j < NUM_OF_FUNC; j++) {
		unit_iter = units_addr_arr[j]; // iterate over the 3 functions
		for (int i = 0; i < unit_iter->nr_reserv; i++) {
			// check if an instruction that started previously is finished
			station_iter = (unit_iter->reserv_stations) + i;
			if (station_iter->cur_inst != NULL && station_iter->cur_inst->cycle_exec_end != -1) {
				if (station_iter->cur_inst->cycle_cdb == -1 && unit_iter->cdb_free) { // check if cdb is free to broadcast
					unit_iter->cdb_free = 0; // mark unit cdb as busy
					station_iter->cur_inst->cycle_cdb = cycle;
					float data = calulate_inst(*station_iter);
					if (regs[station_iter->cur_inst->dst].qi == station_iter) { // check WAW hazard and update reg
						regs[station_iter->cur_inst->dst].vi = data;
						regs[station_iter->cur_inst->dst].qi = NULL;
					}
					update_stations_cdb(units_addr_arr, station_iter, data); // update all stations on cdb
					write_trace_cdb(station_iter->cur_inst, data, cdb_file); // write trace file
				}
			}
		}
	}
}

int look_for_busy_station(func_unit* units_addr_arr[3]) {
	// function checking if there are still instructions that are not finished
	func_unit* unit_iter = NULL;
	reserv_station* station_iter = NULL;

	for (int j = 0; j < NUM_OF_FUNC; j++) {
		func_unit* unit_iter = units_addr_arr[j];
		for (int i = 0; i < unit_iter->nr_reserv; i++) {
			// check if an instruction that started previously is finished
			station_iter = (unit_iter->reserv_stations) + i;
			if (station_iter->cur_inst != NULL) {
				return 1;
			}
		}
	}
	return 0; // return 0 only if all stations are free
}

void end_cycle(func_unit* units_addr_arr[3]) {
	// set components and data that will be available next cycle to free or valid

	func_unit* cur_unit = NULL;
	reserv_station* iter = NULL;

	for (int j = 0; j < NUM_OF_FUNC; j++) {
		cur_unit = units_addr_arr[j]; // iterate over 3 functions
		for (int i = 0; i < cur_unit->nr_reserv; i++) {
			// iterate over all reservation stations
			iter = (cur_unit->reserv_stations) + i;
			if (iter->cur_inst != NULL) {
				if (iter->cur_inst->cycle_cdb != -1) {
					// if the station sent data on cdb, station is free from next cycle
					iter->cur_inst = NULL;
					iter->vj = -1;
					iter->vk = -1;
					cur_unit->nr_units_busy--;
					continue;
				}
				// if in a station new data arrived at the cycle, mark it valid for next cycle
				if (iter->vj != -1 && iter->qj != NULL) {
					iter->qj = NULL;
				}
				if (iter->vk != -1 && iter->qk != NULL) {
					iter->qk = NULL;
				}
			}
		}
		if (!cur_unit->cdb_free) {
			// transmiting on cdb takes at most 1 cycle, mark free for next cycle
			cur_unit->cdb_free = 1;
		}
	}
}

void tomasulo(inst_ll* memin_head, func_unit* units_addr_arr[3], FILE* cdb_file) {

	inst_ll* fetched = NULL, * issued = NULL;
	int cycle = 0, fetch_flag = 1, issue_flag = 1, halt_flag = 0;
	int inst_q_size = 0;

	while (!halt_flag) { // reading intructions		
		// WRITE CBD
		write_cdb(cycle, units_addr_arr, cdb_file);
		
		// EXECUTE
		exec_inst(cycle, units_addr_arr);

		// ISSUE
		for (int i = 0; i < 2; i++) { // can issue 2 inst per cycle
			if (issued != fetched) { // some instructions were fetched but not yet issued
				inst_q_size -= issue_inst(memin_head, &issued, cycle, &issue_flag, &halt_flag, units_addr_arr);
			}
		}

		// FETCH
		for (int i = 0; i < 2; i++) { // can fetch 2 inst per cycle
			if (inst_q_size < INST_Q_MAX_SIZE && (fetched != NULL || fetch_flag)) {
				// fetched = NULL will mark the end of the linked list of instructions, or the first fetch
				inst_q_size += fetch_inst(memin_head, &fetched, &fetch_flag);
			}
		}
		end_cycle(units_addr_arr);
		cycle++;
	}
	while (1) {
		// fetch and issued finished, complete all instruction in process
		if (!look_for_busy_station(units_addr_arr)) break;

		// WRITE CBD
		write_cdb(cycle, units_addr_arr, cdb_file);
		
		// EXECUTE
		exec_inst(cycle, units_addr_arr);

		end_cycle(units_addr_arr);
		cycle++;
	}
}

int main(int argc, char* argv[]) {
	inst_ll* memin_head = NULL;
	func_unit add_units, mul_units, div_units;
	func_unit* units_addr_arr[3] = { &add_units, &mul_units, &div_units };
	FILE* files_handler[NUM_OF_FILES] = { NULL,NULL,NULL,NULL,NULL };

	if (0 != open_files(argc, argv, files_handler))
		return -1;
	set_processor(units_addr_arr, files_handler[CONFIG]);
	if (files_handler[CONFIG] != NULL) fclose(files_handler[CONFIG]);
	memin_head = set_memin(files_handler[INST_MEM]);
	if (files_handler[INST_MEM] != NULL) fclose(files_handler[INST_MEM]);
	set_regs();
	if (memin_head != NULL) {
		tomasulo(memin_head, units_addr_arr, files_handler[TRACE_CDB]);
		if (files_handler[TRACE_CDB] != NULL) fclose(files_handler[TRACE_CDB]);
		write_trace_inst(memin_head, files_handler[TRACE_INST]);
		if (files_handler[TRACE_INST] != NULL) fclose(files_handler[TRACE_INST]);
	}
	write_regout(files_handler[REG_OUT]);
	if (files_handler[REG_OUT] != NULL) fclose(files_handler[REG_OUT]);
	free_reserv_stations(units_addr_arr);
	return 0;
}