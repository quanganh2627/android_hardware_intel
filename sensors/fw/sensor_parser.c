/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * XML Parser for General Sensor Driver Config
 * Parse XML file and generate config firmware image
 * Dump config firmware image to text
 *
 * Date: 	June 2013
 * Authors: 	PSI IO & Sensor Team
 *		qipeng.zha@intel.com
 */

#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <linux/types.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "sensor_driver_config.h"
#include "sensor_parser.h"

#define SENSOR_PARSER_DBG

#ifdef SENSOR_PARSER_DBG

#define LEVEL1		1
#define LEVEL2		2
#define LEVEL3		3
#define LEVEL4		4

static int dbg_level = 0;
#define DBG(level, fmt, ...)				\
do {							\
        if (level <= dbg_level)				\
                printf("[%d]%s "fmt"\n",		\
		__LINE__, __func__, ##__VA_ARGS__);	\
} while(0)

#else

#define DBG(level, fmt, ...)

#endif

struct sensor_parser sensor_parser;

/* Intermediate action attributes
*/
struct im_op_attr im_op_attrs[] = {
	{"==", 2, 2, 40, IM_LOGIC_EQ, OP_LOGIC_EQ},
	{"!=", 2, 2, 40, IM_LOGIC_NEQ, OP_LOGIC_NEQ},
	{">=", 2, 2, 40, IM_LOGIC_GE, OP_LOGIC_GE},
	{"<=", 2, 2, 40, IM_LOGIC_LE, OP_LOGIC_LE},

	{"&&", 2, 2, 20, IM_LOGIC_AND, OP_LOGIC_AND},
	{"||", 2, 2, 20, IM_LOGIC_OR, OP_LOGIC_OR},

	{"<<", 2, 2, 10, IM_BIT_LSL, OP_BIT_LSL},
	{">>", 2, 2, 10, IM_BIT_LSR, OP_BIT_LSR},

	{"|", 1, 2, 30, IM_BIT_OR, OP_BIT_OR},
	{"&", 1, 2, 30, IM_BIT_AND, OP_BIT_AND},
	{"~", 1, 1, 30, IM_BIT_NOR, OP_BIT_NOR},

	{">", 1, 2, 40, IM_LOGIC_GREATER, OP_LOGIC_GREATER},
	{"<", 1, 2, 40, IM_LOGIC_LESS, OP_LOGIC_LESS},

	{"+", 1, 2, 50, IM_ARI_ADD, OP_ARI_ADD},
	{"-", 1, 2, 50, IM_ARI_SUB, OP_ARI_SUB},

	{"*", 1, 2, 60, IM_ARI_MUL, OP_ARI_MUL},
	{"/", 1, 2, 60, IM_ARI_DIV, OP_ARI_DIV},
	{"%", 1, 2, 60, IM_ARI_MOD, OP_ARI_MOD},

	{"(", 1, 0, 100, IM_LEFT_BRACE, OP_RESERVE},
	{")", 1, 0, 1, IM_RIGHT_BRACE, OP_RESERVE},

	{"=", 1, 2, 5, IM_ASSIGN, OP_ACCESS},

/*these action will be handle in top priority
* so can translate immediately like else/endif/sleep
*/
	{"be16_to_cpu", 11, 1, 200, IM_ENDIAN_BE16, OP_ENDIAN_BE16},
	{"be16u_to_cpu", 12, 1, 200, IM_ENDIAN_BE16_UN, OP_ENDIAN_BE16_UN},
	{"be24_to_cpu", 11, 1, 200, IM_ENDIAN_BE24, OP_ENDIAN_BE24},
	{"be32_to_cpu", 11, 1, 200, IM_ENDIAN_BE32, OP_ENDIAN_BE32},
	{"le16_to_cpu", 11, 1, 200, IM_ENDIAN_LE16, OP_ENDIAN_LE16},
	{"le16u_to_cpu", 12, 1, 200, IM_ENDIAN_LE16_UN, OP_ENDIAN_LE16_UN},
	{"le24_to_cpu", 11, 1, 200, IM_ENDIAN_LE24, OP_ENDIAN_LE24},
	{"le32_to_cpu", 11, 1, 200, IM_ENDIAN_LE32, OP_ENDIAN_LE32},

	{"min", 3, 2, 200, IM_MIN, OP_MIN},
	{"max", 3, 2, 200, IM_MAX, OP_MAX},
};

static void display_help(void)
{
	printf(
	"Usage: sensorparser <OPTIONS>\n"
	"Parser or Dump sensor driver config info from XML or image file\n"
	"           --help               Display this help and exit\n"
	"-p         --parser             Parser xml, or Dump firmware\n"
	"-x file    --xml=file           XML config file\n"
	"-f file    --firmware=file      Firmware image file\n"
	"-q         --quiet=0/1/2/3/	 Print level of debug message\n"
	"Example:\n"
	"  ./sensor_parser -p -x sensor_driver_config.xml -f sensor_config.bin\n"
	"  ./sensor_parser -f sensor_config.bin > dump\n"
	"\n");

	exit(EXIT_SUCCESS);
}

/* Option variables*/
static enum { PARSER = 0, DUMP } parser_dump = DUMP;
static const char *xmlfile = NULL;
static const char *firmwarefile = NULL;
/*static const char *dumpfile = NULL;*/

static void process_options(int argc, char *const argv[])
{
	for (;;) {
		int option_index = 0;
		static const char *short_options = "px:f:q:";
		static const struct option long_options[] = {
			{"help", no_argument, 0, 0},
			{"parser", no_argument, 0, 'p'},
			{"xml", required_argument, 0, 'x'},
			{"firmware", required_argument, 0, 'f'},
/*			{"dump", required_argument, 0, 'd'}, */
			{"quiet", required_argument, 0, 'q'},
			{0, 0, 0, 0},
		};

		int c = getopt_long(argc, argv, short_options,
				long_options, &option_index);
		if (c == EOF) {
			break;
		}

		switch (c) {
			case 0:
				display_help();
				break;
			case 'p':
				parser_dump = PARSER;
				break;
			case 'x':
				if (!(xmlfile = strdup(optarg))) {
					perror("stddup");
					exit(-1);
				}
				break;
			case 'f':
				if (!(firmwarefile = strdup(optarg))) {
					perror("stddup");
					exit(-1);
				}
				break;
			case 'q':
				dbg_level = strtol(optarg, NULL, 0);
				break;
		}
	}

	if (parser_dump == PARSER) {
		if (!xmlfile || !firmwarefile)
			display_help();
	} else {
		if (!firmwarefile)
			display_help();
	}
}

int main(int argc, char *argv[])
{
	int ret = 0;

	process_options(argc, argv);

	if (parser_dump == PARSER)
		ret = parser();
	else
		ret = dump();

	return ret;
}

static int dump(void)
{
	char *buf = NULL;
	int fd_firm = 0;
	int size;
	int ret;
	struct sensor_config_image image;
	struct sensor_config_image *image_ptr;
	struct sensor_config *config;
	int i;

	/*firmware*/
	fd_firm = open(firmwarefile, O_RDONLY);
	if(fd_firm <= 0)
	{
		printf("open file error %s\n", firmwarefile);
		ret = -1;
		goto err;
	}

	ret = read(fd_firm, &image, sizeof(image));
	if (ret != sizeof(image)) {
		printf("read file error %d\n", ret);
		goto err;
	}

	if (image.magic != 0x1234) {
		printf("wrong firmware image\n");
		goto err;
	}

	/*data buf*/
	size = lseek64(fd_firm, 0, SEEK_END);
	printf("Image size: %d\n", size);
	buf = malloc(size);
	if(!buf)
	{
		printf("Fail to alloc memory %d\n", size);
		ret = -1;
		goto err;
	}
	else
		memset(buf, 0, size);

	lseek64(fd_firm, 0, SEEK_SET);
	ret = read(fd_firm, buf, size);
	if (ret != size) {
		printf("read file error %d\n", ret);
		goto err;
	}

	/*dump sensor image*/
	image_ptr = (struct sensor_config_image *)buf;
	config = (struct sensor_config *)&image_ptr->configs;

	printf("flags: %d\n", image_ptr->flags);
	printf("dbg_sensors: %d\n", image_ptr->dbg_sensors);
	printf("dbg_level: %d\n", image_ptr->dbg_level);
	for (i = 0; i < image.num; i++)
	{
		dump_sensor_config(config);
		config = (struct sensor_config *)
			((char *)config + config->size);
	}

	ret = 0;
err:
	if (fd_firm)
		close(fd_firm);
	if (buf)
		free(buf);

	return ret;
}

static int parser(void)
{
	char *buf = NULL;
	int fd;
	int size;
	int ret;
	int sensor_num = 0;
	struct sensor_config_image *image;
	struct sensor_config *config;
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr p;
	char *str = NULL;

	/*xml*/
	doc = xmlReadFile(xmlfile, NULL, XML_PARSE_NOBLANKS);
	if (doc == NULL) {
		printf("Fail to read XML Document\n");
		return -1;
	}
	root = xmlDocGetRootElement(doc);
	if (root == NULL) {
		printf("Empty XML document\n");
		xmlFreeDoc(doc);
		return -1;
	}
	if (xmlStrcmp(root->name, (const xmlChar *) "sensor_driver_config")) {
		printf("Wrong XML document"
			"cannot find \"sensor_driver_config\" element!\n");
		xmlFreeDoc(doc);
		return -1;
	}
	p = root->xmlChildrenNode;
	while (p != NULL) {
		sensor_num++;
		p = p->next;
	}

	/*firmware*/
	fd = open(firmwarefile, O_CREAT|O_RDWR, S_IRWXU);
	if(fd <= 0)
	{
		printf("open file error %s\n", firmwarefile);
		xmlFreeDoc(doc);
		return -1;
	}

	/*data buf*/
#define MAX_FIRMWARE_SIZE 0x20000
	size = MAX_FIRMWARE_SIZE;
	buf = malloc(size);
	if(!buf)
	{
		printf("Fail to alloc memory %d\n", size);
		ret = -1;
		goto out;
	}
	else
		memset(buf, 0, size);

	/*init sensor image*/
	image = (struct sensor_config_image *)buf;
	image->magic = 0x1234;
	image->num = sensor_num;

	str = (char *)xmlGetProp(root, (const xmlChar*)"flags");
	if (str) {
		image->flags = strtoul((const char*)str, NULL, 0);
		xmlFree(str);
	}

	str = (char *)xmlGetProp(root, (const xmlChar*)"dbg_sensors");
	if (str) {
		image->dbg_sensors = strtoul((const char*)str, NULL, 0);
		xmlFree(str);
	}

	str = (char *)xmlGetProp(root, (const xmlChar*)"dbg_level");
	if (str) {
		image->dbg_level = strtoul((const char*)str, NULL, 0);
		xmlFree(str);
	}

	config = (struct sensor_config *)&image->configs;
	size = sizeof(struct sensor_config_image) - sizeof(int *);
	ret = sensor_parser_sensors(root->xmlChildrenNode,
				sensor_num, config, &size);
	if (ret)
	{
		printf("Fail to parser sensor XML config\n");
		goto out;
	}

	/*write into file*/
	ret = write(fd, buf, size);
	if (ret != size)
	{
		printf("Fail to write sensor config, ret:%d\n", ret);
		ret = -1;
		goto out;
	}
	ret = ftruncate(fd, size);
	if (ret)
	{
		printf("Fail to truncate sensor config, ret:%d\n", ret);
		goto out;
	}
	fsync(fd);

	printf("\nSuccessfully generate image"
			"num:%d size:%d\n", sensor_num, size);
	ret = 0;
out:
	close(fd);
	if(buf)
		free(buf);
	xmlFreeDoc(doc);

	return ret;
}

#define MAX_SENSORS		0x20
static int sensor_parser_sensors(xmlNodePtr node, int num,
			struct sensor_config *configs, int *size)
{
	int ret;
	xmlNodePtr NodeBuf[MAX_SENSORS] = { NULL };
	int cnt_buf[MAX_SENSORS] = { 0 };
	struct sensor_config *config = configs;
	int i;
	int j;

	ret = sensor_xmlnode_sort(node, NodeBuf, cnt_buf);
	if (ret)
	{
		printf("Fail to sort sensor config\n");
		ret = -1;
		goto out;
	}

	for (i = 0; i < num; i++)
	{
		xmlNodePtr p = NodeBuf[i];

		ret = sensor_config_init(p->xmlChildrenNode, config);
		if (ret)
		{
			char *name = (char *)xmlGetProp(p, (const xmlChar*)"name");

			if (name) {
				printf("Fail to init sensor config  %s\n", name);
				xmlFree(name);
			}
			goto out;
		}

		*size += config->size;
		config = (struct sensor_config *)((char *)config + config->size);
	}

	/*adjust for multi func device*/
	for (config = configs, i = 0; i < MAX_SENSORS && cnt_buf[i]; i++) {
		int shared_num = cnt_buf[i];
		struct sensor_config *share_config;
		int max_regs = config->sensor_regs;

		/*set shared_num*/
		for (share_config = config, j = 0; j < shared_num; j++) {
			share_config->shared_nums = shared_num;
			if (max_regs < share_config->sensor_regs)
				max_regs = share_config->sensor_regs;

			share_config = (struct sensor_config *)
				((char *)share_config + share_config->size);
		}

		/*set sensor_regs for multi device*/
		for (share_config = config, j = 0; j < shared_num; j++) {
			share_config->sensor_regs = max_regs;

			share_config = (struct sensor_config *)
				((char *)share_config + share_config->size);
		}

		while (shared_num--)
			config = (struct sensor_config *)
					((char *)config + config->size);
	}

	ret = 0;
out:
	return ret;
}

static char *sensor_action_debug[] = {
	"INIT", "DEINIT",
	"ENABLE", "DISABLE",
	"INT_ACK",
	"GET_DATA_X", "GET_DATA_Y", "GET_DATA_Z",
};

static void dump_sensor_config(struct sensor_config *config)
{
	int i;

	printf("======Sensor Config======\n");
	printf("size:    \t\t%d\n", config->size);
	/*basic info*/
	printf("i2c_bus: \t\t%d\n", config->i2c_bus);
	printf("test_reg_addr: \t\t0x%02x\n", config->test_reg_addr);
	printf("i2c_addr: \t\t0x%02x 0x%02x 0x%02x 0x%02x\n",
			config->i2c_addrs[0], config->i2c_addrs[1],
			config->i2c_addrs[2], config->i2c_addrs[3]);
	printf("id:     \t\t0x%02x 0x%02x 0x%02x 0x%02x\n", config->id[0],
			config->id[1], config->id[2], config->id[3]);
	printf("name:     \t\t%s\n", config->name);
	printf("input name: \t\t%s\n", config->input_name);
	printf("attr name: \t\t%s\n", config->attr_name);
	printf("id_reg_addr: \t\t0x%02x\n", config->id_reg_addr);
	printf("id_reg_flag: \t\t0x%02x\n", config->id_reg_flag);
	printf("sensor_regs: \t\t%d\n", config->sensor_regs);
	printf("event_type: \t\t%d\n", config->event_type);

	printf("shared_nums: \t\t%d\n", config->shared_nums);
	printf("irq_serialize: \t\t%d\n", config->irq_serialize);

	printf("odr_entries: \t\t%d\n", config->odr_entries);
	for (i = 0; i < config->odr_entries; i++) {
		struct odr *odr = &config->odr_table[i];
		struct lowlevel_action *action_table =
				(struct lowlevel_action *)&config->actions;

		printf(" hz:      \t\t%dHz\n", odr->hz);
		printf(" actions: \t\t%d\n", odr->index.num);
		dump_ll_action(&action_table[odr->index.index],
						odr->index.num, 0);
	}

	printf("default_range: \t\t%d\n", config->default_range);
	printf("range_entries: \t\t%d\n", config->range_entries);
	for (i = 0; i < config->range_entries; i++) {
		struct range_setting *range = &config->range_table[i];
		struct lowlevel_action *action_table =
				(struct lowlevel_action *)&config->actions;

		printf(" range:   \t\t%d\n", range->range);
		printf(" actions: \t\t%d\n", range->index.num);
		dump_ll_action(&action_table[range->index.index],
						range->index.num, 0);
	}

	printf("method: \t\t%d\n", config->method);
	printf("default poll interval:\t%d\n", config->default_poll_interval);
	printf("min poll interval:\t%d\n", config->min_poll_interval);
	printf("max poll interval:\t%d\n", config->max_poll_interval);

	printf("gpio_num: \t\t%d\n", config->gpio_num);
	printf("report_cnt: \t\t%d\n", config->report_cnt);
	printf("report_interval:\t%d\n", config->report_interval);
	printf("irq_flag: \t\t0x%08x\n", config->irq_flag);

	printf("sysfs_entries: \t\t%d\n", config->sysfs_entries);
	for (i = 0; i < config->sysfs_entries; i++) {
		struct sysfs_entry *entry = &config->sysfs_table[i];
		struct lowlevel_action *action_table =
				(struct lowlevel_action *)&config->actions;
		struct lowlevel_action_index *index;

		printf(" name \t\t%s\n", entry->name);
		printf(" mode \t\t%d\n", entry->mode);

		if (entry->type == DATA_ACTION) {
			index = &entry->action.data.index_show;
			printf(" show actions:\t\t%d\n", index->num);
			dump_ll_action(&action_table[index->index],
							index->num, 0);

			index = &entry->action.data.index_store;
			printf(" store actions:\t\t%d\n", index->num);
			dump_ll_action(&action_table[index->index],
							index->num, 0);
		}

	}

	for (i = 0; i <= GET_DATA_Z; i++) {
		struct lowlevel_action *action_table =
				(struct lowlevel_action *)&config->actions;

		printf("Action: %s\n", sensor_action_debug[i]);
		printf("\tindex     \t\t%d\n", config->indexs[i].index);
		printf("\tnum       \t\t%d\n", config->indexs[i].num);

		dump_ll_action(&action_table[config->indexs[i].index],
						config->indexs[i].num, 0);
	}
}

static int sensor_xmlnode_sort(xmlNodePtr node,
		xmlNodePtr *NodeBuf, int *cnt_buf)
{
	int ret;
	int i;
	int share = 0;
	int sort_num = 0;
	xmlNodePtr p1;
	xmlNodePtr p2;
	xmlNodePtr p3;

	/*handle all master&slave*/
	for (p1 = node; p1; p1 = p1->next) {
		int i;
		char *master_name = NULL;

		for (i = 0; i < sort_num; i++) {
			if (NodeBuf[i] == p1)
				break;
		}
		if (i < sort_num)
			continue; /*already in NodeBuf*/

		/*get master of first depend_on node*/
		master_name = (char *)xmlGetProp(p1, (const xmlChar*)"depend_on");
		if (!master_name)
			continue;

		/*get master node*/
		for (p2 = node; p2; p2 = p2->next) {
			char *name = NULL;
			name = (char *)xmlGetProp(p2, (const xmlChar*)"name");

			if (name) {
				if (!strcmp(master_name, name)) {
					NodeBuf[sort_num] = p2;
					sort_num++;
					break;
				}
				xmlFree(name);
			}
		}
		if (!p2) {
			printf("Error, found no master sensor %s\n", master_name);
			ret = -1;
			goto err;
		}

		cnt_buf[share] = 1;

		/*get all slaves*/
		for (p3 = node; p3; p3 = p3->next) {
			char *name = NULL;

			name = (char *)xmlGetProp(p3, (const xmlChar*)"depend_on");
			if (name) {
				if (!strcmp(master_name, name)) {
					NodeBuf[sort_num] = p3;
					sort_num++;
					cnt_buf[share]++;
				}
				xmlFree(name);
			}
		}
		xmlFree(master_name);
		share++;
	}

	/*handle all others*/
	for (p1 = node; p1; p1 = p1->next) {

		for (i = 0; i < sort_num; i++) {
			if (NodeBuf[i] == p1)
				break;
		}
		if (i < sort_num)
			continue; /*already in NodeBuf*/

		NodeBuf[sort_num] = p1;
		sort_num++;
	}

	ret = 0;
err:
	return ret;
}

static int sensor_parser_init(struct sensor_parser *parser,
				struct sensor_config *config)
{
	memset(parser, 0, sizeof(*parser));
	parser->config = config;

	parser->sensor_regs = 0;
	parser->id_reg_flag = 0;
	parser->test_reg_addr = SENSOR_INVALID_REG;

	return 0;
}

static int search_imops_by_op(char *op)
{
	int i;
	struct im_op_attr *ops = im_op_attrs;

	for (i = 0; i < ARRAY_SIZE(im_op_attrs); i++, ops++)
	{
		if(!strncmp(op, ops->op, ops->op_size))
		{
			DBG(LEVEL2, "%s %d", ops->op, i);
			return i;
		}
	}

	printf("[%d]%s Error %s\n", __LINE__, __func__, op);
	return -1;
}

static int search_imops_by_type(enum im_op_type type)
{
	int i;
	struct im_op_attr *ops = im_op_attrs;

	for (i = 0; i < ARRAY_SIZE(im_op_attrs); i++, ops++)
	{
		if(type == ops->type)
			return i;
	}

	printf("[%d]%s Error %d\n", __LINE__, __func__, type);
	return -1;
}

static int imops_priority_type(enum im_op_type type)
{
	int i;
	struct im_op_attr *ops = im_op_attrs;

	for (i = 0; i < ARRAY_SIZE(im_op_attrs); i++, ops++)
	{
		if(type == ops->type)
			return ops->priority;
	}

	printf("[%d]%s Error %d\n", __LINE__, __func__, type);
	return -1;
}

void init_operand_stack(struct im_operand_stack *stack)
{
	stack->top = -1;
}

int operand_stack_empty(struct im_operand_stack *stack)
{
	return stack->top == -1;
}

inline int push_operand(struct im_operand_stack *stack, struct im_operand *oper)
{
	stack->top++;

	if (stack->top < 0 || stack->top >= IM_STACK_MAX_SIZE)
	{
		printf("[%d]%s Error %d\n", __LINE__, __func__, stack->top);
		return -1;
	}

	DBG(LEVEL2, "oper:%d, top:%d", oper->type, stack->top);
	stack->data[stack->top] = *oper;

	return 0;
}

inline int pop_operand(struct im_operand_stack *stack, struct im_operand *oper)
{
	if (stack->top < 0 || stack->top >= IM_STACK_MAX_SIZE)
	{
		printf("[%d]%s Error %d\n", __LINE__, __func__, stack->top);
		return -1;
	}

	*oper = stack->data[stack->top];
	stack->top--;
	DBG(LEVEL2, "oper:%d, top:%d", oper->type, stack->top);

	return 0;
}

void init_op_stack(struct im_op_stack *stack)
{
	stack->top = -1;
}

int op_stack_empty(struct im_op_stack *stack)
{
	DBG(LEVEL2, "%s top:%d", stack->top == -1 ? "true" : "false", stack->top);
	return stack->top == -1;
}

inline int push_op(struct im_op_stack *stack, enum im_op_type op)
{
	stack->top++;

	if (stack->top < 0 || stack->top >= IM_STACK_MAX_SIZE)
	{
		printf("[%d]%s Error %d\n", __LINE__, __func__, stack->top);
		return -1;
	}
	stack->data[stack->top] = op;
	//DBG(LEVEL2, "op:%d %s", op, im_op_attrs[search_imops_by_type(op)].op);

	return 0;
}

inline enum im_op_type pop_op(struct im_op_stack *stack)
{
	enum im_op_type op;

	if (stack->top < 0 || stack->top >= IM_STACK_MAX_SIZE)
	{
		printf("[%d]%s Error %d\n", __LINE__, __func__, stack->top);
		return -1;
	}

	op = stack->data[stack->top];
	stack->top--;
	//DBG(LEVEL2, "op:%d %s", op, im_op_attrs[search_imops_by_type(op)].op);

	return op;
}

inline enum im_op_type top_op(struct im_op_stack *stack)
{
	enum im_op_type op;

	if (stack->top < 0 || stack->top >= IM_STACK_MAX_SIZE)
	{
		printf("[%d]%s Error %d\n", __LINE__, __func__, stack->top);
		return -1;
	}

	op = stack->data[stack->top];
	//DBG(LEVEL2, "op:%d %s", op, im_op_attrs[search_imops_by_type(op)].op);

	return op;
}

/*
* node: basic_info, ...
*/
static int sensor_config_init(xmlNodePtr node, struct sensor_config *config)
{
	int ret = 0;
	xmlNodePtr p = node;
	struct sensor_parser *parser = &sensor_parser;
	int i;

	memset(config, 0, sizeof(struct sensor_config));
	config->actions = (struct lowlevel_action *)&config->actions;

	/*set default*/
	config->i2c_bus = INVALID_I2C_BUS;
	for (i = 0; i < MAX_I2C_ADDRS; i++)
		config->i2c_addrs[i] = INVALID_I2C_ADDR;
	config->test_reg_addr = SENSOR_INVALID_REG;
	config->id_reg_addr = SENSOR_INVALID_REG;
	config->default_poll_interval = SENSOR_INVALID_INTERVAL;
	config->min_poll_interval = SENSOR_INVALID_INTERVAL;
	config->max_poll_interval = SENSOR_INVALID_INTERVAL;
	config->shared_nums = 1;

	sensor_parser_init(parser, config);

	for (; p; p = p->next)
	{
		DBG(LEVEL2, "%s", p->name);

		if (!xmlStrcmp(p->name, (const xmlChar *)"basic_info")) {
			ret = sensor_config_init_basic(parser, p->xmlChildrenNode);
			if (ret) {
				printf("Fail to init basic, ret:%d\n", ret);
				ret = -1;
				goto err;
			}
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"odr_tables")) {
			ret = sensor_config_init_odr(parser, p->xmlChildrenNode);
			if (ret) {
				printf("Fail to init odr, ret:%d\n", ret);
				ret = -1;
				goto err;
			}
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"range_tables")) {
			ret = sensor_config_init_range(parser, p->xmlChildrenNode);
			if (ret) {
				printf("Fail to init range, ret:%d\n", ret);
				ret = -1;
				goto err;
			}
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"sys_entries")) {
			ret = sensor_config_init_sys(parser, p->xmlChildrenNode);
			if (ret) {
				printf("Fail to init sys, ret:%d\n", ret);
				ret = -1;
				goto err;
			}
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"sensor_actions")) {
			ret = sensor_config_init_actions(parser, p->xmlChildrenNode);
			if (ret) {
				printf("Fail to init action, ret:%d\n", ret);
				ret = -1;
				goto err;
			}
		}
		else
		{
			printf("Error:%s %s ret:%d\n", __func__, p->name, ret);
			ret = -1;
			goto err;
		}
	}

	/*init others*/
	config->sensor_regs = (parser->sensor_regs + 1 + 31)/32*32;
	config->id_reg_flag = parser->id_reg_flag;
	config->test_reg_addr = parser->test_reg_addr;

	config->size = sizeof(struct sensor_config) - sizeof(int *)
		+ parser->ll_action_num * sizeof(struct lowlevel_action);

	ret = sensor_config_verify(config);
	if (ret) {
		printf("Fail to init config, ret:%d\n", ret);
		ret = -1;
		goto err;
	}

	ret = 0;
