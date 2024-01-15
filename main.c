#define _CRT_SECURE_NO_WARNINGS
#define MEM_SIZE 4096
#define INSTRUCTION_LEN 8

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

struct {
	int add_nr_units;
	int mul_nr_units;
	int div_nr_units;
	int add_nr_reserv;
	int mul_nr_reserv;
	int div_nr_reserv;
	int add_delay;
	int mul_delay;
	int div_delay;
} func_units;

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
		//printf("i read %c\n", c);
		if (read == size - 1 && read_origin == files_handler.config_file) {
			size = 2 * size;
			if (NULL == (*buffer = (char*)realloc(*buffer, size * sizeof(char)))) {
				printf("Memory allocation failed\n");
				return -1;
			}
		}
		if (read_origin == files_handler.config_file)
			(*buffer)[read] = c;
		else if (read_origin == files_handler.meminst_file && read < 8){
			(*buffer)[INSTRUCTION_LEN - read - 1] = c;
		}
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
		//printf("%s\n", param_buffer);
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

void set_memin(char* memin) {
	int flag = 1, i;
	char* start, * end, temp, c = 0;

	memset(memin, '0', MEM_SIZE * INSTRUCTION_LEN);
	for (i = 0; i < 4096; i++) {
		if (c != EOF) {
			char* ptr = memin + i * INSTRUCTION_LEN;
			c = read_word(&ptr, 0, files_handler.meminst_file);
			start = memin + i * INSTRUCTION_LEN;
			while (*start == '0') start++;
			end = memin + (i + 1) * INSTRUCTION_LEN - 1;
			while (end > start) {
				temp = *start;
				*start = *end;
				*end = temp;
				start++;
				end--;
			}
		}
		else {
			break;
		}
	}
}

int main(int argc, char* argv[]) {
	char memin[MEM_SIZE * INSTRUCTION_LEN];
	int i, j;
	if (0 != get_args(argc, argv))
		return -1;
	set_func_units();
	set_memin(memin);
	for (i = 0; i < 10; i++) {
		printf("%d: ", i);
		for (j = 0; j < 8; j++) {
			printf("%c", memin[i* INSTRUCTION_LEN+j]);
		}
		printf("\n");
	}
	printf("%d", func_units.add_nr_units);
}