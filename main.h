#define MEM_SIZE 4096
#define INSTRUCTION_LEN 8
#define REG_COUNT 16

typedef struct inst_linked_list {
	int pc;
	int opcode;
	int dst;
	int src0;
	int src1;
	struct inst_linked_list* next;
	int tag;
	int cycle_issued;
	int cycle_exec;
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
	int nr_reserv;
	reserv_station* reserv_stations;
	int delay;
} func_unit;

typedef struct reg {
	float vi;
	reserv_station* qi;
} reg;

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
		new_inst->cycle_exec = -1;
		new_inst->cycle_cdb = -1;
	}
	return new_inst;
}

inst_ll* fetch_inst(inst_ll** list_head) {
	inst_ll* temp;

	if (*list_head == NULL) return NULL; // empty list
	temp = *list_head; // item to return
	*list_head = (*list_head)->next; // advance list head
	temp->next = NULL; // cut item to return from list
	return temp;
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

char give_reg(int reg_num) {
	if (reg_num < 10) {
		return '0' + reg_num;
	}
	else {
		return 'A' + 10 - reg_num;
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