err:
	return ret;
}

static int sensor_config_verify(struct sensor_config *config)
{
	int min, max;

	if (config->test_reg_addr == SENSOR_INVALID_REG) {
		/*if more than one i2c addr*/
		if (config->i2c_addrs[1] != INVALID_I2C_ADDR) {
			printf("must specify byte r/w for test\n");
			goto err;
		}
	}

	if (strlen(config->name) == 0) {
		printf("must specify driver name\n");
		goto err;
	}

	if (strlen(config->input_name) == 0) {
		printf("must specify input name\n");
		goto err;
	}

	min = config->min_poll_interval;
	max = config->max_poll_interval;
	if ((unsigned int)min != SENSOR_INVALID_INTERVAL &&
		(unsigned int)max != SENSOR_INVALID_INTERVAL && min > max) {
		printf("wrong min max poll interval %d %d\n", min, max);
		goto err;
	}

	if (config->method != INT && !config->default_poll_interval) {
		printf("wrong default poll interval\n");
		goto err;
	}

	return 0;
err:
	return -1;
}

/*
* node: i2c_bus, ...
*/
static int sensor_config_init_basic(struct sensor_parser *parser, xmlNodePtr node)
{
	int ret;
	xmlChar *str = NULL;
	xmlNodePtr p = node;
	struct sensor_config *config = parser->config;
	int val;

	DBG(LEVEL2, "%s", p->name);

	for (; p; p = p->next)
	{
		str = xmlNodeGetContent(p);
		if (str == NULL || (!xmlStrcmp(str, (const xmlChar *)""))) {
			continue;
		}

		DBG(LEVEL2,"%s", str);

		if (!xmlStrcmp(p->name, (const xmlChar *)"i2c_bus_num")) {
			val = strtoul((const char*)str, NULL, 0);
			config->i2c_bus = val;
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"i2c_addrs")) {
			xmlNodePtr idNode = p->xmlChildrenNode;
			int i;

			for (i = 0; idNode; idNode = idNode->next)
			{
				char *addr;

				addr = (char *)xmlNodeGetContent(idNode);
				if (addr) {
					config->i2c_addrs[i++] =
						strtoul((const char *)addr, NULL, 0);
					xmlFree(addr);
				}
			}
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"id_reg_addr")) {
			val = strtoul((const char*)str, NULL, 0);
			config->id_reg_addr = val;
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"ids")) {
			xmlNodePtr idNode = p->xmlChildrenNode;
			int i;

			for (i = 0; idNode; idNode = idNode->next)
			{
				char *id;

				id = (char *)xmlNodeGetContent(idNode);
				if (id) {
					config->id[i++] = strtoul((const char *)id, NULL, 0);
					xmlFree(id);
				}
			}
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"driver_name")) {
			strncpy((char *)config->name,
				(const char *)str, MAX_DEV_NAME_BYTES);
			/*truncate if too long*/
			config->name[MAX_DEV_NAME_BYTES -1] = '\0';
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"input_name")) {
			strncpy((char *)config->input_name,
				(const char *)str, MAX_DEV_NAME_BYTES);
			/*truncate if too long*/
			config->input_name[MAX_DEV_NAME_BYTES -1] = '\0';
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"event_type")) {
			char *key;
			if ((key="EV_ABS") && !strncmp((const char *)str, key, strlen(key)))
				config->event_type = 3;
			else if ((key="EV_REL") && !strncmp((const char *)str, key, strlen(key)))
				config->event_type = 2;
			else {
				printf("Invalid Event Type%s\n", str);
				ret = -1;
				goto err;
			}
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"method")) {
			char *key;
			if ((key="interrupt") && !strncmp((const char *)str, key, strlen(key)))
				config->method = 0;
			else if ((key="polling") && !strncmp((const char *)str, key, strlen(key)))
				config->method = 1;
			else if ((key="mix") && !strncmp((const char *)str, key, strlen(key)))
				config->method = 2;
			else {
				printf("Invalid method%s\n", str);
				ret = -1;
				goto err;
			}
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"default_poll_interval")) {
			val = strtoul((const char*)str, NULL, 0);
			if (val == 0) {
				printf("Invalid %s %s\n", p->name, str);
				ret = -1;
				goto err;
			}
			config->default_poll_interval = val;
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"min_poll_interval")) {
			config->min_poll_interval = strtoul((const char *)str, NULL, 0);
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"max_poll_interval")) {
			config->max_poll_interval = strtoul((const char *)str, NULL, 0);
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"gpio_num")) {
			config->gpio_num = strtoul((const char *)str, NULL, 0);
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"report_cnt")) {
			config->report_cnt = strtoul((const char *)str, NULL, 0);
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"report_interval")) {
			config->report_interval = strtoul((const char *)str, NULL, 0);
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"irq_flag")) {
			config->irq_flag = strtoul((const char *)str, NULL, 0);
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"irq_serialize")) {
			if (*str == 'Y' || *str == 'y')
				config->irq_serialize = 1;
			else if (*str == 'N' || *str == 'n')
				config->irq_serialize = 0;
			else {
				printf("Error value of irq_serialize %s\n", str);
				ret = -1;
				goto err;
			}
		}
		else
		{
			printf("[%d]%s Non supported node %s\n",
					__LINE__, __func__, p->name);
			ret = -1;
			goto err;
		}
	}

	ret = 0;
