#ifndef STRUCTS_H
#define STRUCTS_H

#define NUM_OF_FUNC 3

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

typedef struct inst_linked_list {
	// struct holding an individual instruction
	int pc;
	int opcode;
	int dst;
	int src0;
	int src1;
	struct inst_linked_list* next; // compatible to holding the instructions in linked lists
	int tag; // tag holding the station number, together with opcode creats the full tag
	int cycle_issued;
	int cycle_exec_start;
	int cycle_exec_end;
	int cycle_cdb;
} inst_ll;

typedef struct reserv_station {
	// struct simulating a reservation station
	inst_ll* cur_inst; // current instruction in the station, NULL for a free station
	float vj; // value of reg src0, -1 when empty
	float vk; // value of reg src1, -1 when empty
	struct reserv_station* qj; // if value not ready will point to the station is waiting for
	struct reserv_station* qk; // if value not ready will point to the station is waiting for
} reserv_station;

typedef struct function_unit {
	// struct simulating a functional unit, 1 will exist in the program for each ADD MUL DIV
	int nr_units; // number of computational units of that function
	int nr_units_busy; // count the number who are busy at any moment
	int nr_reserv; // number of reservation station for that function
	reserv_station* reserv_stations; // pointer to hold array of reservation stations
	int delay; // the delay of that function
	int cdb_free; // 1 for cbd is free to write in that cycle, 0 for busy
} func_unit;

typedef struct reg {
	// struct simulating a register
	float vi; // value in the register
	reserv_station* qi; // NULL for a valid value, pointing to the station computing if not ready
} reg;

inst_ll* new_inst() {
	// function creating a pointer to a new linked list item
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

reserv_station* create_reserv_station(int reserv_num) {
	// create the reservation stations for a single function (add, mul or div)
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

void free_reserv_stations(func_unit* units_addr_arr[3]) {
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

#endif