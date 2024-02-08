#define _CRT_SECURE_NO_WARNINGS
#define MEM_SIZE 4096
#define INST_Q_MAX_SIZE 16
#define INSTRUCTION_LEN 8
#define NUM_OF_FILES 5
#define REG_COUNT 16
#define NUM_OF_FUNC 3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ****structs and their functions****

typedef enum FileType {
	CONFIG, // configuration input file
	INST_MEM, // instruction memory input file
	REG_OUT, // register output file
	TRACE_INST, // instruction trace output file
	TRACE_CDB
} FileType;

typedef enum FuncType {
	ADD_FUNC,
	MUL_FUNC,
	DIV_FUNC
} FuncType;

typedef struct InstLinkedList {
	// struct holding an individual instruction
	int pc;
	int opcode;
	int dst;
	int src0;
	int src1;
	struct InstLinkedList* next; // compatible to holding the instructions in linked lists
	int tag; // tag holding the station number, together with opcode creats the full tag
	int cycle_issued;
	int cycle_exec_start;
	int cycle_exec_end;
	int cycle_cdb;
} InstLL;

typedef struct ReservationStation {
	// struct simulating a reservation station
	InstLL* cur_inst; // current instruction in the station, NULL for a free station
	float vj; // value of reg src0, input of FF
	float vk; // value of reg src1, input of FF
	struct ReservationStation* qj; // if value not ready will point to the station is waiting for
	struct ReservationStation* qk; // if value not ready will point to the station is waiting for
	int j_valid; // tag in qj finished and needs to be marked NULL at next cycle
	int k_valid; // tag in qk finished and needs to be marked NULL at next cycle
} ReservStation;

typedef struct FunctionUnit {
	// struct simulating a functional unit, 1 will exist in the program for each ADD MUL DIV
	int nr_units_free; // number of free computational units
	int nr_reserv; // number of reservation station for that function
	ReservStation* reserv_stations; // pointer to hold array of reservation stations
	int delay; // the delay of that function
	int cdb_free; // 1 for cbd is free to write in that cycle, 0 for busy
} FuncUnit;

typedef struct ProgramRegister {
	// struct simulating a register
	float vi; // value in the register
	ReservStation* qi; // NULL for a valid value, pointing to the station computing if not ready
} ProgReg;

ProgReg regs[REG_COUNT]; // global registers array