err:
	xmlFree(str);
	return ret;
}

static void sensor_odr_bubble_sort(struct odr *odr, int n)
{
	int i, j, flag;
	struct odr tmp_odr;
	struct odr *tmp = &tmp_odr;

	for (i = 0; i < n; i++) {
		flag = 1;

		for (j = 0; j < n - i - 1; j++) {
			if (odr[j].hz > odr[j+1].hz) {
				*tmp = odr[j];
				odr[j] = odr[j+1];
				odr[j+1] = *tmp;

				flag = 0;
			}
		}

		if (flag)
			break;
	}
}

/*
* node: odr_table
*/
static int sensor_config_init_odr(struct sensor_parser *parser, xmlNodePtr node)
{
	int ret;
	xmlChar *str = NULL;
	xmlNodePtr p = node;
	struct sensor_config *config = parser->config;
	struct odr *odr = config->odr_table;

	DBG(LEVEL1, "%s", p->name);

	for (; p; p = p->next)
	{
		xmlNodePtr pChild = p->xmlChildrenNode;
		int valid_hz = 0;
		int valid_action = 0;

		for (; pChild; pChild = pChild->next)
		{
			str = xmlNodeGetContent(pChild);
			if (str == NULL || (!xmlStrcmp(str, (const xmlChar *)""))) {
				DBG(LEVEL2,"Warning: %s\n", pChild->name);
				continue;
			}

			DBG(LEVEL2,"%s", str);

			if (!xmlStrcmp(pChild->name, (const xmlChar *)"hz")) {
				odr->hz = strtoul((const char *)str, NULL, 0);
				valid_hz = 1;
			}
			else if (!xmlStrcmp(pChild->name, (const xmlChar *)"action")) {
				parser_odr_action(parser, (char *)str, odr);
				valid_action = 1;
			}
			else
			{
				printf("[%d]%s Non supported node %s\n",
						__LINE__, __func__, pChild->name);
				ret = -1;
				goto err;
			}

			xmlFree(str);
		}

		if (valid_hz && valid_action) {
			config->odr_entries++;
			odr++;
		} else if (valid_hz != valid_action)
			printf("Warning: Non complete odr entry\n");

		if (config->odr_entries > MAX_ODR_SETTING_ENTRIES) {
			printf("[%d]%s exceed max odr entry %s\n",
					__LINE__, __func__, p->name);
			ret = -1;
			goto err;
		}
	}

	sensor_odr_bubble_sort(config->odr_table, config->odr_entries);
	ret = 0;
err:
	return ret;
}

