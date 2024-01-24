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
} inst_ll;

typedef struct reserv_station {
	inst_ll* cur_inst;
	int vj;
	int vk;
	struct reserv_station* qj;
	struct reserv_station* qk;
} reserv_station;

typedef struct function_unit {
	int nr_units;
	int nr_reserv;
	reserv_station* reserv_stations;
	int delay;
} func_unit;

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
	}
	return new_inst;
}

inst_ll* get_next_inst(inst_ll** list_head) {
	inst_ll* temp;

	if (*list_head == NULL) return NULL; // empty list
	temp = *list_head; // item to return
	*list_head = (*list_head)->next; // advance list head
	temp->next = NULL; // cut item to return from list
	return temp;
}

int intlen(int num) {
	int count = 0;

	while (num > 0) {
		num = num / 10;
		count++;
	}
	return count;
}