InstLL* new_inst() {
	// function creating a pointer to a new linked list item
	InstLL* new_inst = NULL;
	new_inst = (InstLL*)malloc(sizeof(InstLL));
	if (new_inst == NULL)
		printf("New instruction allocation falied\n");
	else { // initializing defaults
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

ReservStation* create_reserv_station(int reserv_num) {
	// create the reservation stations for a single function (add, mul or div)
	ReservStation* cur_reserv = (ReservStation*)malloc(reserv_num * sizeof(ReservStation));

	if (cur_reserv != NULL) {
		for (int i = 0; i < reserv_num; i++) { // initializing defaults
			cur_reserv[i].cur_inst = NULL;
			cur_reserv[i].vj = 0;
			cur_reserv[i].vk = 0;
			cur_reserv[i].qj = NULL;
			cur_reserv[i].qk = NULL;
			cur_reserv[i].j_valid = 0;
			cur_reserv[i].k_valid = 0;
		}
	}
	return cur_reserv;
}

void free_reserv_stations(FuncUnit* units_addr_arr[NUM_OF_FUNC]) {
	// free allocated reservation station array
	for (int j = 0; j < NUM_OF_FUNC; j++) {
		free(units_addr_arr[j]->reserv_stations);
	}
}

int get_reg(char reg_char) {
	// translate a register name in hex char to register number
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
	// reverse of get_reg, turn a number to hex char
	if (reg_num < 10) {
		return '0' + reg_num;
	}
	else {
		return 'A' + reg_num - 10;
	}
}

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

void write_trace_cdb(InstLL* inst, float data, FILE* cdb_file) {
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

void write_trace_inst(InstLL* inst_head, FILE* instrace_file) {
	// function to write instruction trace and free allocated instructions linked list
	// will be called once the algorithm is done simulating
	InstLL* temp;

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

// ****setting simulation****

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
	(*buffer)[read] = '\0';
	return c;
}

void set_unit(char* param_name, int param_val, FuncUnit* units_addr_arr[NUM_OF_FUNC]) {
	// function to set a unit in the proccessor by parameter name and its value
	if (strcmp(param_name, "add_nr_units") == 0) {
		units_addr_arr[ADD_FUNC]->nr_units_free = param_val;
		units_addr_arr[ADD_FUNC]->cdb_free = 1;
	}
	else if (strcmp(param_name, "mul_nr_units") == 0) {
		units_addr_arr[MUL_FUNC]->nr_units_free = param_val;
		units_addr_arr[MUL_FUNC]->cdb_free = 1;
	}
	else if (strcmp(param_name, "div_nr_units") == 0) {
		units_addr_arr[DIV_FUNC]->nr_units_free = param_val;
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

void set_processor(FuncUnit* units_addr_arr[NUM_OF_FUNC], FILE* config_file) {
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

InstLL* set_memin(FILE* meminst_file) {
	// function reading the instruction memory from file and save it in the program as linked list for simulation
	int pc_count;
	char c = 0, * temp_inst = NULL;
	InstLL* head = NULL, * list_ptr = NULL;

	for (pc_count = 0; pc_count < 4096; pc_count++) {
		temp_inst = (char*)malloc((INSTRUCTION_LEN + 1) * sizeof(char));
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

float calulate_inst(ReservStation station_exec) {
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

int fetch_inst(InstLL* memin_head, InstLL** fetched_addr, int* start_flag) {
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

int issue_inst(InstLL* memin_head, InstLL** issued_addr, int cycle, int* start_flag, int* halt_flag, FuncUnit* units_addr_arr[NUM_OF_FUNC]) {
	// simulating issue stage

	FuncUnit* cur_unit;
	InstLL* cur_inst;

	if (*start_flag) { // if no instructions were issued yet, next is the memin head
		cur_inst = memin_head;
	}
	else {
		cur_inst = (*issued_addr)->next;
		if (cur_inst == NULL) {  // all instruction were issued
			*issued_addr = NULL;
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

void exec_inst(int cycle, FuncUnit* units_addr_arr[NUM_OF_FUNC]) {
	// simulating execute stage
	
	FuncUnit* unit_iter = NULL;
	ReservStation* station_iter = NULL;
	
	for (int j = 0; j < NUM_OF_FUNC; j++) {
		unit_iter = units_addr_arr[j]; // iterate over the 3 functions
		for (int i = 0; i < unit_iter->nr_reserv; i++) {
			station_iter = (unit_iter->reserv_stations) + i; // iterate over reservation stations of that function
			if (station_iter->cur_inst != NULL) {
				// if instruction was not executed yet and there is a free unit
				if (station_iter->cur_inst->cycle_exec_start == -1 && unit_iter->nr_units_free > 0) {
					// and if the source registers are ready
					if (station_iter->qj == NULL && station_iter->qk == NULL) {
						station_iter->cur_inst->cycle_exec_start = cycle; // execute in this round
						unit_iter->nr_units_free--; // mark the unit as busy
					}
				}
				if (station_iter->cur_inst->cycle_exec_start != -1 && station_iter->cur_inst->cycle_exec_end == -1) {
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

void update_stations_cdb(FuncUnit* units_addr_arr[NUM_OF_FUNC], ReservStation* finished_station_addr, float data) {
	// function to update all stations on cdb

	FuncUnit* unit_iter = NULL;
	ReservStation* station_iter = NULL;

	for (int j = 0; j < NUM_OF_FUNC; j++) {
		unit_iter = units_addr_arr[j]; // iterate over the 3 functions
		for (int i = 0; i < unit_iter->nr_reserv; i++) {
			// look for a reserve station waiting for the result
			station_iter = (unit_iter->reserv_stations) + i;
			if (station_iter->cur_inst != NULL && station_iter->qj == finished_station_addr) {
				station_iter->vj = data;
				station_iter->j_valid = 1;
			}
			if (station_iter->cur_inst != NULL && station_iter->qk == finished_station_addr) {
				station_iter->vk = data;
				station_iter->k_valid = 1;
			}
		}
	}
}

void write_cdb(int cycle, FuncUnit* units_addr_arr[NUM_OF_FUNC], FILE* cdb_file) {
	// simulating write cdb stage

	FuncUnit* unit_iter = NULL;
	ReservStation* station_iter = NULL;

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

int end_cycle(int* cycle_addr, FuncUnit* units_addr_arr[NUM_OF_FUNC]) {
	// set components and data that will be available next cycle to free or valid
	int inst_count = 0;
	FuncUnit* cur_unit = NULL;
	ReservStation* iter = NULL;

	for (int j = 0; j < NUM_OF_FUNC; j++) {
		cur_unit = units_addr_arr[j]; // iterate over 3 functions
		for (int i = 0; i < cur_unit->nr_reserv; i++) {
			// iterate over all reservation stations
			iter = (cur_unit->reserv_stations) + i;
			if (iter->cur_inst != NULL) {
				inst_count++;
				if (iter->cur_inst->cycle_cdb != -1) {
					// if the station sent data on cdb, station is free from next cycle
					iter->cur_inst = NULL;
					cur_unit->nr_units_free++;
					continue;
				}
				// if in a station new data arrived at the cycle
				if (iter->j_valid) {
					iter->qj = NULL; // mark it valid for next cycle
					iter->j_valid = 0;
				}
				if (iter->k_valid) {
					iter->qk = NULL;
					iter->k_valid = 0;
				}
			}
		}
		if (!cur_unit->cdb_free) {
			// transmiting on cdb takes at most 1 cycle, mark free for next cycle
			cur_unit->cdb_free = 1;
		}
	}
	*cycle_addr += 1;
	return inst_count;
}

void tomasulo(InstLL* memin_head, FuncUnit* units_addr_arr[NUM_OF_FUNC], FILE* cdb_file) {

	InstLL* fetched = NULL, * issued = NULL;
	int cycle = 0, fetch_flag = 1, issue_flag = 1, halt_flag = 0;
	int inst_q_size = 0, temp;

	while (!halt_flag) { // reading intructions		
		// WRITE CBD
		write_cdb(cycle, units_addr_arr, cdb_file);
		
		// EXECUTE
		exec_inst(cycle, units_addr_arr);

		// ISSUE
		for (int i = 0; i < 2; i++) { // can issue 2 inst per cycle
			if (issued == fetched) { // all fetched were issued
				break;
			}
			int temp = issue_inst(memin_head, &issued, cycle, &issue_flag, &halt_flag, units_addr_arr);
			if (temp == 0) break; // nothing to issue in this cycle
			inst_q_size -= temp;
		}

		// FETCH
		for (int i = 0; i < 2; i++) { // can fetch 2 inst per cycle
			if (inst_q_size == INST_Q_MAX_SIZE || (fetched == NULL && !fetch_flag)) {
				// fetched = NULL will mark the end of the linked list of instructions, or the first fetch
				break;
			}
			inst_q_size += fetch_inst(memin_head, &fetched, &fetch_flag);
		}
		end_cycle(&cycle, units_addr_arr);
	}
	// fetch and issued finished, complete all instruction in process
	while (1) {
		// WRITE CBD
		write_cdb(cycle, units_addr_arr, cdb_file);
		
		// EXECUTE
		exec_inst(cycle, units_addr_arr);

		if (0 == end_cycle(&cycle, units_addr_arr)) break;
	}
}

int main(int argc, char* argv[]) {
	InstLL* memin_head = NULL;
	FuncUnit add_units, mul_units, div_units;
	FuncUnit* units_addr_arr[NUM_OF_FUNC] = { &add_units, &mul_units, &div_units };
	FILE* files_handler[NUM_OF_FILES] = { NULL,NULL,NULL,NULL,NULL };

	// setting simulation
	if (0 != open_files(argc, argv, files_handler))
		return -1;
	set_processor(units_addr_arr, files_handler[CONFIG]);
	if (files_handler[CONFIG] != NULL) fclose(files_handler[CONFIG]);
	memin_head = set_memin(files_handler[INST_MEM]);
	if (files_handler[INST_MEM] != NULL) fclose(files_handler[INST_MEM]);
	set_regs();

	// tomasulo simulation
	if (memin_head != NULL) {
		tomasulo(memin_head, units_addr_arr, files_handler[TRACE_CDB]);
	}

	// finishing simulation
	if (files_handler[TRACE_CDB] != NULL) fclose(files_handler[TRACE_CDB]);
	write_trace_inst(memin_head, files_handler[TRACE_INST]);
	if (files_handler[TRACE_INST] != NULL) fclose(files_handler[TRACE_INST]);
	write_regout(files_handler[REG_OUT]);
	if (files_handler[REG_OUT] != NULL) fclose(files_handler[REG_OUT]);
	free_reserv_stations(units_addr_arr);
	return 0;
}