/*
* node: range_table
*/
static int sensor_config_init_range(struct sensor_parser *parser, xmlNodePtr node)
{
	int ret;
	xmlChar *str = NULL;
	xmlNodePtr p = node;
	struct sensor_config *config = parser->config;
	struct range_setting *range = config->range_table;

	for (; p; p = p->next)
	{
		xmlNodePtr pChild = p->xmlChildrenNode;
		int valid_range = 0;
		int valid_action = 0;

		for (; pChild; pChild = pChild->next)
		{
			str = xmlNodeGetContent(pChild);
			if (str == NULL || (!xmlStrcmp(str, (const xmlChar *)""))) {
				DBG(LEVEL2,"Warning: %s", pChild->name);
				continue;
			}

			DBG(LEVEL2,"%s", str);

			if (!xmlStrcmp(pChild->name, (const xmlChar *)"range")) {
				char *isdefault = (char *)xmlGetProp(p, (const xmlChar*)"default");

				range->range = strtoul((const char *)str, NULL, 0);

				if (isdefault && (*isdefault == 'Y' || *isdefault == 'y')) {
					config->default_range = range->range;
					xmlFree(isdefault);
				}
				valid_range = 1;
			}
			else if (!xmlStrcmp(pChild->name, (const xmlChar *)"action")) {
				parser_range_action(parser, (char *)str, range);
				valid_action = 1;
			}
			else
			{
				printf("[%d]%s Non supported node %s\n",
						__LINE__, __func__, pChild->name);
				xmlFree(str);
				ret = -1;
				goto err;
			}
			xmlFree(str);
		}

		if (valid_range && valid_action) {
			config->range_entries++;
			range++;
		} else if (valid_range != valid_action)
			printf("Warning: Non complete range entry\n");

		if (config->range_entries > MAX_RANGES) {
			printf("[%d]%s exceed max range entry %s\n",
					__LINE__, __func__, p->name);
			ret = -1;
			goto err;
		}
	}

	ret = 0;
err:
	return ret;
}

/*
* node: sys_entry
*/
static int sensor_config_init_sys(struct sensor_parser *parser, xmlNodePtr node)
{
	int ret;
	xmlChar *str = NULL;
	xmlNodePtr p = node;
	struct sensor_config *config = parser->config;
	struct sysfs_entry *sys = config->sysfs_table;
	int isrange = 0;

	DBG(LEVEL1, "%s", p->name);

	for (; p; p = p->next)
	{
		int valid_name = 0;
		int valid_show = 0;
		int valid_store = 0;
		xmlNodePtr pChild = p->xmlChildrenNode;

		for (; pChild; pChild = pChild->next)
		{
			str = xmlNodeGetContent(pChild);
			if (str == NULL || (!xmlStrcmp(str, (const xmlChar *)""))) {
				DBG(LEVEL2,"Warning: %s", pChild->name);
				continue;
			}

			DBG(LEVEL2,"%s", str);

			if (!xmlStrcmp(pChild->name, (const xmlChar *)"name")) {
				strncpy((char *)sys->name,
					(const char *)str, MAX_ATTR_NAME_BYTES);
				valid_name = 1;

				if (!strcmp((const char *)str, "range")) {
					sys->type = SENSOR_ACTION;
					sys->action.sensor.show = GET_RANGE;
					sys->action.sensor.store = SET_RANGE;

					isrange = 1;
					break;
				}
			}
			else if (!xmlStrcmp(pChild->name, (const xmlChar *)"mode")) {
				sys->mode = strtoul((const char *)str, NULL, 0);
			}
			else if (!xmlStrcmp(pChild->name, (const xmlChar *)"show_action")) {
				parser_sys_action(parser, (char *)str, sys, 1);
				valid_show = 1;
			}
			else if (!xmlStrcmp(pChild->name, (const xmlChar *)"store_action")) {
				parser_sys_action(parser, (char *)str, sys, 0);
				valid_store = 1;
			}
			else
			{
				printf("[%d]%s Non supported node %s\n",
						__LINE__, __func__, pChild->name);
				ret = -1;
				goto err;
			}
		}
		sys->mode = 0666;

		if (valid_name) {
			sys++;
			config->sysfs_entries++;
			if (!valid_show && ! valid_store && !isrange)
				printf("Warning: no show and store action %s\n", p->name);
		}

		if (config->sysfs_entries > MAX_SYSFS_ENTRIES) {
			printf("[%d]%s exceed max sysfs entry %s\n",
					__LINE__, __func__, p->name);
			ret = -1;
			goto err;
		}
	}

	ret = 0;
err:
	return ret;
}

/*
* node: init, ...
*/
static int sensor_config_init_actions(struct sensor_parser *parser, xmlNodePtr node)
{
	int ret = 0;
	xmlChar *str = NULL;
	xmlNodePtr p = node;

	for (; p; p = p->next)
	{
		str = xmlNodeGetContent(p);
		if (str == NULL || (!xmlStrcmp(str, (const xmlChar *)""))) {
			DBG(LEVEL2,"Warning: %s", p->name);
			continue;
		}

		DBG(LEVEL1, "%s", str);

		if (!xmlStrcmp(p->name, (const xmlChar *)"init")) {
			ret = parser_sensor_action(parser, (char *)str, INIT);
			if (ret) {
				printf("[%d]%s Error when parser init\n",
						__LINE__, __func__);
				goto err;
			}
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"deinit")) {
			ret = parser_sensor_action(parser, (char *)str, DEINIT);
			if (ret) {
				printf("[%d]%s Error when parser deinit\n",
						__LINE__, __func__);
				goto err;
			}
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"enable")) {
			ret = parser_sensor_action(parser, (char *)str, ENABLE);
			if (ret) {
				printf("[%d]%s Error when parser enable\n",
						__LINE__, __func__);
				goto err;
			}
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"disable")) {
			ret = parser_sensor_action(parser, (char *)str, DISABLE);
			if (ret) {
				printf("[%d]%s Error when parser disable\n",
						__LINE__, __func__);
				goto err;
			}
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"int_ack")) {
			ret = parser_sensor_action(parser, (char *)str, INT_ACK);
			if (ret) {
				printf("[%d]%s Error when parser int_ack\n",
						__LINE__, __func__);
				goto err;
			}
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"get_data_x")) {
			ret = parser_sensor_action(parser, (char *)str, GET_DATA_X);
			if (ret) {
				printf("[%d]%s Error when parser get_data_x\n",
						__LINE__, __func__);
				goto err;
			}
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"get_data_y")) {
			ret = parser_sensor_action(parser, (char *)str, GET_DATA_Y);
			if (ret) {
				printf("[%d]%s Error when parser get_data_y\n",
						__LINE__, __func__);
				goto err;
			}
		}
		else if (!xmlStrcmp(p->name, (const xmlChar *)"get_data_z")) {
			ret = parser_sensor_action(parser, (char *)str, GET_DATA_Z);
			if (ret) {
				printf("[%d]%s Error when parser get_data_z\n",
						__LINE__, __func__);
				goto err;
			}
		}
		else
		{
			printf("[%d]%s Non supported node %s\n",
					__LINE__, __func__, p->name);
			goto err;
		}

	}

	return 0;
err:
	return ret;
}

static int sensor_parser_prep(struct sensor_parser *parser)
{
	init_operand_stack(&parser->oper_stack);
	init_op_stack(&parser->op_stack);

	memset(parser->high_actions[0], 0, IM_ACTION_MAX_NUM * CHARS_PER_LINE);
	memset(parser->im_actions, 0,
		IM_ACTION_MAX_NUM * sizeof(struct im_action));
	memset(parser->ll_actions, 0,
		IM_ACTION_MAX_NUM * sizeof(struct lowlevel_action));

	parser->high_num = 0;
	parser->im_num = 0;
	parser->ll_num = 0;

	parser->im_top = 0;
	parser->ll_top = 0;

	return 0;
}

static int parser_high2ll_actions(struct sensor_parser *parser, char *high_actions)
{
	int ret = 0;

	DBG(LEVEL4, "%s", high_actions);

	sensor_parser_prep(parser);

	ret = sensor_action_str_format(parser, high_actions);
	if (ret) {
		printf("[%d]%s Error when sensor_action_str_format\n",
					__LINE__, __func__);
		goto err;
	}

	ret = sensor_high2im_actions(parser);
	if (ret) {
		printf("[%d]%s Error when sensor_high2im_actions\n",
					__LINE__, __func__);
		goto err;
	}

#ifdef SENSOR_PARSER_DBG
	if (dbg_level >= LEVEL1)
		dump_im_actions(parser);
#endif

	ret = sensor_optimize_im_actions(parser);
	if (ret) {
		printf("[%d]%s Error when sensor_optimize_im_actions\n",
					__LINE__, __func__);
		goto err;
	}

	/*dump_im_actions(parser);*/

	ret = sensor_im2ll_actions(parser);
	if (ret) {
		printf("[%d]%s Error when sensor_im2ll_actions\n",
					__LINE__, __func__);
		goto err;
	}

	if ((parser->ll_action_num + parser->ll_num) >= MAX_LL_ACTION_NUM) {
		printf("Error, exceeds max action num\n");
		ret = -1;
		goto err;
	}

#ifdef SENSOR_PARSER_DBG
	if (dbg_level >= LEVEL1)
		dump_ll_actions(parser);
#endif

	return 0;
err:
	return -1;
}

static int parser_odr_action(struct sensor_parser *parser,
				char *high_actions, struct odr *odr)
{
	int ret = 0;
	char *dst;
	struct lowlevel_action *action_table =
			(struct lowlevel_action *)&parser->config->actions;

	DBG(LEVEL4, "%s", high_actions);

	ret = parser_high2ll_actions(parser, high_actions);
	if (ret) {
		printf("[%d]%s Error when high2ll actions\n",
					__LINE__, __func__);
		goto err;
	}

	DBG(LEVEL2, "%d, %d", parser->ll_action_num, parser->ll_num);

	odr->index.index = parser->ll_action_num;
	odr->index.num = parser->ll_num;
	dst = (char *)&action_table[odr->index.index];
	if (parser->ll_num)
		memcpy(dst, parser->ll_actions,
			parser->ll_num * sizeof(struct lowlevel_action));

	parser->ll_action_num += parser->ll_num;

	return 0;
err:
	return -1;
}

static int parser_range_action(struct sensor_parser *parser,
			char *high_actions, struct range_setting *range)
{
	int ret = 0;
	char *dst;
	struct lowlevel_action *action_table =
			(struct lowlevel_action *)&parser->config->actions;

	DBG(LEVEL4, "%s", high_actions);

	ret = parser_high2ll_actions(parser, high_actions);
	if (ret) {
		printf("[%d]%s Error when high2ll actions\n",
					__LINE__, __func__);
		goto err;
	}

	DBG(LEVEL2, "%d, %d", parser->ll_action_num, parser->ll_num);

	range->index.index = parser->ll_action_num;
	range->index.num = parser->ll_num;
	dst = (char *)&action_table[range->index.index];
	if (parser->ll_num)
		memcpy(dst, parser->ll_actions,
			parser->ll_num * sizeof(struct lowlevel_action));

	parser->ll_action_num += parser->ll_num;

	return 0;
err:
	return -1;
}

static int parser_sys_action(struct sensor_parser *parser,
			char *high_actions, struct sysfs_entry *sys, int isshow)
{
	int ret = 0;
	struct lowlevel_action_index *index;
	char *dst;
	struct lowlevel_action *action_table =
			(struct lowlevel_action *)&parser->config->actions;

	DBG(LEVEL4, "%s", high_actions);

	ret = parser_high2ll_actions(parser, high_actions);
	if (ret) {
		printf("[%d]%s Error when high2ll actions\n",
					__LINE__, __func__);
		goto err;
	}

	DBG(LEVEL2, "%d, %d", parser->ll_action_num, parser->ll_num);
	sys->type = DATA_ACTION;

	if (isshow)
		index = &sys->action.data.index_show;
	else
		index = &sys->action.data.index_store;

	index->index = parser->ll_action_num;
	index->num = parser->ll_num;
	dst = (char *)&action_table[index->index];
	if (parser->ll_num)
		memcpy(dst, parser->ll_actions,
			parser->ll_num * sizeof(struct lowlevel_action));

	parser->ll_action_num += parser->ll_num;

	return 0;
err:
	return -1;
}

static int parser_sensor_action(struct sensor_parser *parser,
				char *high_actions, enum sensor_action ll_type)
{
	int ret = 0;
	char *dst;
	struct lowlevel_action_index *index = &parser->config->indexs[ll_type];
	struct lowlevel_action *action_table =
			(struct lowlevel_action *)&parser->config->actions;

	DBG(LEVEL4, "%s", high_actions);

	ret = parser_high2ll_actions(parser, high_actions);
	if (ret) {
		printf("[%d]%s Error when high2ll actions\n",
				__LINE__, __func__);
		goto err;
	}

	DBG(LEVEL2, "%d, %d", parser->ll_action_num, parser->ll_num);

	index->index = parser->ll_action_num;
	index->num = parser->ll_num;

	dst = (char *)&action_table[parser->ll_action_num];
	if (parser->ll_num) {
		memcpy(dst, parser->ll_actions,
			parser->ll_num * sizeof(struct lowlevel_action));
	}

	parser->ll_action_num += parser->ll_num;

	return 0;
err:
	return -1;
}

/*remove comment, space, tab, \r, \n
* replace ; with \n
* add \n behind if()/else/endif
* copy formated str to parser->high_actions
*/
static int sensor_action_str_format(struct sensor_parser *parser, char *input)
{
	int len = strlen(input);
	int out_inx = 0, in_inx = 0;
	int comment = 0;
	int isif = 0, leftBraces = 0, rightBraces = 0;
	int ret = 0;
	int line_start = 0;
	int i;
	char *buf;

	DBG(LEVEL4, "Raw str:%s", input);

	buf = malloc(IM_ACTION_MAX_NUM * CHARS_PER_LINE);
	if (!buf) {
		printf("Fail to alloc in str format\n");
		return -1;
	}

	for (; in_inx < len ; in_inx++)
	{
		char c;

		if (comment) {
			if (input[in_inx] == '*' && input[in_inx + 1] == '/') {
				comment = 0;
				in_inx += 1;
			}
			continue;
		}

		if (input[in_inx] == '/' && input[in_inx + 1] == '*') {
			comment = 1;
			in_inx += 1;
			continue;
		}

		c = input[in_inx];
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
			continue;

		if (c == ';') {
			c = '\0';
			buf[out_inx++] = c;

			DBG(LEVEL4, "input %d %s--", parser->high_num, &buf[line_start]);
			strcpy(parser->high_actions[parser->high_num++], &buf[line_start]);
			line_start = out_inx;

			continue;
		}

		if (!isif && !strncmp(&input[in_inx], "if", 2)) {
			isif = 1;
			leftBraces = 0;
			rightBraces = 0;
			buf[out_inx++] = 'i';
			in_inx++;
			buf[out_inx++] = 'f';
			continue;
		}

		if (isif) {
			if (c == '(') {
				leftBraces++;
				buf[out_inx++] = c;
				continue;
			}
			else if (c  == ')') {
				rightBraces++;

				if (leftBraces == rightBraces) {
					buf[out_inx++] = c;
					buf[out_inx++] = '\0';

					DBG(LEVEL4, "input %s", &input[line_start]);
					strcpy(parser->high_actions[parser->high_num++],
								&buf[line_start]);
					line_start = out_inx;

					isif = 0;
					continue;
				}
			}
		}

		if (!strncmp(&input[in_inx], "else", 4)) {
			memcpy(&buf[out_inx], &input[in_inx], 4);
			in_inx += 3;
			out_inx += 4;
			buf[out_inx++] = '\0';

			DBG(LEVEL4, "input %s", &input[line_start]);
			strcpy(parser->high_actions[parser->high_num++], &buf[line_start]);
			line_start = out_inx;

			continue;
		}

		if (!strncmp(&input[in_inx], "endif", 5)) {
			memcpy(&buf[out_inx], &input[in_inx], 5);
			in_inx += 4;
			out_inx += 5;
			buf[out_inx++] = '\0';

			DBG(LEVEL4, "input %s", &input[line_start]);
			strcpy(parser->high_actions[parser->high_num++], &buf[line_start]);
			line_start = out_inx;

			continue;
		}

		buf[out_inx++] = c;
	}

	if (comment) {
		printf("Error: unbalanced comment %s\n", input);
		ret = -1;
		goto err;
	}

	DBG(LEVEL1, "Formated highactions:%d", parser->high_num);
	for (i = 0; i < parser->high_num; i++)
	{
		DBG(LEVEL1, "\t%s", parser->high_actions[i]);
	}
	DBG(LEVEL1, "\n");

err:
	free(buf);
	return ret;
}

static int sensor_high2im_actions(struct sensor_parser *parser)
{
	int ret;
	int i;
	char *high_action;
	int match = 0;

	for (i = 0; i < parser->high_num; i++)
	{
		high_action = parser->high_actions[i];

		DBG(LEVEL2, "\t\t\t%s", high_action);

		ret = sensor_high2im_single(parser, high_action, &match);
		if (ret) {
			printf("Fail to translate single %s\n", high_action);
			goto err;
		}
		else if (match)
			continue;

		ret = sensor_high2im_multi(parser, high_action, &match);
		if (ret) {
			printf("Fail to translate multi %s %d\n",
						high_action, match);
			goto err;
		}
		else if (match)
			continue;

		printf("Non support experssion %s\n", high_action);
		ret = -1;
		goto err;
	}

	parser->im_num = parser->im_top;

	return 0;
err:
	return ret;
}

/*esle/endif, readreg, sleep*/
static int sensor_high2im_single(struct sensor_parser *parser,
		char *high_action, int *match)
{
	int ret = 0;
	char *key_str = NULL;
	struct im_action *action = &parser->im_actions[parser->im_top];

	*match = 1;
	if (!strcmp(high_action, "else")) {
		action->type = IM_ELSE;
		parser->im_top++;
		DBG(LEVEL1, "else im action: %s", high_action);
		return ret;
	}

	if (!strcmp(high_action, "endif")) {
		action->type = IM_ENDIF;
		parser->im_top++;
		DBG(LEVEL1, "endif im action %s", high_action);
		return ret;
	}

	if ((key_str = "readreg_") &&
		!strncmp(high_action, key_str, strlen(key_str))) {
		char *end_ptr = NULL;
		int addr, flag, reglen;

		action->type = IM_ASSIGN;
		addr = strtol(high_action + strlen(key_str), &end_ptr, 0);
		flag = strtol(end_ptr + 1, &end_ptr, 0);
		reglen = strtol(end_ptr + 1, &end_ptr, 0);

		action->operand1.type = IM_REGBUF;
		action->operand1.data.reg.addr = addr;
		action->operand1.data.reg.len = reglen;
		action->operand1.data.reg.flag = flag;

		action->operand2.type = IM_REG;
		action->operand2.data.reg.addr = addr;
		action->operand2.data.reg.len = reglen;
		action->operand2.data.reg.flag = flag;

		parser->im_top++;

		DBG(LEVEL1, "readreg im action: %s, %x %x %x",
					high_action, addr, reglen, flag);

		if (addr > parser->sensor_regs)
			parser->sensor_regs = addr;

		if (reglen == 1) {
			if (!parser->id_reg_flag && flag)
				parser->id_reg_flag = flag;
			if (parser->test_reg_addr == SENSOR_INVALID_REG)
				parser->test_reg_addr = addr | flag;
		}

		if (*end_ptr) {
			printf("[%d]%s Error when parser %s %s\n",
				__LINE__, __func__, key_str, high_action);
			goto err;
		}

		return ret;
	}

	if ((key_str = "sleep_") &&
		!strncmp(high_action, key_str, strlen(key_str))) {
		char *end_ptr = NULL;

		action->type = IM_SLEEP;
		action->operand1.data.ms =
			strtol(high_action + strlen(key_str), &end_ptr, 0);
		parser->im_top++;

		DBG(LEVEL1, "sleep im action: %s, %d",
				high_action, action->operand1.data.ms);

		if (*end_ptr) {
			printf("[%d]%s Error when parser %s %s\n",
					__LINE__, __func__, key_str, high_action);
			goto err;
		}

		return ret;
	}

	/*no im action here*/
	*match = 0;
	return 0;
err:
	return -1;
}

static int sensor_high2im_multi(struct sensor_parser *parser,
				char *high_action, int *match)
{
	int ret = 0;
	int i;
	int len;
	struct im_action *action = &parser->im_actions[parser->im_top];
	int isif = 0;
	int isreturn = 0;
	int handle_len;

	len = strlen(high_action);
	*match = 1;

	DBG(LEVEL2, "%d %s", len, high_action);

	for (i = 0; i < len; i += handle_len)
	{
		char *key_str = NULL;

		DBG(LEVEL4, "%d %s", i, &high_action[i]);

		if (i == 0 && (key_str = "if") &&
			!strncmp(high_action, key_str, strlen(key_str))) {
			DBG(LEVEL1, "if im action: %s ims:%d",
					high_action, parser->im_top);

			action->type = IM_IF;
			parser->im_top++;

			isif = 1;
			handle_len = strlen(key_str);
			continue;
		}

		if (i == 0 && (key_str = "return") &&
			!strncmp(high_action, key_str, strlen(key_str))) {
			char *end_ptr;
			struct im_operand operand;
			enum im_op_type op = IM_ARI_ADD;

			end_ptr = high_action + strlen(key_str);
			ret = sensor_high2im_operand(parser, end_ptr, &handle_len);
			if (handle_len > 0)
				pop_operand(&parser->oper_stack, &operand);

			end_ptr += handle_len;
			if (*end_ptr == '\0') {
				operand.type = IM_IMM;
				operand.data.immediate = 0;
				push_operand(&parser->oper_stack, &operand);
				push_op(&parser->op_stack, op);
			}

			/*assume no assign in return experssion
			so no additional push statck lowlevel action*/

			DBG(LEVEL2, "return im action: %s", high_action);
			handle_len = strlen(key_str);
			isreturn = 1;
			continue;
		}

		ret = sensor_high2im_operand(parser, &high_action[i], &handle_len);
		if (ret) {
			printf("[%d]%s Error when parser %s\n",
						__LINE__, __func__, high_action);
			goto err;
		}
		else if (handle_len)
			continue;

		ret = sensor_high2im_op(parser, &high_action[i], &handle_len);
		if (ret) {
			printf("[%d]%s Error when parser %s\n",
						__LINE__, __func__, high_action);
			goto err;
		}
		else if (handle_len)
			continue;

		*match = 0;
		printf("Non support keyword:%s\n", &high_action[i]);
		break;
	}

	ret = sensor_tran_last_im_op(parser);
	if (ret) {
		printf("[%d]%s Error parser last im\n", __LINE__, __func__);
		goto err;
	}

	if (isif) {
		DBG(LEVEL1, "if start im action: %s ims:%d",
				high_action, parser->im_top);
		action = &parser->im_actions[parser->im_top];
		action->type = IM_IF_START;
		parser->im_top++;
	} else if (isreturn) {
		DBG(LEVEL1, "return im action: %s ims:%d",
				high_action, parser->im_top);
		action = &parser->im_actions[parser->im_top];
		action->type = IM_RETURN;
		parser->im_top++;
	}

	return 0;
err:
	return -1;
}

static int sensor_high2im_operand(struct sensor_parser *parser, char *str, int *len)
{
	int ret = 0;
	struct im_operand operand;
	char *key_str = NULL;
	char *end_ptr = NULL;

	DBG(LEVEL4, "%s", str);
	*len = 0;

	if ((key_str = "global_") && !strncmp(str, key_str, strlen(key_str))) {
		operand.type = IM_GLOBAL;
		operand.data.index = strtol(str + strlen(key_str), &end_ptr, 0);
		operand.data.index += PRIVATE_MAX_SIZE/2;
		push_operand(&parser->oper_stack, &operand);

		if (operand.data.index >= PRIVATE_MAX_SIZE) {
			printf("Error,only support up to %d global variable %s\n",
					PRIVATE_MAX_SIZE/2, str);
			return -1;
		}
		DBG(LEVEL2, "operand glboal: %s %x", str, operand.data.index);

		*len = (int)(end_ptr - str);
		return ret;
	}

	if ((key_str = "local_") && !strncmp(str, key_str, strlen(key_str))) {
		operand.type = IM_LOCAL;
		operand.data.index = strtol(str + strlen(key_str), &end_ptr, 0);
		push_operand(&parser->oper_stack, &operand);

		if (operand.data.index >= PRIVATE_MAX_SIZE/2) {
			printf("Error,only support up to %d local variable %s\n",
						PRIVATE_MAX_SIZE/2, str);
			return -1;
		}
		DBG(LEVEL2, "operand local:%s %x", str, operand.data.index);

		*len = (int)(end_ptr - str);
		return ret;
	}

	if ((key_str = "regbuf_") && !strncmp(str, key_str, strlen(key_str))) {
		operand.type = IM_REGBUF;
		operand.data.reg.addr = strtol(str + strlen(key_str), &end_ptr, 0);
		operand.data.reg.len = strtol(end_ptr + 1, &end_ptr, 0);
		push_operand(&parser->oper_stack, &operand);

		DBG(LEVEL2, "operand regbuf: %s %x %x",
				str, operand.data.reg.addr, operand.data.reg.len);
		if (operand.data.reg.addr > parser->sensor_regs)
			parser->sensor_regs = operand.data.reg.addr;

		(*len) = (int)(end_ptr - str);
		return ret;
	}

	if ((key_str = "input") && !strncmp(str, key_str, strlen(key_str))) {
		operand.type = IM_BEFORE;
		push_operand(&parser->oper_stack, &operand);

		DBG(LEVEL2, "operand input: %s", str);

		*len = strlen(key_str);
		return ret;
	}

	if ((key_str = "writereg_") && !strncmp(str, key_str, strlen(key_str))) {
		int addr, flag, reglen;

		DBG(LEVEL4, "%s", str + strlen(key_str));
		addr = strtol(str + strlen(key_str), &end_ptr, 0);

		DBG(LEVEL4, "%s", end_ptr + 1);
		flag = strtol(end_ptr + 1, &end_ptr, 0);

		DBG(LEVEL4, "%s", end_ptr + 1);
		reglen = strtol(end_ptr + 1, &end_ptr, 0);

		if (reglen > 2) {
			printf("Error,only support 1Byte and"
					"2Byte i2c register write %s\n", str);
			return -1;
		}

		operand.type = IM_REG;
		operand.data.reg.addr = addr;
		operand.data.reg.flag = flag;
		operand.data.reg.len = reglen;
		push_operand(&parser->oper_stack, &operand);

		if (reglen == 1) {
			if (!parser->id_reg_flag && flag)
				parser->id_reg_flag = flag;

			if (parser->test_reg_addr == SENSOR_INVALID_REG)
				parser->test_reg_addr = addr | flag;
		}

		if (addr > parser->sensor_regs)
			parser->sensor_regs = addr;

		DBG(LEVEL2, "operand writereg: %s, %s, %x %x %x",
					str, end_ptr, addr, flag, reglen);
		*len = (int)(end_ptr - str);
		return ret;
	}

	/*immediate*/
	if (*str == '(') {
		int imm = strtol(str + 1, &end_ptr, 0);
		if (*end_ptr == ')') {

			operand.type = IM_IMM;
			operand.data.immediate = imm;
			push_operand(&parser->oper_stack, &operand);

			DBG(LEVEL2, "operand imm: %s %d", str, operand.data.immediate);

			*len = (int)(end_ptr - str) + 1;
			return ret;
		}

	} else {
		int imm = strtol(str, &end_ptr, 0);
		if (end_ptr > str) {
			/*minus and plus should placed in ()*/
			if (str[0] == '-' || str[0] == '+')
				return ret;
			operand.type = IM_IMM;
			operand.data.immediate = imm;
			push_operand(&parser->oper_stack, &operand);

			DBG(LEVEL2, "operand imm: %s %x", str, operand.data.immediate);

			*len = (int)(end_ptr - str);
			return ret;
		}
	}

	DBG(LEVEL4, "Not operands %s\n", str);
	return ret;
}

static int sensor_high2im_op(struct sensor_parser *parser, char *str, int *len)
{
	int ret = 0;
	int op_inx;
	int handle_len;
	struct im_op_attr *attr;
	char *oper_str;

	if ((op_inx = search_imops_by_op(str)) >= 0) {

		DBG(LEVEL2, "%s %d", str, op_inx);

		attr = &im_op_attrs[op_inx];

		/*min,max,endian, translate now, no need to check priority*/
		if (attr->op_size >= 3) {
			oper_str = str + attr->op_size + 1/*(*/;
			ret = sensor_high2im_operand(parser, oper_str, &handle_len);
			if (attr->op_num == 2) {
				oper_str += (handle_len + 1/*,*/);
				sensor_high2im_operand(parser, oper_str, &handle_len);
			}

			push_op(&parser->op_stack, attr->type);

			DBG(LEVEL2, "com op: %s", str);

			*len = (oper_str + handle_len + 1/*)*/) - str;
			return ret;
		}

		DBG(LEVEL2, "%s %s", str, attr->op);
		if (attr->type == IM_LEFT_BRACE ||
			op_stack_empty(&parser->op_stack) ||
			top_op(&parser->op_stack) == IM_LEFT_BRACE ||
			imops_priority_type(top_op(&parser->op_stack)) < attr->priority) {

			push_op(&parser->op_stack, attr->type);

			DBG(LEVEL2, "push op: %s, %s", str, attr->op);

			*len = attr->op_size;
			return ret;

		} else {
			/*translate now until priority(op) > priority(stack top op)*/
			ret = sensor_tran_im_ops(parser, attr->type);
			if (ret) {
				printf("[%d]%s Error\n", __LINE__, __func__);
				goto err;
			}

			if (attr->type != IM_RIGHT_BRACE)
				push_op(&parser->op_stack, attr->type);

			DBG(LEVEL2, "%s %s", str, attr->op);

			*len = attr->op_size;
			return ret;
		}
	}

	DBG(LEVEL4, "Not op %s\n", str);
	*len = 0;
	return 0;
err:
	return ret;

}

static int sensor_tran_last_im_op(struct sensor_parser *parser)
{
	int ret = 0;
	struct im_action *action = &parser->im_actions[parser->im_top];
	enum im_op_type stack_op;
	int op_inx;
	struct im_operand operand;
	struct im_operand operand2;
	struct im_op_attr *attr;
	int op_empty;
	int finish_last;

	op_empty = op_stack_empty(&parser->op_stack);
	finish_last = op_empty;
	while (!op_empty) {
		stack_op = pop_op(&parser->op_stack);

		action->type = stack_op;
		if ((op_inx = search_imops_by_type(stack_op)) < 0) {
			printf("[%d]%s Error\n", __LINE__, __func__);
			ret = -1;
			goto err;
		}
		DBG(LEVEL2, "Trans op: %d", stack_op);

		pop_operand(&parser->oper_stack, &operand);

		attr = &im_op_attrs[op_inx];
		if (attr->op_num == 1)
			action->operand1 = operand;
		else if (attr->op_num == 2) {
			action->operand2 = operand;

			pop_operand(&parser->oper_stack, &operand2);

			action->operand1 = operand2;
		} else {
			printf("[%d]%s Error\n", __LINE__, __func__);
			ret = -1;
			goto err;
		}

		op_empty = op_stack_empty(&parser->op_stack);

		if (!op_empty) {
			if ((attr->op_num == 2 &&
				operand.type == IM_IMM && operand2.type == IM_IMM) ||
				(attr->op_num == 1 && operand.type == IM_IMM)) {
				int imm;

				ret = sensor_parser_preprocess(attr->type,
					operand2.data.immediate, operand.data.immediate, &imm);
				if (ret) {
					printf("[%d]%s Error\n", __LINE__, __func__);
					ret = -1;
					goto err;
				}
				operand.type = IM_IMM;
				operand.data.immediate = imm;
				push_operand(&parser->oper_stack, &operand);

				parser->im_top--;

			} else {
				operand.type = IM_BEFORE;
				push_operand(&parser->oper_stack, &operand);

				action++;
			}
		}

		parser->im_top++;
	}

	if (finish_last)
		pop_operand(&parser->oper_stack, &operand);

	if (!operand_stack_empty(&parser->oper_stack)) {
		printf("Error: why operand are left\n");
		ret = -1;
		goto err;
	}

	return 0;
err:
	return ret;
}

static int sensor_tran_im_ops(struct sensor_parser *parser, enum im_op_type type)
{
	int ret = 0;
	struct im_action *action = &parser->im_actions[parser->im_top];
	enum im_op_type stack_op;
	int op_inx;
	struct im_operand operand;
	struct im_operand operand2;
	struct im_op_attr *attr;

	stack_op = top_op(&parser->op_stack);
	while (imops_priority_type(stack_op) >= imops_priority_type(type)) {

		if (stack_op == IM_LEFT_BRACE) {
			if (type == IM_RIGHT_BRACE)
				pop_op(&parser->op_stack);

			return ret;
		}

		stack_op = pop_op(&parser->op_stack);

		if ((op_inx = search_imops_by_type(stack_op)) < 0) {
			printf("[%d]%s Error\n", __LINE__, __func__);
			ret = -1;
			goto err;
		}

		attr = &im_op_attrs[op_inx];
		action->type = attr->type;

		pop_operand(&parser->oper_stack, &operand);

		DBG(LEVEL3, "trans op: %d", operand.type);

		if (attr->op_num == 1)
			action->operand1 = operand;
		else if (attr->op_num == 2) {
			action->operand2 = operand;

			pop_operand(&parser->oper_stack, &operand2);

			DBG(LEVEL3, "%d", operand2.type);

			action->operand1 = operand2;
		} else {
			printf("[%d]%s Error\n", __LINE__, __func__);
			ret = -1;
			goto err;
		}

		if ((attr->op_num == 2 &&
				operand.type == IM_IMM && operand2.type == IM_IMM)  ||
				(attr->op_num == 1 && operand.type == IM_IMM)) {
			int imm;

			ret = sensor_parser_preprocess(attr->type,
				operand2.data.immediate, operand.data.immediate, &imm);
			if (ret) {
				printf("[%d]%s Error\n", __LINE__, __func__);
				ret = -1;
				goto err;
			}
			operand.type = IM_IMM;
			operand.data.immediate = imm;
			push_operand(&parser->oper_stack, &operand);

		} else {
			action++;
			parser->im_top++;

			operand.type = IM_BEFORE;
			push_operand(&parser->oper_stack, &operand);
		}

		/*next loop*/
		if (op_stack_empty(&parser->op_stack))
			break;
		else
			stack_op = top_op(&parser->op_stack);
	}

	return 0;
err:
	return ret;
}

static int sensor_parser_preprocess(enum im_op_type type, int op1, int op2, int *output)
{
	int ret = 0;

	switch (type) {
	case IM_LOGIC_EQ:
		ret = (op1 == op2);
		break;
	case IM_LOGIC_NEQ:
		ret = (op1 != op2);
		break;
	case IM_LOGIC_GREATER:
		ret = (op1 > op2);
		break;
	case IM_LOGIC_LESS:
		ret = (op1 < op2);
		break;
	case IM_LOGIC_GE:
		ret = (op1 >= op2);
		break;
	case IM_LOGIC_LE:
		ret = (op1 <= op2);
		break;

	case IM_LOGIC_AND:
		ret = (op1 && op2);
		break;
	case IM_LOGIC_OR:
		ret = (op1 || op2);
		break;

	case IM_ARI_ADD:
		ret = (op1 + op2);
		break;
	case IM_ARI_SUB:
		ret = (op1 - op2);
		break;
	case IM_ARI_MUL:
		ret = (op1 * op2);
		break;
	case IM_ARI_DIV:
		ret = (op1 / op2);
		break;
	case IM_ARI_MOD:
		ret = (op1 % op2);
		break;

	case IM_BIT_OR:
		ret = (op1 | op2);
		break;
	case IM_BIT_AND:
		ret = (op1 & op2);
		break;
	case IM_BIT_LSL:
		ret = (op1 << op2);
		break;
	case IM_BIT_LSR:
		ret = (op1 >> op2);
		break;
	case IM_BIT_NOR:
		ret = (~op2);
		break;

	default:
		printf("[%d]%s Error im tpye:%d\n", __LINE__, __func__, type);
		ret = -1;
		goto err;
	}

	*output = ret;
	ret = 0;
err:
	return ret;
}

/* check if local variable is referenced, if yes, can't optimize
*  action: start action to check
*  num: how many action to check
*  index: index of local variable
*  return: return 1 if can optimize
*/
static int sensor_can_optimize_local(struct im_action *action, int num, int index)
{
	for (; num > 0; num--, action++)
	{
		if (action->operand1.type == IM_LOCAL
			&& action->operand1.data.index == index) {
			if (action->type == IM_ASSIGN)
				return 1;
			else
				return 0;
		}

		if (action->type != IM_BIT_NOR/*single operenad*/
			&& action->operand2.type == IM_LOCAL
			&& action->operand2.data.index == index)
			return 0;
	}
	return 1;
}

static int sensor_optimize_im_actions(struct sensor_parser *parser)
{
	int ret = 0;
	struct im_action *action = parser->im_actions;
	char *buf;
	struct im_action *opt_action = NULL;
	int num;
	int optimized = 0;

	buf = malloc(sizeof(struct im_action) * parser->im_num);
	if (!buf) {
		printf("[%d]%s Error\n", __LINE__, __func__);
		ret = -1;
		goto err;
	}
	opt_action = (struct im_action *)buf;

	for (num = 0; num < parser->im_num - 1; num++, action++, opt_action++)
	{
		struct im_action *next_action = action + 1;

		if (action->type == IM_ASSIGN && action->operand1.type == IM_LOCAL) {
			int index = action->operand1.data.index;
			int inx1 = -1, inx2 = -1;

			/*check if this local is referenced in two operands of next_action*/
			if (next_action->operand1.type == IM_LOCAL)
				inx1 = next_action->operand1.data.index == index ? index: inx1;

			if (next_action->type != IM_BIT_NOR/*single operenad*/
					&& next_action->operand2.type == IM_LOCAL)
				inx2 = next_action->operand2.data.index == index ? index: inx2;

			/*only match one then can optimize*/
			if (inx1 != inx2 && sensor_can_optimize_local(next_action + 1,
				parser->im_num - 2 - num, index)) {

				*opt_action = *next_action;
				if (inx1 == index)
					(*opt_action).operand1 = action->operand2;
				else
					(*opt_action).operand2 = action->operand2;

				optimized++;
				action++;
				continue;
			}
		}

		*opt_action = *action;
	}

	if (optimized) {
		memcpy(parser->im_actions, buf, parser->im_num * sizeof(struct im_action));
		parser->im_num -= optimized;
		DBG(LEVEL1, "optimized %d\n", optimized);
	}

	ret = 0;
err:
	if (buf)
		free(buf);
	return ret;
}

static int sensor_im2ll_actions(struct sensor_parser *parser)
{
	struct im_action *im_actions = parser->im_actions;
	int i;
	int ret = 0;
	int inx_if[SENSOR_MAX_IFS] = { 0 };
	int inx_top = -1;

	for (i = 0; i < parser->im_num; i++, im_actions++)
	{

		if (im_actions->type == IM_IF) {
			if (inx_top >= -1)
				inx_if[++inx_top] = parser->ll_top;
			else {
				printf("[%d]%s Error\n", __LINE__, __func__);
				ret = -1;
				goto err;
			}

			ret = sensor_trans_ll(parser, im_actions);
			if (ret) {
				printf("[%d]%s Error\n", __LINE__, __func__);
				ret = -1;
				goto err;
			}

		} else if (im_actions->type == IM_IF_START) {
			struct ifelse_action *ifelse =
				&parser->ll_actions[inx_if[inx_top]].action.ifelse;

			ifelse->num_con = parser->ll_top - inx_if[inx_top] - 1;

		} else if (im_actions->type == IM_ELSE) {
			struct ifelse_action *ifelse =
				&parser->ll_actions[inx_if[inx_top]].action.ifelse;

			ifelse->num_if = parser->ll_top -
					inx_if[inx_top] - ifelse->num_con - 1;

		} else if (im_actions->type == IM_ENDIF) {
			struct ifelse_action *ifelse =
				&parser->ll_actions[inx_if[inx_top]].action.ifelse;

			if (ifelse->num_if == NON_INITIAL_IFELSE) {
				ifelse->num_if = parser->ll_top -
					inx_if[inx_top] - ifelse->num_con - 1;
				ifelse->num_else = 0;
			} else {
				ifelse->num_else = parser->ll_top -
				inx_if[inx_top] - ifelse->num_con - ifelse->num_if - 1;
			}
			DBG(LEVEL1, "%d %d %d %d", inx_top, ifelse->num_con,
						ifelse->num_if, ifelse->num_else);

			inx_top--;

		} else {

			ret = sensor_trans_ll(parser, im_actions);
			if (ret) {
				printf("[%d]%s Error\n", __LINE__, __func__);
				ret = -1;
				goto err;
			}
		}
	}

	parser->ll_num = parser->ll_top;

err:
	return ret;

}

static int sensor_trans_ll(struct sensor_parser *parser,
			struct im_action *im_action)
{
	int ret = 0;
	int index;
	struct lowlevel_action *ll_action = &parser->ll_actions[parser->ll_top];

	switch (im_action->type) {
	case IM_IF:
		DBG(LEVEL2, "if");

		ll_action->type = IFELSE;
		ll_action->action.ifelse.num_con = NON_INITIAL_IFELSE;
		ll_action->action.ifelse.num_if = NON_INITIAL_IFELSE;
		ll_action->action.ifelse.num_else = NON_INITIAL_IFELSE;

		parser->ll_top++;
		break;

	case IM_IF_START/*start of if true*/:
	case IM_ELSE:
	case IM_ENDIF:
		break;

	case IM_RETURN:
		DBG(LEVEL2, "return");
		ll_action->type = RETURN;

		parser->ll_top++;
		break;

	case IM_SLEEP:
		DBG(LEVEL2, "sleep");

		ll_action->type = SLEEP;
		ll_action->action.sleep.ms = im_action->operand1.data.ms;

		parser->ll_top++;
		break;

	case IM_ASSIGN/*=*/:

	case IM_LOGIC_EQ:
	case IM_LOGIC_NEQ:
	case IM_LOGIC_GREATER:
	case IM_LOGIC_LESS:
	case IM_LOGIC_GE:
	case IM_LOGIC_LE:

	case IM_LOGIC_AND:
	case IM_LOGIC_OR:

	case IM_ARI_ADD:
	case IM_ARI_SUB:
	case IM_ARI_MUL:
	case IM_ARI_DIV:
	case IM_ARI_MOD:

	case IM_BIT_OR:
	case IM_BIT_AND:
	case IM_BIT_LSL:
	case IM_BIT_LSR:
	case IM_BIT_NOR:

	case IM_ENDIAN_BE16:
	case IM_ENDIAN_BE16_UN:
	case IM_ENDIAN_BE24:
	case IM_ENDIAN_BE32:
	case IM_ENDIAN_LE16:
	case IM_ENDIAN_LE16_UN:
	case IM_ENDIAN_LE24:
	case IM_ENDIAN_LE32:

	case IM_MIN:
	case IM_MAX:
		ll_action->type = DATA;

		index = search_imops_by_type(im_action->type);
		if (index < 0) {
			printf("[%d]%s Error\n", __LINE__, __func__);
			ret = -1;
			goto err;
		}
		DBG(LEVEL2, "data %d", index);

		ll_action->action.data.op = im_op_attrs[index].ll_type;

		if (im_op_attrs[index].op_num == 1)
			trans_ll_oper(&im_action->operand1,
				&ll_action->action.data.operand1);
		else if (im_op_attrs[index].op_num == 2) {
			 trans_ll_oper(&im_action->operand1,
				&ll_action->action.data.operand1);
			 trans_ll_oper(&im_action->operand2,
				&ll_action->action.data.operand2);
		} else {
			printf("[%d]%s Error\n", __LINE__, __func__);
			ret = -1;
			goto err;
		}

		parser->ll_top++;
		break;

	case IM_LEFT_BRACE:
	case IM_RIGHT_BRACE:

	default:
		printf("[%d]%s Error im tpye:%d\n",
				__LINE__, __func__, im_action->type);
		ret = -1;
		goto err;
	}
err:
	return ret;
}

static int trans_ll_oper(struct im_operand *im_oper, struct operand *ll_oper)
{
	int ret = 0;

	switch (im_oper->type) {
	case IM_GLOBAL:
	case IM_LOCAL:
		ll_oper->type = OPT_INDEX;
		ll_oper->data.index = im_oper->data.index;
		break;

	case IM_REGBUF:
		ll_oper->type = OPT_REG_BUF;
		ll_oper->data.reg.addr = im_oper->data.reg.addr;
		ll_oper->data.reg.len = im_oper->data.reg.len;
		break;

	case IM_IMM:
		ll_oper->type = OPT_IMM;
		ll_oper->data.immediate = im_oper->data.immediate;
		break;

	case IM_BEFORE:
		ll_oper->type = OPT_BEFORE;
		break;

	case IM_REG:
		ll_oper->type = OPT_REG;
		ll_oper->data.reg.addr = im_oper->data.reg.addr;
		ll_oper->data.reg.flag = im_oper->data.reg.flag;
		ll_oper->data.reg.len = im_oper->data.reg.len;
		break;

	default:
		printf("[%d]%s Error\n", __LINE__, __func__);
		ret = -1;
		goto err;
	}

err:
	return ret;
}

static inline void dump_tabs(int line, int level)
{
	printf("%4d", line);

	while (level-- > 0)
		printf("\t");
}

static void dump_im_actions(struct sensor_parser *parser)
{
	struct im_action *actions = parser->im_actions;
	struct im_operand *oper;
	int i;
	int level = 0;

	printf("Translated to %d intermediate actions\n", parser->im_num);

	for (i = 0; i < parser->im_num; i++, actions++)
	{
		if (actions->type == IM_SLEEP) {
			dump_tabs(i, level);
			printf("im actoin: sleep ");
			oper = &actions->operand1;
			printf("\t %d\n", oper->data.ms);
			continue;
		}

		if (actions->type == IM_IF) {
			dump_tabs(i, level);
			printf("%s\n", "if");
			level++;
			continue;
		}
		else if (actions->type == IM_IF_START) {
			dump_tabs(i, level - 1);
			printf("%s\n", "if_start");
			continue;
		}
		else if (actions->type == IM_ELSE) {
			dump_tabs(i, level - 1);
			printf("%s\n", "else");
			continue;
		}
		else if (actions->type == IM_ENDIF) {
			dump_tabs(i, level - 1);
			printf("%s\n", "endif");
			level--;
			continue;
		}
		else if (actions->type == IM_RETURN) {
			dump_tabs(i, level - 1);
			printf("%s\n", "return");
			level--;
			continue;
		}

		dump_tabs(i, level);

#if 0 //KW check
		if (search_imops_by_type(actions->type) != -1) {
			printf("im actoin: %s ",
				im_op_attrs[search_imops_by_type(actions->type)].op);
		}
#else
		printf("im actoin type: %d ", actions->type);
#endif

		oper = &actions->operand1;
		if (oper->type == IM_GLOBAL || oper->type == IM_LOCAL) {
			printf("\t%s_%02d ",
				oper->type == IM_GLOBAL ? "global": "local",
				oper->data.index);
		}
		else if (oper->type == IM_REG || oper->type == IM_REGBUF) {
			printf("\t%s_%02x_%02x_%02x",
				oper->type == IM_REG ? "reg": "regbuf",
				oper->data.reg.addr, oper->data.reg.flag,
				oper->data.reg.len);
		}
		else if (oper->type == IM_IMM) {
			printf("\t%s_%08x", "immediate",oper->data.immediate);
		}
		else if (oper->type == IM_BEFORE) {
			printf("\tbefore\t");
		}

#if 0 //KW check
		if (im_op_attrs[search_imops_by_type(actions->type)].op_num == 1) {
			printf("\n");
			continue;
		}
#endif

		oper = &actions->operand2;
		if (oper->type == IM_GLOBAL || oper->type == IM_LOCAL) {
			printf("\t%s_%d\n", oper->type == IM_GLOBAL ? "global": "local",
						oper->data.index);
		}
		else if (oper->type == IM_REG || oper->type == IM_REGBUF) {
			printf("\t%s_%x_%x_%x\n",
				oper->type == IM_REG ? "reg": "regbuf",
				oper->data.reg.addr, oper->data.reg.flag,
				oper->data.reg.len);
		}
		else if (oper->type == IM_IMM) {
			printf("\t%s_%x\n", "immediate",oper->data.immediate);
		}
		else if (oper->type == IM_BEFORE) {
			printf("\tbefore\n");
		}

	}
}

static char *action_debug[] = {
	"OP_ACCESS        ",
	"OP_MIN           ", "OP_MAX           ",
	"OP_LOGIC_EQ      ", "OP_LOGIC_NEQ     ", "OP_LOGIC_GREATER ",
	"OP_LOGIC_LESS    ",
	"OP_LOGIC_GE      ", "OP_LOGIC_LE      ", "OP_LOGIC_AND     ",
	"OP_LOGIC_OR      ",
	"OP_ARI_ADD       ", "OP_ARI_SUB       ", "OP_ARI_MUL       ",
	"OP_ARI_DIV       ", "OP_ARI_MOD       ",
	"OP_BIT_OR        ", "OP_BIT_AND       ", "OP_BIT_LSL       ",
	"OP_BIT_LSR       ", "OP_BIT_NOR       ",
	"OP_ENDIAN_BE16   ", "OP_ENDIAN_BE16_UN", "OP_ENDIAN_BE24   ",
	"OP_ENDIAN_BE32   ", "OP_ENDIAN_LE16   ", "OP_ENDIAN_LE16_UN",
	"OP_ENDIAN_LE24   ", "OP_ENDIAN_LE32   ",
	"OP_COMMA_EXPR    ",
	"OP_RESERVE",
};

static void dump_ll_action(struct lowlevel_action *actions, int num, int level)
{
	int i;

	DBG(LEVEL3, "%p %d %d\n", actions, num, level);

	for (i = 0; i < num; i++, actions++)
	{
		if (actions->type == IFELSE) {
			int num_con, num_if, num_else;

			num_con = actions->action.ifelse.num_con;
			num_if = actions->action.ifelse.num_if;
			num_else = actions->action.ifelse.num_else;

			dump_tabs(i, level);
			printf(" if level:%d (%d):%d:%d\n",
					level, num_con, num_if, num_else);
			dump_ll_action(actions + 1, num_con, level + 1);

			dump_tabs(i, level);
			printf(" ifstart level:%d\n", level);
			dump_ll_action(actions + 1 + num_con, num_if, level + 1);

			dump_tabs(i, level);
			printf(" else level:%d\n", level);
			dump_ll_action(actions + 1 + num_con + num_if,
							num_else, level + 1);

			dump_tabs(i, level);
			printf(" endif level:%d\n", level);

			i += (num_con + num_if + num_else);
			actions += (num_con + num_if + num_else);

		} else  {
			struct operand *oper;

			if (actions->type == SLEEP) {
				dump_tabs(i, level);
				printf(" sleep");
				printf("\t%d\n", actions->action.sleep.ms);
				continue;
			} else if (actions->type == RETURN) {
				dump_tabs(i, level);
				printf(" return\n");
				continue;
			} else if (actions->type != DATA) {
				printf("Error wrong lowlevel actions %d\n",
							actions->type);
			}

			dump_tabs(i, level);
			printf(" data action: %s\t",
					action_debug[actions->action.data.op]);

			oper = &actions->action.data.operand1;
			if (oper->type == OPT_IMM) {
				printf("\timmediate_%08x", oper->data.immediate);
			} else if (oper->type == OPT_REG) {
				printf("\treg_%02x_%02x_%02x",
					oper->data.reg.addr,
					oper->data.reg.flag,
					oper->data.reg.len);
			} else if (oper->type == OPT_REG_BUF) {
				printf("\tregbuf_%02x_%02x_%02x",
					oper->data.reg.addr,
					oper->data.reg.flag,
					oper->data.reg.len);
			} else if (oper->type == OPT_INDEX) {
				printf("\tindex_%02d    ", oper->data.index);
			} else if (oper->type == OPT_BEFORE) {
				printf("\tbefore\t");
			} else
				printf("Error wrong lowlevel operand %d\n", oper->type);

			if (actions->action.data.op >= OP_BIT_NOR &&
					actions->action.data.op <= OP_ENDIAN_LE32) {
				printf("\n");
				continue;
			}

			oper = &actions->action.data.operand2;
			if (oper->type == OPT_IMM) {
				printf("\timmediate_%x\n", oper->data.immediate);
			} else if (oper->type == OPT_REG) {
				printf("\treg_%x_%x_%x\n", oper->data.reg.addr,
					oper->data.reg.flag, oper->data.reg.len);
			} else if (oper->type == OPT_REG_BUF) {
				printf("\tregbuf_%x_%x_%x\n", oper->data.reg.addr,
					oper->data.reg.flag, oper->data.reg.len);
			} else if (oper->type == OPT_INDEX) {
				printf("\tindex_%d\n", oper->data.index);
			} else if (oper->type == OPT_BEFORE) {
				printf("\tbefore\n");
			} else
				printf("Error wrong lowlevel operand %d\n", oper->type);
		}
	}
}

static void dump_ll_actions(struct sensor_parser *parser)
{
	printf("Translated to %d lowlevel actions\n", parser->ll_num);

	dump_ll_action(parser->ll_actions, parser->ll_num, 0);
}
